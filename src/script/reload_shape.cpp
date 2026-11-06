#include "script/reload_shape.hpp"
#include "script/ruby_reload.hpp"
#include <mruby/compile.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <iterator>
#include <stdexcept>
#include "node.h"  // the AST of the pinned mruby compiler; never infer Ruby syntax from text

namespace redgem {
namespace {
using Node = mrb_ast_node;
constexpr int kLiteralDepth = 16;
constexpr int kNodeBudget = 4096;  // bound validation work independently of method bodies
constexpr const char* kUnsupported =
    "live reload needs one behaviour class with methods and literal constants; restart Play for executable setup";
int kind(const Node* node) { return node ? static_cast<int>(reinterpret_cast<std::intptr_t>(node->car)) : -1; }
mrb_sym symbol(const Node* node) { return static_cast<mrb_sym>(reinterpret_cast<std::uintptr_t>(node)); }
bool tagged(const Node* node, int tag) { return kind(node) == tag; }
struct ContextDeleter {
  mrb_state* mrb;
  void operator()(mrbc_context* context) const { mrbc_context_free(mrb, context); }
};
struct Parser {
  std::unique_ptr<mrbc_context, ContextDeleter> context;
  std::unique_ptr<mrb_parser_state, decltype(&mrb_parser_free)> parser;
  explicit Parser(mrb_state* state)
      : context(mrbc_context_new(state), ContextDeleter{state}), parser(mrb_parser_new(state), mrb_parser_free) {
    if (!context || !parser) throw std::runtime_error("cannot allocate a script parser");
  }
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  Node* parse(const std::string& code) {
    context->capture_errors = true;
    parser->s = code.data();
    parser->send = code.data() + code.size();
    mrb_parser_parse(parser.get(), context.get());
    if (parser->nerr != 0) throw std::runtime_error(kUnsupported);
    return parser->tree;
  }
};
class Shape {
 public:
  Shape(mrb_state* mrb, const std::string& name, const std::string& base) : mrb_(mrb), name_(name), base_(base) {}
  bool file(Node* scope) {
    if (!tagged(scope, NODE_SCOPE)) return false;
    return statements(scope->cdr->cdr, [this](Node* node) { return declaration(node); }) && classes_ == 1;
  }

 private:
  template <class Predicate>
  bool statements(Node* node, const Predicate& accepted) {
    if (!node) return true;
    if (!take()) return false;
    if (!tagged(node, NODE_BEGIN)) return accepted(node);
    for (auto* item = node->cdr; item; item = item->cdr)
      if (!statements(item->car, accepted)) return false;
    return true;
  }
  bool take() { return --remaining_ >= 0; }
  bool named(const Node* value, const std::string& name) const { return mrb_sym_name(mrb_, symbol(value)) == name; }
  bool base(Node* node) const { return !node || (tagged(node, NODE_CONST) && named(node->cdr, base_)); }
  bool declaration(Node* node) {
    if (!tagged(node, NODE_CLASS) || ++classes_ != 1) return false;
    auto* args = node->cdr;
    auto* path = args->car;
    const auto prefix = reinterpret_cast<std::uintptr_t>(path->car);
    if (prefix > 1 || !named(path->cdr, name_)) return false;
    if (!base(args->cdr->car)) return false;
    return statements(args->cdr->cdr->car->cdr, [this](Node* member) { return definition(member); });
  }
  bool scalar(Node* node) const {
    constexpr int types[] = {NODE_INT, NODE_FLOAT, NODE_STR, NODE_SYM, NODE_NIL, NODE_TRUE, NODE_FALSE, NODE_ZARRAY};
    return std::find(std::begin(types), std::end(types), kind(node)) != std::end(types);
  }
  bool hash_key(Node* node) const { return tagged(node, NODE_SYM) || tagged(node, NODE_STR); }
  bool pair_literal(Node* pair, int depth) {
    if (!hash_key(pair->car)) return false;
    return literal(pair->car, depth) && literal(pair->cdr, depth);
  }
  bool sequence(Node* items, int depth, bool pairs) {
    for (; items; items = items->cdr) {
      const bool allowed = pairs ? pair_literal(items->car, depth) : literal(items->car, depth);
      if (!allowed) return false;
    }
    return true;
  }
  bool numeric(Node* node) const { return tagged(node, NODE_INT) || tagged(node, NODE_FLOAT); }
  bool literal(Node* node, int depth = kLiteralDepth) {
    if (!node || depth == 0 || !take()) return false;
    if (scalar(node)) return true;
    if (tagged(node, NODE_NEGATE)) return numeric(node->cdr);
    if (tagged(node, NODE_ARRAY)) return sequence(node->cdr, depth - 1, false);
    return tagged(node, NODE_HASH) && sequence(node->cdr, depth - 1, true);
  }
  bool constant(Node* node) {
    if (!tagged(node, NODE_ASGN)) return false;
    return tagged(node->cdr->car, NODE_CONST) && literal(node->cdr->cdr);
  }
  bool attribute_names(Node* items) {
    for (; items; items = items->cdr)
      if (!take() || !tagged(items->car, NODE_SYM)) return false;
    return true;
  }
  bool declaration_helper(const std::string& method) const {
    return std::find(kReloadHookNames.begin() + 1, kReloadHookNames.end(), method) != kReloadHookNames.end();
  }
  bool attributes(Node* node) {
    if (!tagged(node, NODE_FCALL)) return false;
    auto* args = node->cdr;
    const std::string method = mrb_sym_name(mrb_, symbol(args->cdr->car));
    if (!declaration_helper(method)) return false;
    auto* call = args->cdr->cdr->car;
    if (!call) return true;
    if (call->cdr->car || call->cdr->cdr) return false;
    return attribute_names(call->car);
  }
  bool definition(Node* node) {
    return tagged(node, NODE_DEF) || constant(node) || attributes(node) || tagged(node, NODE_NIL);
  }
  mrb_state* mrb_;
  const std::string& name_;
  const std::string& base_;
  int classes_ = 0;
  int remaining_ = kNodeBudget;
};
}  // namespace
void require_reload_shape(mrb_state* mrb, const std::string& code, const std::string& name, const std::string& base) {
  Parser parser(mrb);
  Shape shape(mrb, name, base);
  if (!shape.file(parser.parse(code))) throw std::runtime_error(kUnsupported);
}
}  // namespace redgem
