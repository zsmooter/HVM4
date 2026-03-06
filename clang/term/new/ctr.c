fn Term term_new_ctr_at(u64 loc, u32 nam, u32 ari, Term *args) {
  if (ari == 0) {
    return term_new(0, C00, nam, 0);
  }
  if (ari == 1) {
    heap_set(loc + 0, args[0]);
    return term_new(0, C01, nam, loc);
  }
  return term_new_at(loc, C00 + ari, nam, ari, args);
}

fn Term term_new_ctr(u32 nam, u32 ari, Term *args) {
  if (ari == 0) {
    return term_new(0, C00, nam, 0);
  }
  return term_new_ctr_at(heap_alloc(ari), nam, ari, args);
}
