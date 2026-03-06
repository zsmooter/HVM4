// (#a op #b)
// -------------- OP2-NUM-NUM
// #(a opr b)
fn Term wnf_op2_num_num_raw(u32 opr, u32 a, u32 b) {
  ITRS_INC("OP2-NUM-NUM");
  return term_new_num(term_op2_u32(opr, a, b));
}

fn Term wnf_op2_num_num(u32 opr, Term x, Term y) {
  return wnf_op2_num_num_raw(opr, (u32)term_val(x), (u32)term_val(y));
}
