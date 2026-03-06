// AOT Program Emitter
// ===================
//
// Emits standalone C for HVM's case-tree AOT mode.

fn char *table_get(u32 id);

static int AOT_EMIT_ITRS = 1;
#define AOT_EMIT_TMP_LIM 8192

// Needs counters.
fn int aot_emit_counting(const AotBuildCfg *cfg) {
  if (cfg == NULL) {
    return 1;
  }
  return cfg->eval.stats || cfg->eval.silent || cfg->eval.step_by_step;
}

// Emits one counter tick.
fn void aot_emit_itrs_inc(FILE *f, const char *pad) {
  if (!AOT_EMIT_ITRS) {
    return;
  }
  fprintf(f, "%saot_itrs_inc();\n", pad);
}

// Builds one temp name.
fn void aot_emit_tmp(char *out, u32 out_cap, const char *pre, u32 *next) {
  snprintf(out, out_cap, "%s_%u", pre, *next);
  *next = *next + 1;
}

// Builds one deeper pad.
fn void aot_emit_pad_next(char *out, u32 out_cap, const char *pad) {
  snprintf(out, out_cap, "%s  ", pad);
}

// Uppercases one symbol.
fn void aot_emit_upper_ident(char *out, u32 out_cap, const char *name) {
  if (out_cap == 0) {
    return;
  }

  u32 j = 0;
  for (u32 i = 0; name[i] != '\0'; i++) {
    u8 c   = (u8)name[i];
    u8 az  = c >= 'a' && c <= 'z';
    u8 AZ  = c >= 'A' && c <= 'Z';
    u8 d9  = c >= '0' && c <= '9';
    u8 ok  = az || AZ || d9;
    char d = ok ? (char)c : '_';

    if (d >= 'a' && d <= 'z') {
      d = (char)(d - 'a' + 'A');
    }

    if (j + 1 >= out_cap) {
      break;
    }

    out[j++] = d;
  }

  if (j == 0) {
    out[j++] = '_';
  }

  out[j] = '\0';
}

// Builds one function name.
fn void aot_emit_fun_name(char *out, u32 out_cap, const char *name) {
  if (out_cap < 5) {
    if (out_cap > 0) {
      out[0] = '\0';
    }
    return;
  }

  u32 need = 4;
  for (u32 i = 0; name[i] != '\0'; i++) {
    need += 1;
  }
  if (need > out_cap) {
    fprintf(stderr, "ERROR: AOT function name for '@%s' exceeds emitter limit (%u chars)\n", name, out_cap - 1);
    exit(1);
  }

  u32 j = 0;
  out[j++] = 'F';
  out[j++] = 'N';
  out[j++] = '_';

  for (u32 i = 0; name[i] != '\0'; i++) {
    u8 c  = (u8)name[i];
    u8 az = c >= 'a' && c <= 'z';
    u8 AZ = c >= 'A' && c <= 'Z';
    u8 d9 = c >= '0' && c <= '9';
    char d = (az || AZ || d9) ? (char)c : '_';

    if (j == 3 && d9) {
      if (j + 1 >= out_cap) {
        break;
      }
      out[j++] = '_';
    }

    if (j + 1 >= out_cap) {
      break;
    }
    out[j++] = d;
  }

  if (j == 3) {
    out[j++] = '_';
  }
  out[j] = '\0';
}

// Builds one constant name.
fn void aot_emit_const_name(char *out, u32 out_cap, const char *pre, u32 id) {
  char *name = table_get(id);
  if (name == NULL || name[0] == '\0') {
    snprintf(out, out_cap, "%s_%u", pre, id);
    return;
  }

  char up[256];
  aot_emit_upper_ident(up, sizeof(up), name);
  snprintf(out, out_cap, "%s_%s", pre, up);
}

// Emits one constructor token.
fn void aot_emit_ctr_id(FILE *f, u32 id) {
  if (id >= TABLE.len) {
    fprintf(f, "%u", id);
    return;
  }

  char *name = table_get(id);
  if (name == NULL) {
    fprintf(f, "%u", id);
    return;
  }

  char tok[320];
  aot_emit_const_name(tok, sizeof(tok), "C", id);
  fprintf(f, "%s", tok);
}

// Emits one ref token.
fn void aot_emit_ref_id(FILE *f, u32 id) {
  if (id >= TABLE.len || BOOK[id] == 0) {
    fprintf(f, "%u", id);
    return;
  }

  char *name = table_get(id);
  if (name == NULL) {
    fprintf(f, "%u", id);
    return;
  }

  char tok[320];
  aot_emit_const_name(tok, sizeof(tok), "F", id);
  fprintf(f, "%s", tok);
}

// Escapes one byte.
fn void aot_emit_escaped_byte(FILE *f, u8 c) {
  switch (c) {
    case '\\': {
      fputs("\\\\", f);
      return;
    }
    case '"': {
      fputs("\\\"", f);
      return;
    }
    case '\n': {
      fputs("\\n", f);
      return;
    }
    case '\r': {
      fputs("\\r", f);
      return;
    }
    case '\t': {
      fputs("\\t", f);
      return;
    }
    default: {
      break;
    }
  }

  if (c >= 32 && c <= 126) {
    fputc((int)c, f);
    return;
  }

  fprintf(f, "\\%03o", (unsigned)c);
}

// Emits one C string.
fn void aot_emit_c_string_token(FILE *f, const char *str) {
  fputc('"', f);
  for (u32 i = 0; str[i] != '\0'; i++) {
    aot_emit_escaped_byte(f, (u8)str[i]);
  }
  fputc('"', f);
}

// Emits one string decl.
fn void aot_emit_c_string_decl(FILE *f, const char *name, const char *text) {
  fprintf(f, "static const char *%s =\n", name);

  const u8 *ptr = (const u8 *)text;
  if (ptr[0] == '\0') {
    fprintf(f, "  \"\";\n\n");
    return;
  }

  u32 i = 0;
  while (ptr[i] != 0) {
    fprintf(f, "  \"");
    u32 chunk = 0;
    while (ptr[i] != 0 && chunk < 64) {
      aot_emit_escaped_byte(f, ptr[i]);
      i++;
      chunk++;
    }
    fprintf(f, "\"\n");
  }

  fprintf(f, "  ;\n\n");
}

// Marks used constructor ids.
fn void aot_emit_mark_ctr_ids(u64 loc, u8 *ctr_used, u32 ctr_len) {
  Term tm  = heap_read(loc);
  u8   tag = term_tag(tm);

  switch (tag) {
    case C00 ... C16:
    case MAT: {
      u32 id = term_ext(tm);
      if (id < ctr_len) {
        ctr_used[id] = 1;
      }
      break;
    }
    default: {
      break;
    }
  }

  u32 ari = term_arity(tm);
  u64 val = term_val(tm);
  for (u32 i = 0; i < ari; i++) {
    aot_emit_mark_ctr_ids(val + i, ctr_used, ctr_len);
  }
}

// Emits named ids.
fn void aot_emit_named_consts(FILE *f) {
  u32  len      = TABLE.len;
  u32  ctr_cap  = len == 0 ? 1 : len;
  u8  *ctr_used = calloc(ctr_cap, 1);
  if (ctr_used == NULL) {
    sys_error("AOT constant scan allocation failed");
  }

  for (u32 id = 0; id < len; id++) {
    if (BOOK[id] == 0) {
      continue;
    }
    aot_emit_mark_ctr_ids(BOOK[id], ctr_used, len);
  }

  fprintf(f, "// Named Constants\n");
  fprintf(f, "// ---------------\n\n");

  for (u32 id = 0; id < len; id++) {
    if (!ctr_used[id]) {
      continue;
    }
    char tok[320];
    aot_emit_const_name(tok, sizeof(tok), "C", id);
    fprintf(f, "#define %s %u\n", tok, id);
  }

  if (len > 0) {
    fprintf(f, "\n");
  }

  for (u32 id = 0; id < len; id++) {
    if (BOOK[id] == 0) {
      continue;
    }
    char tok[320];
    aot_emit_const_name(tok, sizeof(tok), "F", id);
    fprintf(f, "#define %s %u\n", tok, id);
  }

  fprintf(f, "\n");
  free(ctr_used);
}

#define AOT_SUB_LAM 1
#define AOT_SUB_DUP 2

typedef struct {
  u32 len;
  u32 self_id;
  u8  kind[AOT_SUB_CAP];
  u32 lab[AOT_SUB_CAP];
} AotSubst;

typedef struct {
  u64 root_loc;
  u64 zero_lam_loc;
  u64 succ_lam_loc;
  u32 zero_id;
  u32 succ_id;
  u8  tail_acc;
} AotLoopAdd;

typedef struct {
  u64 root_loc;
  u64 zero_lam_loc;
  u64 succ_lam_loc;
  u32 zero_id;
  u32 succ_id;
  u8  tail_acc;
} AotLoopU32;

// Emits one WNF comment.
fn void aot_emit_wnf_comment(FILE *f, u64 loc, const char *pad) {
  fprintf(f, "%s// wnf ", pad);
  print_term_quoted_ex(f, heap_read(loc), 0);
  fprintf(f, "\n");
}

// Reuses one scrutinee.
fn int aot_emit_reuse_arg(u64 loc) {
  Term term = heap_read(loc);
  switch (term_tag(term)) {
    case LAM:
    case MAT:
    case SWI: {
      return 1;
    }
    case USE: {
      return 1;
    }
    default: {
      return 0;
    }
  }
}

// Emits one subst entry.
fn void aot_emit_subst_entry(FILE *f, u32 idx, const char *val, const char *pad) {
  fprintf(f, "%su64 ls%u = heap_alloc(2);\n", pad, idx);
  fprintf(f, "%sheap_set(ls%u + 0, %s);\n", pad, idx, val);
  fprintf(f, "%sheap_set(ls%u + 1, term_new(0, NUM, 0, ", pad, idx);
  if (idx == 0) {
    fprintf(f, "0ULL");
  } else {
    fprintf(f, "ls%u", idx - 1);
  }
  fprintf(f, "));\n");
}

// Emits one ALO fallback expr.
fn void aot_emit_fallback_expr(FILE *f, u64 loc, AotSubst sub) {
  fprintf(f, "aot_fallback_alo_ls(%lluULL, %u, ", (unsigned long long)loc, sub.len);
  if (sub.len == 0) {
    fprintf(f, "0ULL");
  } else {
    fprintf(f, "ls%u", sub.len - 1);
  }
  fprintf(f, ")");
}

// Returns one fallback ALO.
fn void aot_emit_ret_fallback(FILE *f, u64 loc, AotSubst sub, const char *pad) {
  fprintf(f, "%sreturn ", pad);
  aot_emit_fallback_expr(f, loc, sub);
  fprintf(f, ";\n");
}

// Returns one fallback app.
fn void aot_emit_ret_fallback_app(FILE *f, u64 loc, AotSubst sub, const char *arg, const char *pad) {
  fprintf(f, "%sreturn term_new_app(", pad);
  aot_emit_fallback_expr(f, loc, sub);
  fprintf(f, ", %s);\n", arg);
}

// Checks one binder var.
fn int aot_term_is_var_lvl(Term term, u32 lvl) {
  switch (term_tag(term)) {
    case VAR:
    case BJV: {
      return (u32)term_val(term) == lvl;
    }
    default: {
      return 0;
    }
  }
}

// Checks one num literal.
fn int aot_term_is_num_val(Term term, u32 val) {
  return term_tag(term) == NUM && (u32)term_val(term) == val;
}

// Collects one unary constructor chain.
fn int aot_collect_ctr1_chain(u64 loc, u32 *ctr_id, u32 *reps, u64 *bod_loc) {
  Term term = heap_read(loc);
  if (term_tag(term) != C01) {
    return 0;
  }

  u32 id  = term_ext(term);
  u32 len = 0;
  u64 cur = loc;
  while (1) {
    term = heap_read(cur);
    if (term_tag(term) != C01 || term_ext(term) != id) {
      break;
    }
    len += 1;
    cur = term_val(term);
  }

  *ctr_id  = id;
  *reps    = len;
  *bod_loc = cur;
  return 1;
}

// Matches `self(x)`.
fn int aot_match_self_app1(u64 loc, u32 self_id, u32 x_lvl) {
  Term app = heap_read(loc);
  if (term_tag(app) != APP) {
    return 0;
  }

  u64  app_loc = term_val(app);
  Term fun     = heap_read(app_loc + 0);
  Term arg     = heap_read(app_loc + 1);

  if (!aot_term_is_var_lvl(arg, x_lvl)) {
    return 0;
  }

  return term_tag(fun) == REF && term_ext(fun) == self_id;
}

// Matches `self(x,y)`.
fn int aot_match_self_app2(u64 loc, u32 self_id, u32 x_lvl, u32 y_lvl) {
  Term app1 = heap_read(loc);
  if (term_tag(app1) != APP) {
    return 0;
  }

  u64  app1_loc = term_val(app1);
  Term fun1     = heap_read(app1_loc + 0);
  Term arg1     = heap_read(app1_loc + 1);

  if (!aot_term_is_var_lvl(arg1, y_lvl)) {
    return 0;
  }

  Term app0 = fun1;
  if (term_tag(app0) != APP) {
    return 0;
  }

  u64  app0_loc = term_val(app0);
  Term fun0     = heap_read(app0_loc + 0);
  Term arg0     = heap_read(app0_loc + 1);

  if (!aot_term_is_var_lvl(arg0, x_lvl)) {
    return 0;
  }

  return term_tag(fun0) == REF && term_ext(fun0) == self_id;
}

// Checks one unary ctr wrapping one binder var.
fn int aot_term_is_ctr1_var(Term term, u32 ctr_id, u32 lvl) {
  if (term_tag(term) != C01 || term_ext(term) != ctr_id) {
    return 0;
  }

  return aot_term_is_var_lvl(heap_read(term_val(term) + 0), lvl);
}

// Checks one numeric op over one binder var.
fn int aot_term_is_op2_num_var(Term term, u32 opr, u32 num, u32 lvl) {
  if (term_tag(term) != OP2 || term_ext(term) != opr) {
    return 0;
  }

  u64 op2 = term_val(term);
  if (!aot_term_is_num_val(heap_read(op2 + 0), num)) {
    return 0;
  }

  return aot_term_is_var_lvl(heap_read(op2 + 1), lvl);
}

// Matches `self(x, ctr(y))`.
fn int aot_match_self_app2_ctr1(u64 loc, u32 self_id, u32 x_lvl, u32 y_lvl, u32 ctr_id) {
  Term app1 = heap_read(loc);
  if (term_tag(app1) != APP) {
    return 0;
  }

  u64  app1_loc = term_val(app1);
  Term fun1     = heap_read(app1_loc + 0);
  Term arg1     = heap_read(app1_loc + 1);

  if (!aot_term_is_ctr1_var(arg1, ctr_id, y_lvl)) {
    return 0;
  }

  Term app0 = fun1;
  if (term_tag(app0) != APP) {
    return 0;
  }

  u64  app0_loc = term_val(app0);
  Term fun0     = heap_read(app0_loc + 0);
  Term arg0     = heap_read(app0_loc + 1);

  if (!aot_term_is_var_lvl(arg0, x_lvl)) {
    return 0;
  }

  return term_tag(fun0) == REF && term_ext(fun0) == self_id;
}

// Matches `self(x, num + y)`.
fn int aot_match_self_app2_op2_num_var(u64 loc, u32 self_id, u32 x_lvl, u32 y_lvl, u32 opr, u32 num) {
  Term app1 = heap_read(loc);
  if (term_tag(app1) != APP) {
    return 0;
  }

  u64  app1_loc = term_val(app1);
  Term fun1     = heap_read(app1_loc + 0);
  Term arg1     = heap_read(app1_loc + 1);

  if (!aot_term_is_op2_num_var(arg1, opr, num, y_lvl)) {
    return 0;
  }

  Term app0 = fun1;
  if (term_tag(app0) != APP) {
    return 0;
  }

  u64  app0_loc = term_val(app0);
  Term fun0     = heap_read(app0_loc + 0);
  Term arg0     = heap_read(app0_loc + 1);

  if (!aot_term_is_var_lvl(arg0, x_lvl)) {
    return 0;
  }

  return term_tag(fun0) == REF && term_ext(fun0) == self_id;
}

// Matches one add loop.
fn int aot_match_loop_add(u64 root, u32 self_id, AotLoopAdd *out) {
  Term root_tm = heap_read(root);
  if (term_tag(root_tm) != MAT) {
    return 0;
  }

  u64 mat0 = term_val(root_tm);
  u64 hit0 = mat0 + 0;
  u64 mis0 = mat0 + 1;

  Term hit0_tm = heap_read(hit0);
  if (term_tag(hit0_tm) != LAM) {
    return 0;
  }
  if (!aot_term_is_var_lvl(heap_read(term_val(hit0_tm)), 1)) {
    return 0;
  }

  Term mis0_tm = heap_read(mis0);
  if (term_tag(mis0_tm) != MAT) {
    return 0;
  }

  u64 mat1 = term_val(mis0_tm);
  u64 hit1 = mat1 + 0;
  u64 mis1 = mat1 + 1;

  if (!aot_term_is_num_val(heap_read(mis1), 0)) {
    return 0;
  }

  Term hit1_tm = heap_read(hit1);
  if (term_tag(hit1_tm) != LAM) {
    return 0;
  }

  u64 lam1 = term_val(hit1_tm);
  Term lam1_tm = heap_read(lam1);
  if (term_tag(lam1_tm) != LAM) {
    return 0;
  }

  u64 body = term_val(lam1_tm);
  Term bod_tm = heap_read(body);
  out->root_loc     = root;
  out->zero_lam_loc = hit0;
  out->succ_lam_loc = lam1;
  out->zero_id      = term_ext(root_tm);
  out->succ_id      = term_ext(mis0_tm);

  if (term_tag(bod_tm) == C01 && term_ext(bod_tm) == out->succ_id) {
    u64 ctr = term_val(bod_tm);
    if (aot_match_self_app2(ctr + 0, self_id, 1, 2)) {
      out->tail_acc = 0;
      return 1;
    }
  }

  if (aot_match_self_app2_ctr1(body, self_id, 1, 2, out->succ_id)) {
    out->tail_acc = 1;
    return 1;
  }

  return 0;
}

// Matches one u32 loop.
fn int aot_match_loop_u32(u64 root, u32 self_id, AotLoopU32 *out) {
  Term root_tm = heap_read(root);
  if (term_tag(root_tm) != MAT) {
    return 0;
  }

  u64 mat0 = term_val(root_tm);
  u64 hit0 = mat0 + 0;
  u64 mis0 = mat0 + 1;

  Term mis0_tm = heap_read(mis0);
  if (term_tag(mis0_tm) != MAT) {
    return 0;
  }

  u64 mat1 = term_val(mis0_tm);
  u64 hit1 = mat1 + 0;
  u64 mis1 = mat1 + 1;

  if (!aot_term_is_num_val(heap_read(mis1), 0)) {
    return 0;
  }

  Term hit1_tm = heap_read(hit1);
  if (term_tag(hit1_tm) != LAM) {
    return 0;
  }

  out->root_loc = root;
  out->zero_lam_loc = 0;
  out->succ_lam_loc = 0;
  out->zero_id  = term_ext(root_tm);
  out->succ_id  = term_ext(mis0_tm);

  if (aot_term_is_num_val(heap_read(hit0), 0)) {
    u64  body   = term_val(hit1_tm);
    Term bod_tm = heap_read(body);
    if (term_tag(bod_tm) != OP2 || term_ext(bod_tm) != 0) {
      return 0;
    }

    u64 op2 = term_val(bod_tm);
    if (!aot_term_is_num_val(heap_read(op2 + 0), 1)) {
      return 0;
    }
    if (!aot_match_self_app1(op2 + 1, self_id, 1)) {
      return 0;
    }

    out->tail_acc = 0;
    return 1;
  }

  Term hit0_tm = heap_read(hit0);
  if (term_tag(hit0_tm) != LAM) {
    return 0;
  }
  if (!aot_term_is_var_lvl(heap_read(term_val(hit0_tm)), 1)) {
    return 0;
  }

  u64  lam1    = term_val(hit1_tm);
  Term lam1_tm = heap_read(lam1);
  if (term_tag(lam1_tm) != LAM) {
    return 0;
  }

  if (!aot_match_self_app2_op2_num_var(term_val(lam1_tm), self_id, 1, 2, 0, 1)) {
    return 0;
  }

  out->zero_lam_loc = hit0;
  out->succ_lam_loc = lam1;
  out->tail_acc     = 1;
  return 1;
}

// Emits constructor cases.
fn void aot_emit_ctr_cases(FILE *f, const char *pad) {
  fprintf(f, "%scase C00: case C01: case C02: case C03: case C04:\n", pad);
  fprintf(f, "%scase C05: case C06: case C07: case C08: case C09:\n", pad);
  fprintf(f, "%scase C10: case C11: case C12: case C13: case C14:\n", pad);
  fprintf(f, "%scase C15: case C16: {\n", pad);
}

#define AOT_CALL_ARG_CAP 64

fn void aot_emit_spine(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp);
fn void aot_emit_expr(FILE *f, u64 loc, AotSubst sub, const char *out, const char *pad, u32 *tmp);

// Returns one compiled function name for a REF id.
fn int aot_emit_get_fun_name(char *out, u32 out_cap, u32 ref_id) {
  if (ref_id >= TABLE.len || BOOK[ref_id] == 0) {
    return 0;
  }

  char *name = table_get(ref_id);
  if (name == NULL) {
    return 0;
  }

  aot_emit_fun_name(out, out_cap, name);
  return 1;
}

// Checks one direct unary helper.
fn int aot_emit_has_direct_mat1(u32 ref_id) {
  AotLoopAdd loop_add;
  AotLoopU32 loop_u32;
  if (ref_id >= BOOK_CAP || BOOK[ref_id] == 0) {
    return 0;
  }
  if (aot_match_loop_add(BOOK[ref_id], ref_id, &loop_add)) {
    return 0;
  }
  if (aot_match_loop_u32(BOOK[ref_id], ref_id, &loop_u32)) {
    return 0;
  }
  return term_tag(heap_read(BOOK[ref_id])) == MAT;
}

// Returns one direct helper name for a saturated REF call.
fn int aot_emit_get_direct_name(char *out, u32 out_cap, u32 ref_id, u32 argc) {
  AotLoopAdd loop;
  if (argc != 2 || ref_id >= BOOK_CAP || BOOK[ref_id] == 0) {
    return 0;
  }
  if (!aot_match_loop_add(BOOK[ref_id], ref_id, &loop)) {
    return 0;
  }
  if (!aot_emit_get_fun_name(out, out_cap, ref_id)) {
    return 0;
  }
  strncat(out, "_2", out_cap - strlen(out) - 1);
  return 1;
}

// Collects one APP chain headed by a REF, keeping args outer-to-inner.
fn int aot_emit_collect_ref_app(u64 loc, u32 *ref_id, u64 *args, u32 *argc) {
  u64 cur = loc;
  u32 len = 0;

  while (1) {
    Term term = heap_read(cur);
    switch (term_tag(term)) {
      case APP: {
        if (len >= AOT_CALL_ARG_CAP) {
          return 0;
        }
        u64 app = term_val(term);
        args[len++] = app + 1;
        cur = app + 0;
        continue;
      }
      case REF: {
        *ref_id = term_ext(term);
        *argc   = len;
        return 1;
      }
      default: {
        return 0;
      }
    }
  }
}

// Emits one residual APP expression without compiled-call lowering.
fn void aot_emit_expr_app_raw(FILE *f, u64 loc, AotSubst sub, const char *out, const char *pad, u32 *tmp) {
  Term term = heap_read(loc);
  u64  app  = term_val(term);

  char fun[32];
  char arg[32];
  aot_emit_tmp(fun, sizeof(fun), "fun", tmp);
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);

  aot_emit_expr(f, app + 0, sub, fun, pad, tmp);
  aot_emit_expr(f, app + 1, sub, arg, pad, tmp);

  fprintf(f, "%sTerm %s = term_new_app(%s, %s);\n", pad, out, fun, arg);
}

// Emits one residual APP expression into an existing local.
fn void aot_emit_expr_app_set(FILE *f, u64 loc, AotSubst sub, const char *out, const char *pad, u32 *tmp) {
  Term term = heap_read(loc);
  u64  app  = term_val(term);

  char fun[32];
  char arg[32];
  aot_emit_tmp(fun, sizeof(fun), "fun", tmp);
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);

  aot_emit_expr(f, app + 0, sub, fun, pad, tmp);
  aot_emit_expr(f, app + 1, sub, arg, pad, tmp);

  fprintf(f, "%s%s = term_new_app(%s, %s);\n", pad, out, fun, arg);
}

// Emits one compiled direct call for an APP chain headed by REF.
fn void aot_emit_expr_call(FILE *f, u64 loc, AotSubst sub, const char *out, const char *pad, u32 *tmp) {
  u32  ref_id = 0;
  u32  argc   = 0;
  u64  args[AOT_CALL_ARG_CAP];
  char fun_name[256];

  if (!aot_emit_collect_ref_app(loc, &ref_id, args, &argc)) {
    aot_emit_expr_app_raw(f, loc, sub, out, pad, tmp);
    return;
  }

  if (!aot_emit_get_fun_name(fun_name, sizeof(fun_name), ref_id)) {
    aot_emit_expr_app_raw(f, loc, sub, out, pad, tmp);
    return;
  }

  char pad1[128];
  char pad2[128];
  char base[32];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);
  aot_emit_pad_next(pad2, sizeof(pad2), pad1);
  aot_emit_tmp(base, sizeof(base), "sb", tmp);

  fprintf(f, "%sTerm %s;\n", pad, out);
  fprintf(f, "%sif (STEPS_ITRS_LIM == 0) {\n", pad);
  fprintf(f, "%su32 %s = *sp;\n", pad1, base);
  if (argc == 1 && ref_id == sub.self_id && aot_emit_has_direct_mat1(ref_id)) {
    char dir_name[256];
    char arg[32];
    aot_emit_get_fun_name(dir_name, sizeof(dir_name), ref_id);
    fprintf(f, "%s// Direct unary self call.\n", pad1);
    aot_emit_tmp(arg, sizeof(arg), "arg", tmp);
    aot_emit_expr(f, args[0], sub, arg, pad1, tmp);
    fprintf(f, "%s%s = aot_call_expr1(%s_1, %s, ", pad1, out, dir_name, dir_name);
    aot_emit_ref_id(f, ref_id);
    fprintf(f, ", %s, stack, sp, %s);\n", arg, base);
    fprintf(f, "%s} else {\n", pad);
    aot_emit_expr_app_set(f, loc, sub, out, pad1, tmp);
    fprintf(f, "%s}\n", pad);
    return;
  }
  for (u32 i = 0; i < argc; i++) {
    char arg[32];
    aot_emit_tmp(arg, sizeof(arg), "arg", tmp);
    aot_emit_expr(f, args[i], sub, arg, pad1, tmp);
    fprintf(f, "%saot_push_app_arg(stack, sp, %s, %s);\n", pad1, base, arg);
  }
  fprintf(f, "%s%s = aot_call_expr(%s, ", pad1, out, fun_name);
  aot_emit_ref_id(f, ref_id);
  fprintf(f, ", stack, sp, %s);\n", base);
  fprintf(f, "%s} else {\n", pad);
  aot_emit_expr_app_set(f, loc, sub, out, pad1, tmp);
  fprintf(f, "%s}\n", pad);
}

// Emits one saturated direct tail call.
fn int aot_emit_spine_direct(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  u32  ref_id = 0;
  u32  argc   = 0;
  u64  args[AOT_CALL_ARG_CAP];
  char fun_name[256];

  if (!aot_emit_collect_ref_app(loc, &ref_id, args, &argc)) {
    return 0;
  }
  if (!aot_emit_get_direct_name(fun_name, sizeof(fun_name), ref_id, argc)) {
    return 0;
  }

  char pad1[128];
  char arg[AOT_CALL_ARG_CAP][32];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);

  fprintf(f, "%sif (STEPS_ITRS_LIM == 0) {\n", pad);
  for (u32 i = 0; i < argc; i++) {
    aot_emit_tmp(arg[i], sizeof(arg[i]), "arg", tmp);
    aot_emit_expr(f, args[i], sub, arg[i], pad1, tmp);
  }
  fprintf(f, "%sreturn %s(", pad1, fun_name);
  for (u32 i = argc; i > 0; i--) {
    if (i != argc) {
      fprintf(f, ", ");
    }
    fprintf(f, "%s", arg[i - 1]);
  }
  fprintf(f, ");\n");
  fprintf(f, "%s}\n", pad);
  return 1;
}

// Emits one expression into a temporary and returns it.
fn void aot_emit_ret_expr(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  char ret[32];
  aot_emit_tmp(ret, sizeof(ret), "ret", tmp);
  aot_emit_expr(f, loc, sub, ret, pad, tmp);
  fprintf(f, "%sreturn %s;\n", pad, ret);
}

// Emits one APP spine node.
fn void aot_emit_spine_app(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term    = heap_read(loc);
  u64  app_loc = term_val(term);
  u64  fun_loc = app_loc + 0;
  u64  arg_loc = app_loc + 1;

  char arg[32];
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);

  aot_emit_expr(f, arg_loc, sub, arg, pad, tmp);

  fprintf(f, "%saot_push_app_arg(stack, sp, sb, %s);\n", pad, arg);

  aot_emit_spine(f, fun_loc, sub, pad, tmp);
}

// Emits one LAM spine node.
fn void aot_emit_spine_lam(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term = heap_read(loc);

  if (sub.len >= AOT_SUB_CAP) {
    aot_emit_ret_fallback(f, loc, sub, pad);
    return;
  }

  char x_n[32];
  snprintf(x_n, sizeof(x_n), "x%u", sub.len);

  fprintf(f, "%sTerm %s = aot_pop_app_arg(stack, sp, sb);\n", pad, x_n);
  fprintf(f, "%sif (!%s) {\n", pad, x_n);
  {
    char pad1[128];
    aot_emit_pad_next(pad1, sizeof(pad1), pad);
    aot_emit_ret_fallback(f, loc, sub, pad1);
  }
  fprintf(f, "%s}\n", pad);

  aot_emit_itrs_inc(f, pad);
  {
    char sub_x[64];
    snprintf(sub_x, sizeof(sub_x), "term_sub_set(%s, 1)", x_n);
    aot_emit_subst_entry(f, sub.len, sub_x, pad);
  }

  AotSubst sub1 = sub;
  sub1.kind[sub.len] = AOT_SUB_LAM;
  sub1.lab[sub.len]  = 0;
  sub1.len = sub.len + 1;

  aot_emit_spine(f, term_val(term), sub1, pad, tmp);
}

// Emits one MAT spine node.
fn void aot_emit_spine_mat_arg(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp, const char *arg_in) {
  Term term    = heap_read(loc);
  u64  mat_loc = term_val(term);
  u64  hit_loc = mat_loc + 0;
  u64  mis_loc = mat_loc + 1;

  char arg[32];
  char tag[32];
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);
  aot_emit_tmp(tag, sizeof(tag), "tag", tmp);

  char pad1[128];
  char pad2[128];
  char pad3[128];
  char pad4[128];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);
  aot_emit_pad_next(pad2, sizeof(pad2), pad1);
  aot_emit_pad_next(pad3, sizeof(pad3), pad2);
  aot_emit_pad_next(pad4, sizeof(pad4), pad3);

  if (arg_in == NULL) {
    fprintf(f, "%sTerm %s = aot_pop_app_arg(stack, sp, sb);\n", pad, arg);
    fprintf(f, "%sif (!%s) {\n", pad, arg);
    aot_emit_ret_fallback(f, loc, sub, pad1);
    fprintf(f, "%s}\n", pad);
  } else {
    fprintf(f, "%sTerm %s = %s;\n", pad, arg, arg_in);
  }

  fprintf(f, "%s%s = aot_force(%s);\n", pad, arg, arg);
  fprintf(f, "%su8 %s = term_tag(%s);\n", pad, tag, arg);
  fprintf(f, "%sswitch (%s) {\n", pad, tag);

  fprintf(f, "%scase C00: case C01: case C02: case C03: case C04:\n", pad1);
  fprintf(f, "%scase C05: case C06: case C07: case C08: case C09:\n", pad1);
  fprintf(f, "%scase C10: case C11: case C12: case C13: case C14:\n", pad1);
  fprintf(f, "%scase C15: case C16: {\n", pad1);

  fprintf(f, "%sswitch (term_ext(%s)) {\n", pad2, arg);

  fprintf(f, "%scase ", pad3);
  aot_emit_ctr_id(f, term_ext(term));
  fprintf(f, ": {\n");
  aot_emit_itrs_inc(f, pad4);
  fprintf(f, "%saot_push_fields(stack, sp, sb, %s);\n", pad4, arg);
  aot_emit_spine(f, hit_loc, sub, pad4, tmp);
  fprintf(f, "%s}\n", pad3);

  fprintf(f, "%sdefault: {\n", pad3);
  aot_emit_itrs_inc(f, pad4);
  if (aot_emit_reuse_arg(mis_loc)) {
    fprintf(f, "%saot_push_app_arg(stack, sp, sb, %s);\n", pad4, arg);
  }
  aot_emit_spine(f, mis_loc, sub, pad4, tmp);
  fprintf(f, "%s}\n", pad3);

  fprintf(f, "%s}\n", pad2);
  fprintf(f, "%s}\n", pad1);

  fprintf(f, "%sdefault: {\n", pad1);
  aot_emit_ret_fallback_app(f, loc, sub, arg, pad2);
  fprintf(f, "%s}\n", pad1);

  fprintf(f, "%s}\n", pad);
}

// Emits one MAT spine node.
fn void aot_emit_spine_mat(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  aot_emit_spine_mat_arg(f, loc, sub, pad, tmp, NULL);
}

// Emits one USE spine node.
fn void aot_emit_spine_use(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term  = heap_read(loc);
  u64  f_loc = term_val(term);

  char arg[32];
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);

  char pad1[128];
  char pad2[128];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);
  aot_emit_pad_next(pad2, sizeof(pad2), pad1);

  fprintf(f, "%sTerm %s = aot_pop_app_arg(stack, sp, sb);\n", pad, arg);
  fprintf(f, "%sif (!%s) {\n", pad, arg);
  aot_emit_ret_fallback(f, loc, sub, pad1);
  fprintf(f, "%s}\n", pad);

  fprintf(f, "%s%s = aot_eval(%s, sp);\n", pad, arg, arg);
  fprintf(f, "%sswitch (term_tag(%s)) {\n", pad, arg);

  fprintf(f, "%scase ERA: {\n", pad1);
  aot_emit_itrs_inc(f, pad2);
  fprintf(f, "%sTerm ret = term_new_era();\n", pad2);
  fprintf(f, "%sreturn ret;\n", pad2);
  fprintf(f, "%s}\n", pad1);

  fprintf(f, "%scase SUP:\n", pad1);
  fprintf(f, "%scase INC: {\n", pad1);
  aot_emit_ret_fallback_app(f, loc, sub, arg, pad2);
  fprintf(f, "%s}\n", pad1);

  fprintf(f, "%sdefault: {\n", pad1);
  aot_emit_itrs_inc(f, pad2);
  fprintf(f, "%saot_push_app_arg(stack, sp, sb, %s);\n", pad2, arg);
  aot_emit_spine(f, f_loc, sub, pad2, tmp);
  fprintf(f, "%s}\n", pad1);

  fprintf(f, "%s}\n", pad);
}

// Emits one SWI spine node.
fn void aot_emit_spine_swi(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term    = heap_read(loc);
  u64  swi_loc = term_val(term);
  u64  hit_loc = swi_loc + 0;
  u64  mis_loc = swi_loc + 1;

  char arg[32];
  aot_emit_tmp(arg, sizeof(arg), "arg", tmp);

  char pad1[128];
  char pad2[128];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);
  aot_emit_pad_next(pad2, sizeof(pad2), pad1);

  fprintf(f, "%sTerm %s = aot_pop_app_arg(stack, sp, sb);\n", pad, arg);
  fprintf(f, "%sif (!%s) {\n", pad, arg);
  aot_emit_ret_fallback(f, loc, sub, pad1);
  fprintf(f, "%s}\n", pad);

  fprintf(f, "%s%s = aot_force(%s);\n", pad, arg, arg);
  fprintf(f, "%sif (term_tag(%s) != NUM) {\n", pad, arg);
  aot_emit_ret_fallback_app(f, loc, sub, arg, pad1);
  fprintf(f, "%s}\n", pad);

  fprintf(f, "%sif ((u32)term_val(%s) == %u) {\n", pad, arg, term_ext(term));
  aot_emit_itrs_inc(f, pad1);
  aot_emit_spine(f, hit_loc, sub, pad1, tmp);
  fprintf(f, "%s} else {\n", pad);
  aot_emit_itrs_inc(f, pad1);
  if (aot_emit_reuse_arg(mis_loc)) {
    fprintf(f, "%saot_push_app_arg(stack, sp, sb, %s);\n", pad1, arg);
  }
  aot_emit_spine(f, mis_loc, sub, pad1, tmp);
  fprintf(f, "%s}\n", pad);
}

// Emits one DUP spine node.
fn void aot_emit_spine_dup(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term    = heap_read(loc);
  u64  dup_loc = term_val(term);
  u64  val_loc = dup_loc + 0;
  u64  bod_loc = dup_loc + 1;

  if (sub.len >= AOT_SUB_CAP) {
    aot_emit_ret_fallback(f, loc, sub, pad);
    return;
  }

  char x_n[32];
  snprintf(x_n, sizeof(x_n), "x%u", sub.len);
  aot_emit_expr(f, val_loc, sub, x_n, pad, tmp);
  aot_emit_subst_entry(f, sub.len, x_n, pad);
  fprintf(f, "%sDups d%u = (Dups){ term_new_dp0(%u, ls%u), term_new_dp1(%u, ls%u) };\n",
    pad,
    sub.len,
    term_ext(term),
    sub.len,
    term_ext(term),
    sub.len);

  AotSubst sub1 = sub;
  sub1.kind[sub.len] = AOT_SUB_DUP;
  sub1.lab[sub.len]  = term_ext(term);
  sub1.len = sub.len + 1;

  aot_emit_spine(f, bod_loc, sub1, pad, tmp);
}

// Emits one REF spine node.
fn void aot_emit_spine_ref(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  Term term = heap_read(loc);
  u32 ref_id = term_ext(term);
  char pad1[128];
  char fun_name[256];
  aot_emit_pad_next(pad1, sizeof(pad1), pad);

  fprintf(f, "%sif (STEPS_ITRS_LIM == 0) {\n", pad);
  if (aot_emit_get_fun_name(fun_name, sizeof(fun_name), ref_id)) {
    fprintf(f, "%sreturn aot_call_direct(%s, ", pad1, fun_name);
    aot_emit_ref_id(f, ref_id);
    fprintf(f, ", stack, sp, sb);\n");
  } else {
    fprintf(f, "%sreturn aot_call_ref(", pad1);
    aot_emit_ref_id(f, ref_id);
    fprintf(f, ", stack, sp, sb);\n");
  }
  fprintf(f, "%s}\n", pad);

  aot_emit_ret_expr(f, loc, sub, pad, tmp);
}

// Emits one spine node.
fn void aot_emit_spine(FILE *f, u64 loc, AotSubst sub, const char *pad, u32 *tmp) {
  if (*tmp >= AOT_EMIT_TMP_LIM) {
    aot_emit_ret_fallback(f, loc, sub, pad);
    return;
  }

  Term term = heap_read(loc);
  u8   tag  = term_tag(term);

  aot_emit_wnf_comment(f, loc, pad);

  switch (tag) {
    case APP: {
      if (aot_emit_spine_direct(f, loc, sub, pad, tmp)) {
        aot_emit_spine_app(f, loc, sub, pad, tmp);
        return;
      }
      aot_emit_spine_app(f, loc, sub, pad, tmp);
      return;
    }
    case LAM: {
      aot_emit_spine_lam(f, loc, sub, pad, tmp);
      return;
    }
    case MAT: {
      aot_emit_spine_mat(f, loc, sub, pad, tmp);
      return;
    }
    case SWI: {
      aot_emit_spine_swi(f, loc, sub, pad, tmp);
      return;
    }
    case USE: {
      aot_emit_spine_use(f, loc, sub, pad, tmp);
      return;
    }
    case DUP: {
      aot_emit_spine_dup(f, loc, sub, pad, tmp);
      return;
    }
    case REF: {
      aot_emit_spine_ref(f, loc, sub, pad, tmp);
      return;
    }
    default: {
      aot_emit_ret_expr(f, loc, sub, pad, tmp);
      return;
    }
  }
}

// Emits one expression node into local `Term out`.
fn void aot_emit_expr(FILE *f, u64 loc, AotSubst sub, const char *out, const char *pad, u32 *tmp) {
  if (*tmp >= AOT_EMIT_TMP_LIM) {
    fprintf(f, "%sTerm %s = ", pad, out);
    aot_emit_fallback_expr(f, loc, sub);
    fprintf(f, ";\n");
    return;
  }

  Term term = heap_read(loc);
  u8   tag  = term_tag(term);

  switch (tag) {
    case NUM: {
      fprintf(f, "%sTerm %s = term_new_num((u32)%lluULL);\n", pad, out, (unsigned long long)term_val(term));
      return;
    }
    case ERA: {
      fprintf(f, "%sTerm %s = term_new_era();\n", pad, out);
      return;
    }
    case ANY: {
      fprintf(f, "%sTerm %s = term_new_any();\n", pad, out);
      return;
    }
    case NAM: {
      fprintf(f, "%sTerm %s = term_new_nam(%u);\n", pad, out, term_ext(term));
      return;
    }
    case REF: {
      fprintf(f, "%sTerm %s = term_new_ref(", pad, out);
      aot_emit_ref_id(f, term_ext(term));
      fprintf(f, ");\n");
      return;
    }
    case C00: {
      fprintf(f, "%sTerm %s = term_new(0, C00, ", pad, out);
      aot_emit_ctr_id(f, term_ext(term));
      fprintf(f, ", 0);\n");
      return;
    }
    case C01 ... C16: {
      u32 ctr_id  = 0;
      u32 reps    = 0;
      u64 bod_loc = 0;
      if (aot_collect_ctr1_chain(loc, &ctr_id, &reps, &bod_loc) && reps > 1) {
        char bod[32];
        aot_emit_tmp(bod, sizeof(bod), "bod", tmp);
        aot_emit_expr(f, bod_loc, sub, bod, pad, tmp);
        fprintf(f, "%sTerm %s = aot_wrap_ctr1(", pad, out);
        aot_emit_ctr_id(f, ctr_id);
        fprintf(f, ", %u, %s);\n", reps, bod);
        return;
      }

      u32 ari = (u32)(tag - C00);
      u64 ctr = term_val(term);

      char arr[32];
      aot_emit_tmp(arr, sizeof(arr), "args", tmp);
      fprintf(f, "%sTerm %s[%u];\n", pad, arr, ari);

      for (u32 i = 0; i < ari; i++) {
        char arg[32];
        aot_emit_tmp(arg, sizeof(arg), "arg", tmp);
        aot_emit_expr(f, ctr + i, sub, arg, pad, tmp);
        fprintf(f, "%s%s[%u] = %s;\n", pad, arr, i, arg);
      }

      fprintf(f, "%sTerm %s = term_new_ctr(", pad, out);
      aot_emit_ctr_id(f, term_ext(term));
      fprintf(f, ", %u, %s);\n", ari, arr);
      return;
    }
    case APP: {
      aot_emit_expr_call(f, loc, sub, out, pad, tmp);
      return;
    }
    case OP2: {
      u64 op2 = term_val(term);

      char x[32];
      char y[32];
      char pad1[128];
      char pad2[128];
      char pad3[128];
      aot_emit_tmp(x, sizeof(x), "x", tmp);
      aot_emit_tmp(y, sizeof(y), "y", tmp);
      aot_emit_pad_next(pad1, sizeof(pad1), pad);
      aot_emit_pad_next(pad2, sizeof(pad2), pad1);
      aot_emit_pad_next(pad3, sizeof(pad3), pad2);

      aot_emit_expr(f, op2 + 0, sub, x, pad, tmp);
      aot_emit_expr(f, op2 + 1, sub, y, pad, tmp);

      fprintf(f, "%sTerm %s;\n", pad, out);
      fprintf(f, "%sswitch (term_tag(%s)) {\n", pad, x);
      fprintf(f, "%scase NUM: {\n", pad1);
      fprintf(f, "%sswitch (term_tag(%s)) {\n", pad2, y);
      fprintf(f, "%scase NUM: {\n", pad3);
      fprintf(f, "%s%s = term_new_num(term_op2_u32(%u, (u32)term_val(%s), (u32)term_val(%s)));\n",
        pad3,
        out,
        term_ext(term),
        x,
        y);
      aot_emit_itrs_inc(f, pad3);
      fprintf(f, "%sbreak;\n", pad3);
      fprintf(f, "%s}\n", pad2);
      fprintf(f, "%sdefault: {\n", pad3);
      fprintf(f, "%s%s = term_new_op2(%u, %s, %s);\n", pad3, out, term_ext(term), x, y);
      fprintf(f, "%sbreak;\n", pad3);
      fprintf(f, "%s}\n", pad2);
      fprintf(f, "%s}\n", pad1);
      fprintf(f, "%sbreak;\n", pad1);
      fprintf(f, "%s}\n", pad1);
      fprintf(f, "%sdefault: {\n", pad1);
      fprintf(f, "%s%s = term_new_op2(%u, %s, %s);\n", pad2, out, term_ext(term), x, y);
      fprintf(f, "%sbreak;\n", pad2);
      fprintf(f, "%s}\n", pad1);
      fprintf(f, "%s}\n", pad);
      return;
    }
    case SUP: {
      u64 sup = term_val(term);

      char a[32];
      char b[32];
      aot_emit_tmp(a, sizeof(a), "a", tmp);
      aot_emit_tmp(b, sizeof(b), "b", tmp);

      aot_emit_expr(f, sup + 0, sub, a, pad, tmp);
      aot_emit_expr(f, sup + 1, sub, b, pad, tmp);

      fprintf(f, "%sTerm %s = term_new_sup(%u, %s, %s);\n", pad, out, term_ext(term), a, b);
      return;
    }
    case USE: {
      u64 use = term_val(term);

      char fun[32];
      aot_emit_tmp(fun, sizeof(fun), "fun", tmp);
      aot_emit_expr(f, use + 0, sub, fun, pad, tmp);

      fprintf(f, "%sTerm %s = term_new_use(%s);\n", pad, out, fun);
      return;
    }
    case VAR:
    case BJV: {
      u32 lvl = (u32)term_val(term);
      if (lvl == 0 || lvl > sub.len) {
        fprintf(f, "%sTerm %s = ", pad, out);
        aot_emit_fallback_expr(f, loc, sub);
        fprintf(f, ";\n");
        return;
      }

      fprintf(f, "%sTerm %s = x%u;\n", pad, out, lvl - 1);
      return;
    }
    case DP0:
    case BJ0: {
      u32 lvl = (u32)term_val(term);
      if (lvl == 0 || lvl > sub.len) {
        fprintf(f, "%sTerm %s = ", pad, out);
        aot_emit_fallback_expr(f, loc, sub);
        fprintf(f, ";\n");
        return;
      }

      u32 idx = lvl - 1;
      if (sub.kind[idx] != AOT_SUB_DUP) {
        fprintf(f, "%sTerm %s = ", pad, out);
        aot_emit_fallback_expr(f, loc, sub);
        fprintf(f, ";\n");
        return;
      }
      fprintf(f, "%sTerm %s = d%u.dp0;\n", pad, out, idx);
      return;
    }
    case DP1:
    case BJ1: {
      u32 lvl = (u32)term_val(term);
      if (lvl == 0 || lvl > sub.len) {
        fprintf(f, "%sTerm %s = ", pad, out);
        aot_emit_fallback_expr(f, loc, sub);
        fprintf(f, ";\n");
        return;
      }

      u32 idx = lvl - 1;
      if (sub.kind[idx] != AOT_SUB_DUP) {
        fprintf(f, "%sTerm %s = ", pad, out);
        aot_emit_fallback_expr(f, loc, sub);
        fprintf(f, ";\n");
        return;
      }
      fprintf(f, "%sTerm %s = d%u.dp1;\n", pad, out, idx);
      return;
    }
    default: {
      fprintf(f, "%sTerm %s = ", pad, out);
      aot_emit_fallback_expr(f, loc, sub);
      fprintf(f, ";\n");
      return;
    }
  }
}

// Emits one loop-specialized add-like definition.
fn void aot_emit_def_loop_add(FILE *f, u32 id, AotLoopAdd loop) {
  char *name = table_get(id);
  if (name == NULL) {
    return;
  }

  char fun_name[256];
  aot_emit_fun_name(fun_name, sizeof(fun_name), name);
  char zero_tok[320];
  char succ_tok[320];
  aot_emit_const_name(zero_tok, sizeof(zero_tok), "C", loop.zero_id);
  aot_emit_const_name(succ_tok, sizeof(succ_tok), "C", loop.succ_id);

  if (loop.tail_acc) {
    fprintf(f,
      "// Direct helper for @%s with 2 args.\n"
      "static Term %s_2(Term a_0, Term b_0) {\n"
      "add_loop_2:\n"
      "  a_0 = aot_force(a_0);\n"
      "  switch (term_tag(a_0)) {\n"
      "    case C00: case C01: case C02: case C03: case C04:\n"
      "    case C05: case C06: case C07: case C08: case C09:\n"
      "    case C10: case C11: case C12: case C13: case C14:\n"
      "    case C15: case C16: {\n"
      "      switch (term_ext(a_0)) {\n"
      "        case %s: {\n"
      "          aot_itrs_add(2);\n"
      "          return b_0;\n"
      "        }\n"
      "        case %s: {\n"
      "          aot_itrs_add(4);\n"
      "          a_0 = heap_read(term_val(a_0) + 0);\n"
      "          b_0 = aot_wrap_ctr1(%s, 1, b_0);\n"
      "          goto add_loop_2;\n"
      "        }\n"
      "        default: {\n"
      "          aot_itrs_add(2);\n"
      "          Term ret_0 = term_new_app(term_new_num(0), b_0);\n"
      "          return ret_0;\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    default: {\n"
      "      Term fun_0 = aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "      Term app_0 = term_new_app(fun_0, a_0);\n"
      "      Term ret_0 = term_new_app(app_0, b_0);\n"
      "      return ret_0;\n"
      "    }\n"
      "  }\n"
      "}\n\n",
      name,
      fun_name,
      zero_tok,
      succ_tok,
      succ_tok,
      (unsigned long long)loop.root_loc);

    fprintf(f,
      "// Compiled function for @%s (id %u).\n"
      "static Term %s(Term *stack, u32 *sp, u32 sb) {\n"
      "  Term a_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "  if (!a_0) {\n"
      "    return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "  }\n"
      "  a_0 = aot_force(a_0);\n"
      "  switch (term_tag(a_0)) {\n"
      "    case C00: case C01: case C02: case C03: case C04:\n"
      "    case C05: case C06: case C07: case C08: case C09:\n"
      "    case C10: case C11: case C12: case C13: case C14:\n"
      "    case C15: case C16: {\n"
      "      switch (term_ext(a_0)) {\n"
      "        case %s: {\n"
      "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "          if (!b_0) {\n"
      "            aot_itrs_inc();\n"
      "            return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "          }\n"
      "          aot_itrs_add(2);\n"
      "          return b_0;\n"
      "        }\n"
      "        case %s: {\n"
      "          Term a_1 = heap_read(term_val(a_0) + 0);\n"
      "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "          if (!b_0) {\n"
      "            aot_itrs_add(3);\n"
      "            u64 ls0 = heap_alloc(2);\n"
      "            heap_set(ls0 + 0, term_sub_set(a_1, 1));\n"
      "            heap_set(ls0 + 1, term_new(0, NUM, 0, 0ULL));\n"
      "            return aot_fallback_alo_ls(%lluULL, 1, ls0);\n"
      "          }\n"
      "          return %s_2(a_0, b_0);\n"
      "        }\n"
      "        default: {\n"
      "          aot_itrs_add(2);\n"
      "          return term_new_num(0);\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    default: {\n"
      "      return term_new_app(aot_fallback_alo_ls(%lluULL, 0, 0ULL), a_0);\n"
      "    }\n"
      "  }\n"
      "}\n\n",
      name,
      id,
      fun_name,
      (unsigned long long)loop.root_loc,
      zero_tok,
      (unsigned long long)loop.zero_lam_loc,
      succ_tok,
      (unsigned long long)loop.succ_lam_loc,
      fun_name,
      (unsigned long long)loop.root_loc);
    return;
  }

  fprintf(f,
    "// Direct helper for @%s with 2 args.\n"
    "static Term %s_2(Term a_0, Term b_0) {\n"
    "  u32 n_0 = 0;\n"
    "  a_0 = aot_force(a_0);\n"
    "  switch (term_tag(a_0)) {\n"
    "    case C00: case C01: case C02: case C03: case C04:\n"
    "    case C05: case C06: case C07: case C08: case C09:\n"
    "    case C10: case C11: case C12: case C13: case C14:\n"
    "    case C15: case C16: {\n"
    "      switch (term_ext(a_0)) {\n"
    "        case %s: {\n"
    "          aot_itrs_add(2);\n"
    "          return b_0;\n"
    "        }\n"
    "        case %s: {\n"
    "          a_0 = heap_read(term_val(a_0) + 0);\n"
    "          n_0 = 1;\n"
    "add_loop_2:\n"
    "          a_0 = aot_force(a_0);\n"
    "          switch (term_tag(a_0)) {\n"
    "            case C00: case C01: case C02: case C03: case C04:\n"
    "            case C05: case C06: case C07: case C08: case C09:\n"
    "            case C10: case C11: case C12: case C13: case C14:\n"
    "            case C15: case C16: {\n"
    "              switch (term_ext(a_0)) {\n"
    "                case %s: {\n"
    "                  aot_itrs_add(4ULL * (u64)n_0 + 2ULL);\n"
    "                  return aot_wrap_ctr1(%s, n_0, b_0);\n"
    "                }\n"
    "                case %s: {\n"
    "                  a_0 = heap_read(term_val(a_0) + 0);\n"
    "                  n_0 = n_0 + 1;\n"
    "                  goto add_loop_2;\n"
    "                }\n"
    "                default: {\n"
    "                  aot_itrs_add(4ULL * (u64)n_0 + 2ULL);\n"
    "                  Term ret_0 = term_new_app(term_new_num(0), b_0);\n"
    "                  return aot_wrap_ctr1(%s, n_0, ret_0);\n"
    "                }\n"
    "              }\n"
    "            }\n"
    "            default: {\n"
    "              aot_itrs_add(4ULL * (u64)n_0);\n"
    "              Term fun_0 = aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
    "              Term app_0 = term_new_app(fun_0, a_0);\n"
    "              Term ret_0 = term_new_app(app_0, b_0);\n"
    "              return aot_wrap_ctr1(%s, n_0, ret_0);\n"
    "            }\n"
    "          }\n"
    "        }\n"
    "        default: {\n"
    "          aot_itrs_add(2);\n"
    "          return term_new_app(term_new_num(0), b_0);\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    default: {\n"
    "      Term fun_0 = aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
    "      Term app_0 = term_new_app(fun_0, a_0);\n"
    "      Term ret_0 = term_new_app(app_0, b_0);\n"
    "      return ret_0;\n"
    "    }\n"
    "  }\n"
    "}\n\n",
    name,
    fun_name,
    zero_tok,
    succ_tok,
    zero_tok,
    succ_tok,
    succ_tok,
    succ_tok,
    (unsigned long long)loop.root_loc,
    succ_tok,
    (unsigned long long)loop.root_loc);

  fprintf(f,
    "// Compiled function for @%s (id %u).\n"
    "static Term %s(Term *stack, u32 *sp, u32 sb) {\n"
    "  Term a_0 = aot_pop_app_arg(stack, sp, sb);\n"
    "  if (!a_0) {\n"
    "    return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
    "  }\n"
    "  a_0 = aot_force(a_0);\n"
    "  switch (term_tag(a_0)) {\n"
    "    case C00: case C01: case C02: case C03: case C04:\n"
    "    case C05: case C06: case C07: case C08: case C09:\n"
    "    case C10: case C11: case C12: case C13: case C14:\n"
    "    case C15: case C16: {\n"
    "      switch (term_ext(a_0)) {\n"
    "        case %s: {\n"
    "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
    "          if (!b_0) {\n"
    "            aot_itrs_inc();\n"
    "            return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
    "          }\n"
    "          return %s_2(a_0, b_0);\n"
    "        }\n"
    "        case %s: {\n"
    "          Term a_1 = heap_read(term_val(a_0) + 0);\n"
    "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
    "          if (!b_0) {\n"
    "            aot_itrs_add(3);\n"
    "            u64 ls0 = heap_alloc(2);\n"
    "            heap_set(ls0 + 0, term_sub_set(a_1, 1));\n"
    "            heap_set(ls0 + 1, term_new(0, NUM, 0, 0ULL));\n"
    "            return aot_fallback_alo_ls(%lluULL, 1, ls0);\n"
    "          }\n"
    "          return %s_2(a_0, b_0);\n"
    "        }\n"
    "        default: {\n"
    "          aot_itrs_add(2);\n"
    "          return term_new_num(0);\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    default: {\n"
    "      return term_new_app(aot_fallback_alo_ls(%lluULL, 0, 0ULL), a_0);\n"
    "    }\n"
    "  }\n"
    "}\n\n",
    name,
    id,
    fun_name,
    (unsigned long long)loop.root_loc,
    zero_tok,
    (unsigned long long)loop.zero_lam_loc,
    fun_name,
    succ_tok,
    (unsigned long long)loop.succ_lam_loc,
    fun_name,
    (unsigned long long)loop.root_loc);
}

// Emits one loop-specialized nat-to-u32 definition.
fn void aot_emit_def_loop_u32(FILE *f, u32 id, AotLoopU32 loop) {
  char *name = table_get(id);
  if (name == NULL) {
    return;
  }

  char fun_name[256];
  aot_emit_fun_name(fun_name, sizeof(fun_name), name);
  char zero_tok[320];
  char succ_tok[320];
  aot_emit_const_name(zero_tok, sizeof(zero_tok), "C", loop.zero_id);
  aot_emit_const_name(succ_tok, sizeof(succ_tok), "C", loop.succ_id);

  if (loop.tail_acc) {
    fprintf(f,
      "// Direct helper for @%s with 2 args.\n"
      "static Term %s_2(Term a_0, Term b_0) {\n"
      "u32_loop_2:\n"
      "  a_0 = aot_force(a_0);\n"
      "  switch (term_tag(a_0)) {\n"
      "    case C00: case C01: case C02: case C03: case C04:\n"
      "    case C05: case C06: case C07: case C08: case C09:\n"
      "    case C10: case C11: case C12: case C13: case C14:\n"
      "    case C15: case C16: {\n"
      "      switch (term_ext(a_0)) {\n"
      "        case %s: {\n"
      "          aot_itrs_add(2);\n"
      "          return b_0;\n"
      "        }\n"
      "        case %s: {\n"
      "          aot_itrs_add(4);\n"
      "          Term x_0 = term_new_num((u32)1ULL);\n"
      "          Term y_0 = b_0;\n"
      "          switch (term_tag(x_0)) {\n"
      "            case NUM: {\n"
      "              switch (term_tag(y_0)) {\n"
      "                case NUM: {\n"
      "                  aot_itrs_inc();\n"
      "                  b_0 = term_new_num(term_op2_u32(0, (u32)term_val(x_0), (u32)term_val(y_0)));\n"
      "                  break;\n"
      "                }\n"
      "                default: {\n"
      "                  b_0 = term_new_op2(0, x_0, y_0);\n"
      "                  break;\n"
      "                }\n"
      "              }\n"
      "              break;\n"
      "            }\n"
      "            default: {\n"
      "              b_0 = term_new_op2(0, x_0, y_0);\n"
      "              break;\n"
      "            }\n"
      "          }\n"
      "          a_0 = heap_read(term_val(a_0) + 0);\n"
      "          goto u32_loop_2;\n"
      "        }\n"
      "        default: {\n"
      "          aot_itrs_add(2);\n"
      "          Term ret_0 = term_new_app(term_new_num(0), b_0);\n"
      "          return ret_0;\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    default: {\n"
      "      Term fun_0 = aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "      Term app_0 = term_new_app(fun_0, a_0);\n"
      "      Term ret_0 = term_new_app(app_0, b_0);\n"
      "      return ret_0;\n"
      "    }\n"
      "  }\n"
      "}\n\n",
      name,
      fun_name,
      zero_tok,
      succ_tok,
      (unsigned long long)loop.root_loc);

    fprintf(f,
      "// Compiled function for @%s (id %u).\n"
      "static Term %s(Term *stack, u32 *sp, u32 sb) {\n"
      "  Term a_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "  if (!a_0) {\n"
      "    return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "  }\n"
      "  a_0 = aot_force(a_0);\n"
      "  switch (term_tag(a_0)) {\n"
      "    case C00: case C01: case C02: case C03: case C04:\n"
      "    case C05: case C06: case C07: case C08: case C09:\n"
      "    case C10: case C11: case C12: case C13: case C14:\n"
      "    case C15: case C16: {\n"
      "      switch (term_ext(a_0)) {\n"
      "        case %s: {\n"
      "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "          if (!b_0) {\n"
      "            aot_itrs_inc();\n"
      "            return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
      "          }\n"
      "          aot_itrs_add(2);\n"
      "          return b_0;\n"
      "        }\n"
      "        case %s: {\n"
      "          Term a_1 = heap_read(term_val(a_0) + 0);\n"
      "          Term b_0 = aot_pop_app_arg(stack, sp, sb);\n"
      "          if (!b_0) {\n"
      "            aot_itrs_add(3);\n"
      "            u64 ls0 = heap_alloc(2);\n"
      "            heap_set(ls0 + 0, term_sub_set(a_1, 1));\n"
      "            heap_set(ls0 + 1, term_new(0, NUM, 0, 0ULL));\n"
      "            return aot_fallback_alo_ls(%lluULL, 1, ls0);\n"
      "          }\n"
      "          return %s_2(a_0, b_0);\n"
      "        }\n"
      "        default: {\n"
      "          aot_itrs_add(2);\n"
      "          return term_new_num(0);\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    default: {\n"
      "      return term_new_app(aot_fallback_alo_ls(%lluULL, 0, 0ULL), a_0);\n"
      "    }\n"
      "  }\n"
      "}\n\n",
      name,
      id,
      fun_name,
      (unsigned long long)loop.root_loc,
      zero_tok,
      (unsigned long long)loop.zero_lam_loc,
      succ_tok,
      (unsigned long long)loop.succ_lam_loc,
      fun_name,
      (unsigned long long)loop.root_loc);
    return;
  }

  fprintf(f,
    "// Compiled function for @%s (id %u).\n"
    "static Term %s(Term *stack, u32 *sp, u32 sb) {\n"
    "  u32 n_0 = 0;\n"
    "  Term a_0 = aot_pop_app_arg(stack, sp, sb);\n"
    "  if (!a_0) {\n"
    "    return aot_fallback_alo_ls(%lluULL, 0, 0ULL);\n"
    "  }\n"
    "u32_loop:\n"
    "  a_0 = aot_force(a_0);\n"
    "  switch (term_tag(a_0)) {\n"
    "    case C00: case C01: case C02: case C03: case C04:\n"
    "    case C05: case C06: case C07: case C08: case C09:\n"
    "    case C10: case C11: case C12: case C13: case C14:\n"
    "    case C15: case C16: {\n"
    "      switch (term_ext(a_0)) {\n"
    "        case %s: {\n"
    "          aot_itrs_add(4ULL * (u64)n_0 + 1ULL);\n"
    "          return term_new_num(n_0);\n"
    "        }\n"
    "        case %s: {\n"
    "          a_0 = heap_read(term_val(a_0) + 0);\n"
    "          n_0 = n_0 + 1;\n"
    "          goto u32_loop;\n"
    "        }\n"
    "        default: {\n"
    "          aot_itrs_add(4ULL * (u64)n_0 + 2ULL);\n"
    "          return term_new_num(n_0);\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    default: {\n"
    "      aot_itrs_add(3ULL * (u64)n_0);\n"
    "      Term ret_0 = term_new_app(aot_fallback_alo_ls(%lluULL, 0, 0ULL), a_0);\n"
    "      return aot_wrap_op2_num_lhs(0, 1, n_0, ret_0);\n"
    "    }\n"
    "  }\n"
    "}\n\n",
    name,
    id,
    fun_name,
    (unsigned long long)loop.root_loc,
    zero_tok,
    succ_tok,
    (unsigned long long)loop.root_loc);
}

// Emits one direct unary MAT helper.
fn void aot_emit_def_direct_mat1(FILE *f, u32 id) {
  if (!aot_emit_has_direct_mat1(id)) {
    return;
  }

  char *name = table_get(id);
  if (name == NULL) {
    return;
  }

  char fun_name[256];
  aot_emit_fun_name(fun_name, sizeof(fun_name), name);

  fprintf(f, "// Direct helper for @%s with 1 arg.\n", name);
  fprintf(f, "static Term %s_1(Term x_0, Term *stack, u32 *sp, u32 sb) {\n", fun_name);

  AotSubst sub = {0};
  sub.self_id = id;
  u32 tmp = 0;
  aot_emit_spine_mat_arg(f, BOOK[id], sub, "  ", &tmp, "x_0");

  fprintf(f, "}\n\n");
}

// Emits one forward declaration for a compiled definition.
fn void aot_emit_decl(FILE *f, u32 id) {
  if (BOOK[id] == 0) {
    return;
  }

  char *name = table_get(id);
  if (name == NULL) {
    return;
  }

  char fun_name[256];
  aot_emit_fun_name(fun_name, sizeof(fun_name), name);
  AotLoopAdd loop_add;
  AotLoopU32 loop_u32;
  if (aot_match_loop_add(BOOK[id], id, &loop_add)) {
    fprintf(f, "static Term %s_2(Term a_0, Term b_0);\n", fun_name);
  }
  if (aot_match_loop_u32(BOOK[id], id, &loop_u32) && loop_u32.tail_acc) {
    fprintf(f, "static Term %s_2(Term a_0, Term b_0);\n", fun_name);
  }
  if (aot_emit_has_direct_mat1(id)) {
    fprintf(f, "static Term %s_1(Term x_0, Term *stack, u32 *sp, u32 sb);\n", fun_name);
  }
  fprintf(f, "static Term %s(Term *stack, u32 *sp, u32 sb);\n", fun_name);
}

// Emits one compiled definition.
fn void aot_emit_def(FILE *f, u32 id) {
  if (BOOK[id] == 0) {
    return;
  }

  char *name = table_get(id);
  if (name == NULL) {
    return;
  }

  u64  root = BOOK[id];
  char fun_name[256];
  aot_emit_fun_name(fun_name, sizeof(fun_name), name);

  AotLoopAdd loop_add;
  if (aot_match_loop_add(root, id, &loop_add)) {
    aot_emit_def_loop_add(f, id, loop_add);
    return;
  }

  AotLoopU32 loop_u32;
  if (aot_match_loop_u32(root, id, &loop_u32)) {
    aot_emit_def_loop_u32(f, id, loop_u32);
    return;
  }

  aot_emit_def_direct_mat1(f, id);

  fprintf(f, "// Compiled function for @%s (id %u).\n", name, id);
  fprintf(f, "static Term %s(Term *stack, u32 *sp, u32 sb) {\n", fun_name);

  AotSubst sub = {0};
  sub.self_id = id;
  u32 tmp = 0;
  aot_emit_spine(f, root, sub, "  ", &tmp);

  fprintf(f, "}\n\n");
}

// Emits registration for all compiled definitions.
fn void aot_emit_register(FILE *f) {
  fprintf(f, "// Registers generated functions into the runtime table.\n");
  fprintf(f, "static void aot_register_generated(void) {\n");
  for (u32 id = 0; id < TABLE.len; id++) {
    if (BOOK[id] == 0) {
      continue;
    }

    char *name = table_get(id);
    if (name == NULL) {
      continue;
    }

    char fun_name[256];
    aot_emit_fun_name(fun_name, sizeof(fun_name), name);

    fprintf(f, "  // @%s\n", name);
    fprintf(f, "  AOT_FNS[%u] = %s;\n", id, fun_name);
  }
  fprintf(f, "}\n\n");
}

// Emits the static FFI-load table used by standalone AOT programs.
fn void aot_emit_ffi_table(FILE *f, const AotBuildCfg *cfg) {
  u32 ffi_len = cfg ? cfg->ffi_len : 0;
  u32 ffi_cap = ffi_len == 0 ? 1 : ffi_len;

  fprintf(f, "static const RuntimeFfiLoad AOT_FFI_LOADS[%u] = {\n", ffi_cap);
  if (ffi_len == 0) {
    fprintf(f, "  { .is_dir = 0, .path = NULL },\n");
  } else {
    for (u32 i = 0; i < ffi_len; i++) {
      fprintf(f, "  { .is_dir = %d, .path = ", cfg->ffi[i].is_dir);
      aot_emit_c_string_token(f, cfg->ffi[i].path ? cfg->ffi[i].path : "");
      fprintf(f, " },\n");
    }
  }
  fprintf(f, "};\n");
  fprintf(f, "static const u32 AOT_FFI_LEN = %u;\n\n", ffi_len);
}

// Emits standalone entrypoint using shared runtime helper functions.
fn void aot_emit_entry_main(FILE *f, const AotBuildCfg *cfg) {
  u32 threads = (cfg && cfg->threads > 0) ? cfg->threads : 1;
  int debug   = cfg ? cfg->debug : 0;

  RuntimeEvalCfg eval_cfg = {
    .do_collapse    = 0,
    .collapse_limit = -1,
    .stats          = 0,
    .silent         = 0,
    .step_by_step   = 0,
  };

  if (cfg != NULL) {
    eval_cfg = cfg->eval;
  }

  fprintf(f, "int main(void) {\n");
  fprintf(f, "  runtime_init(%u, %d, %d, %d);\n", threads, debug, eval_cfg.silent, eval_cfg.step_by_step);
  fprintf(f, "  runtime_load_ffi(AOT_FFI_LOADS, AOT_FFI_LEN, 0);\n");
  fprintf(f, "\n");
  fprintf(f, "  u32 main_id = 0;\n");
  fprintf(f, "  if (!runtime_prepare_text(&main_id, AOT_SOURCE_PATH, AOT_SOURCE_TEXT)) {\n");
  fprintf(f, "    runtime_free();\n");
  fprintf(f, "    return 1;\n");
  fprintf(f, "  }\n");
  fprintf(f, "\n");
  fprintf(f, "  aot_register_generated();\n");
  fprintf(f, "  RuntimeEvalCfg eval = {\n");
  fprintf(f, "    .do_collapse = %d,\n", eval_cfg.do_collapse);
  fprintf(f, "    .collapse_limit = %d,\n", eval_cfg.collapse_limit);
  fprintf(f, "    .stats = %d,\n", eval_cfg.stats);
  fprintf(f, "    .silent = %d,\n", eval_cfg.silent);
  fprintf(f, "    .step_by_step = %d,\n", eval_cfg.step_by_step);
  fprintf(f, "  };\n");
  fprintf(f, "  runtime_eval_main(main_id, &eval);\n");
  fprintf(f, "  runtime_free();\n");
  fprintf(f, "  return 0;\n");
  fprintf(f, "}\n");
}

// Emits the full standalone AOT C program.
fn void aot_emit_to_file(FILE *f, const char *runtime_path, const char *src_path, const char *src_text, const AotBuildCfg *cfg) {
  AOT_EMIT_ITRS = aot_emit_counting(cfg);

  fprintf(f, "// Auto-generated by HVM AOT.\n");
  fprintf(f, "// This file is standalone: compile with `clang -O2 -o <out> <file.c>`.\n");
  fprintf(f, "//\n");
  fprintf(f, "// AOT summary:\n");
  fprintf(f, "// - Includes full runtime TU directly (%s).\n", runtime_path);
  fprintf(f, "// - Emits one case-tree function per definition.\n");
  fprintf(f, "// - Separates spine interactions from tail expression building.\n");
  fprintf(f, "// - Deopts by rebuilding ALO with LAM/DUP-aware substitution entries.\n\n");

  fprintf(f, "#include ");
  aot_emit_c_string_token(f, runtime_path);
  fprintf(f, "\n\n");

  aot_emit_c_string_decl(f, "AOT_SOURCE_PATH", src_path);
  aot_emit_c_string_decl(f, "AOT_SOURCE_TEXT", src_text);
  aot_emit_ffi_table(f, cfg);
  aot_emit_named_consts(f);

  fprintf(f, "// Forward Declarations\n");
  fprintf(f, "// --------------------\n\n");
  for (u32 id = 0; id < TABLE.len; id++) {
    aot_emit_decl(f, id);
  }
  fprintf(f, "\n");

  for (u32 id = 0; id < TABLE.len; id++) {
    if (BOOK[id] == 0) {
      continue;
    }
    aot_emit_def(f, id);
  }

  aot_emit_register(f);
  aot_emit_entry_main(f, cfg);
}

// Emits the full standalone AOT C program to a file path.
fn void aot_emit(const char *c_path, const char *runtime_path, const char *src_path, const char *src_text, const AotBuildCfg *cfg) {
  FILE *f = fopen(c_path, "w");
  if (f == NULL) {
    fprintf(stderr, "ERROR: failed to open AOT output '%s'\n", c_path);
    exit(1);
  }

  aot_emit_to_file(f, runtime_path, src_path, src_text, cfg);
  fclose(f);
}

// Emits the full standalone AOT C program to stdout.
fn void aot_emit_stdout(const char *runtime_path, const char *src_path, const char *src_text, const AotBuildCfg *cfg) {
  aot_emit_to_file(stdout, runtime_path, src_path, src_text, cfg);
}
