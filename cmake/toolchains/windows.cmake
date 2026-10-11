# Builds RedGem for 64-bit Windows from Linux with llvm-mingw (tools/windows/fetch_toolchain.sh
# downloads it into .deps/llvm-mingw). Used by tools/windows/build_runtime.sh.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(REDGEM_MINGW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.deps/llvm-mingw" CACHE PATH "Where llvm-mingw is installed")
set(REDGEM_MINGW_TRIPLE x86_64-w64-mingw32)

set(CMAKE_C_COMPILER "${REDGEM_MINGW_ROOT}/bin/${REDGEM_MINGW_TRIPLE}-clang")
set(CMAKE_CXX_COMPILER "${REDGEM_MINGW_ROOT}/bin/${REDGEM_MINGW_TRIPLE}-clang++")
set(CMAKE_RC_COMPILER "${REDGEM_MINGW_ROOT}/bin/${REDGEM_MINGW_TRIPLE}-windres")

# Libraries and headers come from the toolchain, programs (rake, cmake) from this machine.
set(CMAKE_FIND_ROOT_PATH "${REDGEM_MINGW_ROOT}/${REDGEM_MINGW_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# One self-contained .exe: no runtime DLLs to ship next to it.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
