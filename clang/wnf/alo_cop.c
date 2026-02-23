// @{s} n₀
// ------- ALO-DP0
// s[n]₀ or n₀ when substitution missing (n is a de Bruijn level)
//
// @{s} n₁
// ------- ALO-DP1
// s[n]₁ or n₁ when substitution missing (n is a de Bruijn level)
fn Term wnf_alo_cop(u64 ls, u32 len, Term book) {
  u32 lvl  = (u32)term_val(book);
  u32 lab  = term_ext(book);
  u8  tag  = term_tag(book);
  u8  side = (tag == DP0 || tag == BJ0) ? 0 : 1;
  if (lvl == 0 || lvl > len) {
    return term_new(0, tag, lab, lvl);
  }
  u32 idx = len - lvl;
  u64 it  = ls;
  for (u32 i = 0; i < idx && it != 0; i++) {
    it = term_val(heap_read(it + 1));
  }
  u8 rtag = side == 0 ? DP0 : DP1;
  return it != 0 ? term_new(0, rtag, lab, it) : term_new(0, tag, lab, lvl);
}
