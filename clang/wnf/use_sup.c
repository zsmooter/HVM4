// (λ{f} &L{a,b})
// ----------------- USE-SUP
// ! F &L = f
// &L{(λ{F₀} a), (λ{F₁} b)}
fn Term wnf_use_sup(Term use, Term sup) {
  ITRS_INC("USE-SUP");
  u64  use_loc = term_val(use);
  u32  lab     = term_ext(sup);
  u64  sup_loc = term_val(sup);
  Copy F       = term_clone_at(use_loc, lab);
  Term use0    = term_new_use(F.k0);
  Term use1    = term_new_use(F.k1);
  Term app0    = term_new_app(use0, heap_read(sup_loc + 0));
  Term app1    = term_new_app(use1, heap_read(sup_loc + 1));
  return term_new_sup_at(sup_loc, lab, app0, app1);
}
