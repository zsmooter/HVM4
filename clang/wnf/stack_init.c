fn void wnf_stack_init(void) {
  WnfBank *bank = WNF_BANK;
  if (bank->stack) {
    return;
  }

  u64 bytes = WNF_CAP * sizeof(Term);
  Term *stack      = (Term *)sys_mmap_anon(bytes);
  u8    stack_mmap = 1;

  if (stack == NULL) {
    stack = (Term *)malloc(bytes);
    stack_mmap = 0;
  }

  if (!stack) {
    fprintf(stderr, "wnf: stack allocation failed\n");
    exit(1);
  }

  bank->stack       = stack;
  bank->stack_bytes = bytes;
  bank->stack_mmap  = stack_mmap;
  bank->s_pos       = 1;
}
