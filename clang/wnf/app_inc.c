// (↑f x)
// -------- APP-INC
// ↑(f x)
fn Term wnf_app_inc(Term app, Term inc) {
  ITRS_INC("APP-INC");
  u64  app_loc = term_val(app);
  u64  inc_loc = term_val(inc);
  Term f       = heap_read(inc_loc);
  // Build APP(f, x) in-place at app_loc, then store it under INC at inc_loc.
  heap_set(app_loc + 0, f);
  heap_set(inc_loc + 0, term_new(0, APP, 0, app_loc));
  return inc;
}
