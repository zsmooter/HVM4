fn u32 term_ext(Term t) {
  return (t >> EXT_SHIFT) & EXT_MASK;
}
