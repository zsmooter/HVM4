// (&L{a0,a1} === b)
// ---------------------- EQL-SUP-L
// ! B &L = b
// &L{(a0 === B₀), (a1 === B₁)}
fn Term wnf_eql_sup_l(u64 eql_loc, Term sup, Term b) {
  ITRS_INC("EQL-SUP-L");
  u64  sup_loc = term_val(sup);
  u32  lab = term_ext(sup);
  Term a0  = heap_read(sup_loc + 0);
  Term a1  = heap_read(sup_loc + 1);
  Copy B   = term_clone(lab, b);
  Term eq0 = term_new_eql_at(eql_loc, a0, B.k0);
  Term eq1 = term_new_eql(a1, B.k1);
  return term_new_sup_at(sup_loc, lab, eq0, eq1);
}

// (a === &L{b0,b1})
// ---------------------- EQL-SUP-R
// ! A &L = a
// &L{(A₀ === b0), (A₁ === b1)}
fn Term wnf_eql_sup_r(u64 eql_loc, Term a, Term sup) {
  ITRS_INC("EQL-SUP-R");
  u64  sup_loc = term_val(sup);
  u32  lab = term_ext(sup);
  Term b0  = heap_read(sup_loc + 0);
  Term b1  = heap_read(sup_loc + 1);
  Copy A   = term_clone(lab, a);
  Term eq0 = term_new_eql_at(eql_loc, A.k0, b0);
  Term eq1 = term_new_eql(A.k1, b1);
  return term_new_sup_at(sup_loc, lab, eq0, eq1);
}
