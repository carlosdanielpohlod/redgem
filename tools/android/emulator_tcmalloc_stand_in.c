// An empty stand-in for the Android emulator's bundled tcmalloc, for a computer whose C library is
// glibc 2.39 (Ubuntu 24.04): there the bundled emulator/lib64/libtcmalloc_minimal.so.4 calls itself
// without end while glibc makes room for a new thread's storage, and the emulator crashes as soon
// as a game draws (see KNOWN_LIMITATIONS.md, Shipping). With this one, memory comes from glibc.
// qemu calls only these two functions of tcmalloc's own; they accept the hook and never use it.
//
//   lib=$ANDROID_SDK_ROOT/emulator/lib64/libtcmalloc_minimal.so.4
//   mv "$lib" "$lib.bak"
//   cc -shared -fPIC -Wl,-soname,libtcmalloc_minimal.so.4 -o "$lib" tools/android/emulator_tcmalloc_stand_in.c
//   ...run the emulator...
//   mv "$lib.bak" "$lib"      (the emulator as it was)

int MallocHook_SetMmapReplacement(void* hook) {
  (void)hook;
  return 1;
}

int MallocHook_SetMunmapReplacement(void* hook) {
  (void)hook;
  return 1;
}
