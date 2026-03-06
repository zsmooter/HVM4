// heap_set with release ordering, for writing locations that may be shared with other threads
fn void heap_set_rel(u64 loc, Term val) {
  if (__builtin_expect(THREAD_COUNT == 1, 1)) {
    HEAP[loc] = val;
    return;
  }
  __atomic_store_n(&HEAP[loc], val, __ATOMIC_RELEASE);
}
