#pragma once

// The names the packer of Android apps goes by: the tools it runs and the files it lays out for
// them. They are defined once, out of line (android_package_names.cpp), and not as constants a
// compiler sees into: tests/ships_no_tools.sh tells whether a program has the packer in it by
// looking for these very words, and a name folded into the code that uses it is not found whole.
namespace redgem::android_names {

// The SDK's tools, in its newest build-tools folder, and Java's, found by the shell.
extern const char kAaptTool[];
extern const char kAapt2Tool[];
extern const char kZipalignTool[];
extern const char kApksignerTool[];
extern const char kKeytoolTool[];
// What a staged app is made of (see stage_android_game), which the tools are then pointed at.
extern const char kAssetsFolder[];
extern const char kLibrariesFolder[];
extern const char kManifestFile[];
// The engine's own signing key, in the user's RedGem folder.
extern const char kKeystoreFile[];

}  // namespace redgem::android_names
