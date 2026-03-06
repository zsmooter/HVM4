fn void heap_set(u64 loc, Term val) {
  if (__builtin_expect(THREAD_COUNT == 1, 1)) {
    HEAP[loc] = val;
    return;
  }
  __atomic_store_n(&HEAP[loc], val, __ATOMIC_RELAXED);
}
