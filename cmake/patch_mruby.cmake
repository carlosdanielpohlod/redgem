# Makes mruby's interpreter read its code fetch hook with an atomic load: the engine's watchdog
# sets the hook from another thread to stop a script that runs past its budget
# (src/script/script_budget.hpp), and a plain read the compiler may keep in a register is not
# certain to ever see it. Included with MRUBY_SOURCE set to mruby's source folder; changes the
# one line, once.
set(REDGEM_MRUBY_VM "${MRUBY_SOURCE}/src/vm.c")
set(REDGEM_HOOK_MARK "redgem: the hook is read atomically")
set(REDGEM_PLAIN_HOOK
    "#define CODE_FETCH_HOOK(mrb, irep, pc, regs) if ((mrb)->code_fetch_hook) (mrb)->code_fetch_hook((mrb), (irep), (pc), (regs));")
set(REDGEM_ATOMIC_HOOK
    "/* ${REDGEM_HOOK_MARK} */\n#define CODE_FETCH_HOOK(mrb, irep, pc, regs) do { __typeof__((mrb)->code_fetch_hook) redgem_hook = __atomic_load_n(&(mrb)->code_fetch_hook, __ATOMIC_RELAXED); if (redgem_hook) redgem_hook((mrb), (irep), (pc), (regs)); } while (0);")

file(READ "${REDGEM_MRUBY_VM}" REDGEM_VM_SOURCE)
string(FIND "${REDGEM_VM_SOURCE}" "${REDGEM_HOOK_MARK}" REDGEM_PATCHED)
if(REDGEM_PATCHED EQUAL -1)
  string(FIND "${REDGEM_VM_SOURCE}" "${REDGEM_PLAIN_HOOK}" REDGEM_HOOK_AT)
  if(REDGEM_HOOK_AT EQUAL -1)
    message(FATAL_ERROR "mruby's code fetch hook is not where cmake/patch_mruby.cmake expects it: check the new version's src/vm.c")
  endif()
  string(REPLACE "${REDGEM_PLAIN_HOOK}" "${REDGEM_ATOMIC_HOOK}" REDGEM_VM_SOURCE "${REDGEM_VM_SOURCE}")
  file(WRITE "${REDGEM_MRUBY_VM}" "${REDGEM_VM_SOURCE}")
endif()
