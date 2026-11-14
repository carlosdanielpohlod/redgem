#pragma once

// The types of the API reference (docs/scripting/api.json): what each parameter takes and each
// method returns, written in a small closed language that Ruby's type signatures (RBS) read too:
//   Integer, Float, Numeric, String, Symbol, bool, nil, untyped, self, Proc, Class
//   Color                    a color's name (:orange) or [r, g, b] from 0 to 255
//   Vector3, GameObject...   the API's own classes, by their path (Animation::Graph)
//   Array[T], Hash[K, V], Range[T]
//   [A, B]                   an array of exactly those (a tuple)
//   A | B                    either (A | nil: maybe nothing)
//   ^(A, B) -> R             a block: what it is called with and returns (only a block's type)
//   void                     returned only: nothing to use (a method's or a block's)

#include <string>
#include <vector>

namespace redgem::tools {

// Where a type is written, which decides what it may be.
enum class TypePlace {
  Parameter,  // what a parameter takes
  Block,      // a block's: ^(A) -> R, and nothing else
  Returned,   // what a method returns: may be void
};

// What is wrong with a type, or nothing when it is one of the language's. `classes` are the
// names of the API's classes, which it may use besides the built-in ones.
std::string api_type_problem(const std::string& type, const std::vector<std::string>& classes, TypePlace place);

// The same type as RBS writes it: the same, but for Color (RedGem::color there, see api_rbs.hpp).
std::string rbs_type(const std::string& type);

// ... as RBS writes what a method returns: in parentheses when it is a union (A | B), as `|`
// there also parts a method's overloads.
std::string rbs_returned(const std::string& type);

// A block's type without its mark: "(Float) -> void" for ^(Float) -> void.
std::string block_signature(const std::string& type);

// A block's type (^(Float) -> void) as RBS writes a method's block: { (Float) -> void }.
std::string rbs_block(const std::string& type);

}  // namespace redgem::tools
