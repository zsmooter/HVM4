// @{s} T{a,b,...}
// ---------------- ALO-NOD
// T{@{s}a, @{s}b, ...}
fn Term wnf_alo_nod(u64 alo_loc, u64 ls_loc, u32 len, Term book) {
  u64 loc = term_val(book);
  u8  tag = term_tag(book);
  u32 ext = term_ext(book);
  u32 ari = term_arity(book);
  Term args[16];
  if (ari == 0) {
    return book;
  }
  args[0] = term_new_alo_at(alo_loc, ls_loc, len, loc + 0);
  for (u32 i = 1; i < ari; i++) {
    args[i] = term_new_alo(ls_loc, len, loc + i);
  }
  return term_new_(tag, ext, ari, args);
}
