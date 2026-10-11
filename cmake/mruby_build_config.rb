# mruby build configuration used by the engine.
# C++ exceptions are enabled so Ruby errors unwind C++ frames safely.

# What the engine uses of mruby, for every system it is built for.
def redgem_gems(conf)
  conf.gembox 'stdlib'
  conf.gembox 'stdlib-ext'
  conf.gembox 'stdlib-io'
  conf.gembox 'math'
  conf.gembox 'metaprog'
  conf.gem core: 'mruby-compiler'
  conf.gem core: 'mruby-error'
  conf.enable_cxx_exception
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
