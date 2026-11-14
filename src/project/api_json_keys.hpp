#pragma once

// The fields of docs/scripting/api.json (see api_reference.hpp), which the tools also write.

namespace redgem::tools::api_keys {

// Where the reference is in the repository, as it was built in.
inline constexpr const char* kReferenceFile = "docs/scripting/api.json";

// The reference's own.
inline constexpr const char* kDescription = "description";  // of the manual's page, for its index
inline constexpr const char* kAbout = "about";
inline constexpr const char* kNotes = "notes";
inline constexpr const char* kInternalClasses = "internal_classes";
inline constexpr const char* kClasses = "classes";
// A class's.
inline constexpr const char* kName = "name";
inline constexpr const char* kKind = "kind";
inline constexpr const char* kSuperclass = "superclass";
inline constexpr const char* kGuide = "guide";
inline constexpr const char* kMethods = "methods";
inline constexpr const char* kInternal = "internal";
// A method's.
inline constexpr const char* kStatic = "static";
inline constexpr const char* kParams = "params";
inline constexpr const char* kReturns = "returns";
inline constexpr const char* kTop = "top";
inline constexpr const char* kExample = "example";
// A parameter's (and "returns" has a type).
inline constexpr const char* kType = "type";
inline constexpr const char* kKeyword = "keyword";
inline constexpr const char* kRest = "rest";
inline constexpr const char* kBlock = "block";
inline constexpr const char* kOptional = "optional";
inline constexpr const char* kDefault = "default";
// What a class's kind is.
inline constexpr const char* kClassKind = "class";
inline constexpr const char* kModuleKind = "module";

}  // namespace redgem::tools::api_keys
