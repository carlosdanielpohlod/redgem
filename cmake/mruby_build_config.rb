# mruby build configuration used by the engine.
# C++ exceptions are enabled so Ruby errors unwind C++ frames safely.
MRuby::Build.new do |conf|
  conf.toolchain :gcc

  conf.gembox 'stdlib'
  conf.gembox 'stdlib-ext'
  conf.gembox 'stdlib-io'
  conf.gembox 'math'
  conf.gembox 'metaprog'
  conf.gem core: 'mruby-compiler'
  conf.gem core: 'mruby-error'

  conf.enable_cxx_exception
  conf.cc.flags << '-fPIC'
  conf.cxx.flags << '-fPIC'
end
