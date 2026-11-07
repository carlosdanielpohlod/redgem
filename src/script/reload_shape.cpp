#include "script/reload_shape.hpp"
#include "script/ruby_reload.hpp"
#include "script/ruby_context.hpp"
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
struct Parser {
  RubyContext context;
  std::unique_ptr<mrb_parser_state, decltype(&mrb_parser_free)> parser;
  explicit Parser(mrb_state* state) : context(state), parser(mrb_parser_new(state), mrb_parser_free) {
    if (!parser) throw std::runtime_error("cannot allocate a script parser");
  }
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  Node* parse(const std::string& code) {
    context->capture_errors = true;
    parser->s = code.data();
    parser->send = code.data() + code.size();
    mrb_parser_parse(parser.get(), context.get());
    if (parser->nerr != 0) {
      const auto& problem = parser->error_buffer[0];
      const char* message = problem.message ? problem.message : "syntax error";
      throw std::runtime_error("line " + std::to_string(problem.lineno) + ": " + message);
    }
    return parser->tree;
  }
};
class Shape {
 public:
  Shape(mrb_state* mrb, const std::string& name, const std::string& base) : mrb_(mrb), name_(name), base_(base) {}
  const std::string& problem() const { return problem_; }
  bool file(Node* scope) {
    if (!tagged(scope, NODE_SCOPE)) return reject(scope, "expected a file scope");
    if (!statements(scope->cdr->cdr, [this](Node* node) { return declaration(node); })) return false;
    return classes_ == 1 || reject(scope, "expected one behaviour class");
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
  bool reject(Node* node, const char* reason) {
    const int line = node ? std::max(1, static_cast<int>(node->lineno)) : 1;
    problem_ = "line " + std::to_string(line) + ": " + reason + "; restart Play";
    return false;
  }
  bool take() { return --remaining_ >= 0; }
  bool named(const Node* value, const std::string& name) const { return mrb_sym_name(mrb_, symbol(value)) == name; }
  bool base(Node* node) const {
    const bool named_base = tagged(node, NODE_CONST) || tagged(node, NODE_COLON3);
    if (!node) return true;
    const bool matching_base = named_base && named(node->cdr, base_);
    return matching_base;
  }
  bool declaration(Node* node) {
    if (!tagged(node, NODE_CLASS)) return reject(node, "top-level executable setup");
    if (++classes_ != 1) return reject(node, "multiple class declarations");
    auto* args = node->cdr;
    auto* path = args->car;
    const auto prefix = reinterpret_cast<std::uintptr_t>(path->car);
    const bool matching_class = prefix <= 1 && named(path->cdr, name_);
    if (!matching_class) return reject(node, "class name or namespace changed");
    if (!base(args->cdr->car)) return reject(node, "superclass expression changed");
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
  bool words(Node* items) {
    for (; items; items = items->cdr) {
      if (!take()) return false;
      const bool text = tagged(items->car, NODE_STR) || tagged(items->car, NODE_LITERAL_DELIM);
      if (!text) return false;
    }
    return true;
  }
  bool numeric(Node* node) const { return tagged(node, NODE_INT) || tagged(node, NODE_FLOAT); }
  bool can_validate(Node* node, int depth) {
    if (!node) return false;
    if (depth == 0) return false;
    return take();
  }
  bool heredoc(Node* node) {
    const auto* info = reinterpret_cast<mrb_parser_heredoc_info*>(node->cdr);
    const bool command = (info->type & STR_FUNC_XQUOTE) != 0;
    if (command) return false;
    return words(info->doc);  // interpolation is a non-string AST node, so words refuses it
  }
  bool word_array(Node* node) const { return tagged(node, NODE_WORDS) || tagged(node, NODE_SYMBOLS); }
  bool container_literal(Node* node, int depth) {
    if (tagged(node, NODE_ARRAY)) return sequence(node->cdr, depth - 1, false);
    if (word_array(node)) return words(node->cdr);
    const bool hash = tagged(node, NODE_HASH);
    return hash && sequence(node->cdr, depth - 1, true);
  }
  bool literal(Node* node, int depth = kLiteralDepth) {
    if (!can_validate(node, depth)) return false;
    if (scalar(node)) return true;
    if (tagged(node, NODE_HEREDOC)) return heredoc(node);
    if (tagged(node, NODE_NEGATE)) return numeric(node->cdr);
    return container_literal(node, depth);
  }
  bool constant(Node* node) {
    if (!tagged(node, NODE_ASGN)) return false;
    return tagged(node->cdr->car, NODE_CONST) && literal(node->cdr->cdr);
  }
  bool attribute_names(Node* items, bool visibility) {
    for (; items; items = items->cdr) {
      if (!take()) return false;
      const bool literal_name = tagged(items->car, NODE_SYM) || tagged(items->car, NODE_STR);
      const bool visibility_method = visibility && tagged(items->car, NODE_DEF);
      const bool allowed = literal_name || visibility_method;
      if (!allowed) return false;
    }
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
    const bool extra_arguments = call->cdr->car || call->cdr->cdr;
    if (extra_arguments) return false;
    const bool visibility = method == "private" || method == "protected" || method == "public";
    return attribute_names(call->car, visibility);
  }
  const char* unsupported(Node* node) const {
    if (tagged(node, NODE_CLASS)) return "nested class declaration";
    if (tagged(node, NODE_SDEF)) return "class method declaration";
    if (tagged(node, NODE_ASGN)) return "class assignment or computed constant initializer";
    if (tagged(node, NODE_FCALL)) return "unsupported declaration call";
    return "executable class setup";
  }
  bool definition(Node* node) {
    const bool declaration_only =
        tagged(node, NODE_DEF) || constant(node) || attributes(node) || tagged(node, NODE_NIL);
    if (declaration_only) return true;
    return reject(node, unsupported(node));
  }
  mrb_state* mrb_;
  const std::string& name_;
  const std::string& base_;
  std::string problem_ = kUnsupported;
  int classes_ = 0;
  int remaining_ = kNodeBudget;
};
}  // namespace
void require_reload_shape(mrb_state* mrb, const std::string& code, const std::string& name, const std::string& base) {
  Parser parser(mrb);
  Shape shape(mrb, name, base);
  if (!shape.file(parser.parse(code))) throw std::runtime_error(shape.problem());
}
}  // namespace redgem
