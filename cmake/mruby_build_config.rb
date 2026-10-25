# mruby build configuration used by the engine.
# C++ exceptions are enabled so Ruby errors unwind C++ frames safely.

# What the engine uses of mruby, for every system it is built for.
# A game's scripts are code someone else may have written, so the Ruby they run in can compute,
# and reach the engine, and nothing else. Left out on purpose: files, directories, sockets and
# processes (the stdlib-io gembox: File, IO, Dir, sockets, backticks, IO.popen), and running
# text as code (mruby-eval, mruby-binding). A script has no way to touch the player's files or
# start a program.
def redgem_gems(conf)
  conf.gembox 'stdlib'
  conf.gembox 'stdlib-ext'
  conf.gembox 'math'
  conf.gem core: 'mruby-metaprog'   # __send__, instance_variable_set, const_get: the prelude uses them
  conf.gem core: 'mruby-compiler'   # the engine compiles the scripts itself
  conf.gem core: 'mruby-error'
  conf.enable_cxx_exception
  # The hook the interpreter calls before each instruction: how a script that never returns is
  # stopped (src/script/script_budget.hpp). The engine is compiled with the same definition
  # (cmake/Dependencies.cmake), as it changes what mrb_state holds.
  conf.compilers.each { |compiler| compiler.defines << 'MRB_USE_DEBUG_HOOK' }
end

# For the machine doing the build (a Windows build still needs it: it compiles mruby's own tools).
MRuby::Build.new do |conf|
  conf.toolchain :gcc
  redgem_gems(conf)
  conf.cc.flags << '-fPIC'
  conf.cxx.flags << '-fPIC'
end

# For Windows, cross-compiled with llvm-mingw when the engine's CMake build asks for it.
if ENV['REDGEM_MINGW_BIN']
  MRuby::CrossBuild.new('windows') do |conf|
    tools = ENV['REDGEM_MINGW_BIN']
    conf.toolchain :gcc
    conf.host_target = 'x86_64-w64-mingw32'
    conf.cc.command = "#{tools}/x86_64-w64-mingw32-clang"
    conf.cxx.command = "#{tools}/x86_64-w64-mingw32-clang++"
    conf.linker.command = "#{tools}/x86_64-w64-mingw32-clang++"
    conf.archiver.command = "#{tools}/llvm-ar"
    conf.exts.executable = '.exe'
    redgem_gems(conf)
  end
end

# For Android, cross-compiled with the NDK's clang for one kind of processor at a time.
if ENV['REDGEM_NDK_BIN']
  MRuby::CrossBuild.new("android-#{ENV['REDGEM_ANDROID_ABI']}") do |conf|
    tools = ENV['REDGEM_NDK_BIN']
    target = "--target=#{ENV['REDGEM_ANDROID_TARGET']}"
    conf.toolchain :clang
    conf.host_target = ENV['REDGEM_ANDROID_TARGET']
    conf.cc.command = "#{tools}/clang"
    conf.cxx.command = "#{tools}/clang++"
    conf.linker.command = "#{tools}/clang++"
    conf.archiver.command = "#{tools}/llvm-ar"
    [conf.cc, conf.cxx].each { |compiler| compiler.flags << target << '-fPIC' }
    conf.linker.flags << target
    redgem_gems(conf)
  end
end
