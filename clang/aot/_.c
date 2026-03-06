// HVM AOT Runtime
// ===============
//
// Tiny runtime surface used by generated AOT code.

typedef Term (*HvmAotFn)(Term *stack, u32 *s_pos, u32 base);

typedef struct {
  u32            threads;
  int            debug;
  RuntimeEvalCfg eval;
  u32            ffi_len;
  RuntimeFfiLoad ffi[RUNTIME_FFI_MAX];
} AotBuildCfg;

typedef struct {
  Term dp0;
  Term dp1;
} Dups;

fn Term wnf(Term term);
fn Term wnf_dup_nam(u32 lab, u64 loc, u8 side, Term nam);
fn Term wnf_dup_lam(u32 lab, u64 loc, u8 side, Term lam);
fn Term wnf_dup_sup(u32 lab, u64 loc, u8 side, Term sup);
fn Term wnf_dup_nod(u32 lab, u64 loc, u8 side, Term term);

static HvmAotFn AOT_FNS[BOOK_CAP] = {0};

#define AOT_SUB_CAP   16
#define AOT_MAX_DEPTH 4096

static _Thread_local u32 AOT_CALL_DEPTH = 0;

// Counts one interaction.
fn void aot_itrs_inc(void) {
  if (ITRS_ENABLED) {
    ITRS++;
  }
}

// Adds interactions.
fn void aot_itrs_add(u64 amount) {
  if (ITRS_ENABLED) {
    ITRS += amount;
  }
}

// Rebuilds one ALO.
fn Term aot_fallback_alo_ls(u64 tm_loc, u16 len, u64 ls_loc) {
  if (len == 0) {
    return term_new(0, ALO, 0, tm_loc);
  }
  return term_new_alo(ls_loc, len, tm_loc);
}

// Rebuilds one REF app.
fn Term aot_fallback_ref(u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  Term out = term_new_ref(ref_id);
  while (*s_pos > base) {
    Term frm = stack[--(*s_pos)];
    if (term_tag(frm) != APP) {
      sys_runtime_error("AOT REF fallback saw a non-APP frame");
    }
    u64  app = term_val(frm);
    Term arg = heap_read(app + 1);
    out = term_new_app(out, arg);
  }
  return out;
}

// Closes one APP slice.
fn Term aot_close_apps(Term out, Term *stack, u32 *s_pos, u32 base) {
  while (*s_pos > base) {
    Term frm = stack[--(*s_pos)];
    if (term_tag(frm) != APP) {
      sys_runtime_error("AOT expr call saw a non-APP frame");
    }
    u64  app = term_val(frm);
    Term arg = heap_read(app + 1);
    out = term_new_app(out, arg);
  }
  return out;
}

// Wraps unary ctrs.
fn Term aot_wrap_ctr1(u32 ctr_id, u32 reps, Term bod) {
  for (u32 i = 0; i < reps; i++) {
    Term args[1];
    args[0] = bod;
    bod = term_new_ctr(ctr_id, 1, args);
  }
  return bod;
}

// Wraps numeric OP2s.
fn Term aot_wrap_op2_num_lhs(u32 opr, u32 lhs, u32 reps, Term bod) {
  Term num = term_new_num(lhs);
  for (u32 i = 0; i < reps; i++) {
    bod = term_new_op2(opr, num, bod);
  }
  return bod;
}

// Runs WNF on one term.
fn Term aot_eval(Term term, u32 *s_pos) {
  WNF_S_POS = *s_pos;
  Term out  = wnf(term);
  *s_pos    = WNF_S_POS;
  return out;
}

// Forces one head fragment.
fn Term aot_force(Term term) {
  while (1) {
    switch (term_tag(term)) {
      case VAR: {
        u64  loc  = term_val(term);
        Term cell = heap_read(loc);
        if (!term_sub_get(cell)) {
          return term;
        }
        term = term_sub_set(cell, 0);
        continue;
      }
      case DP0:
      case DP1: {
        u8  side = term_tag(term) == DP0 ? 0 : 1;
        u64 loc  = term_val(term);
        u32 lab  = term_ext(term);
        Term val = heap_take(loc);
        if (term_sub_get(val)) {
          term = term_sub_set(val, 0);
          continue;
        }
        switch (term_tag(val)) {
          case VAR:
          case DP0:
          case DP1: {
            val = aot_force(val);
            switch (term_tag(val)) {
              case VAR:
              case DP0:
              case DP1: {
                heap_set(loc, val);
                return term;
              }
              default: {
                break;
              }
            }
            break;
          }
          default: {
            break;
          }
        }
        switch (term_tag(val)) {
          case NAM:
          case BJV:
          case BJ0:
          case BJ1: {
            term = wnf_dup_nam(lab, loc, side, val);
            continue;
          }
          case LAM: {
            term = wnf_dup_lam(lab, loc, side, val);
            continue;
          }
          case SUP: {
            term = wnf_dup_sup(lab, loc, side, val);
            continue;
          }
          case ERA:
          case ANY:
          case PRI:
          case NUM:
          case DRY:
          case MAT:
          case SWI:
          case USE:
          case INC:
          case OP2:
          case DSU:
          case DDU:
          case C00 ... C16: {
            term = wnf_dup_nod(lab, loc, side, val);
            continue;
          }
          default: {
            heap_set(loc, val);
            return term;
          }
        }
      }
      default: {
        return term;
      }
    }
  }
}

// Pops one APP arg.
fn Term aot_pop_app_arg(Term *stack, u32 *s_pos, u32 base) {
  if (*s_pos <= base) {
    return 0;
  }

  Term frm = stack[*s_pos - 1];
  if (term_tag(frm) != APP) {
    return 0;
  }

  (*s_pos)--;
  u64 app = term_val(frm);
  return heap_read(app + 1);
}

// Pushes one APP arg.
fn void aot_push_app_arg(Term *stack, u32 *s_pos, u32 base, Term arg) {
  (void)base;
  if (*s_pos == UINT32_MAX) {
    sys_runtime_error("AOT stack overflow while pushing application argument");
  }

  u64 app = heap_alloc(2);
  heap_set(app + 0, term_new_era());
  heap_set(app + 1, arg);
  stack[*s_pos] = term_new(0, APP, 0, app);
  (*s_pos)++;
}

// Pushes constructor fields.
fn void aot_push_fields(Term *stack, u32 *s_pos, u32 base, Term ctr) {
  (void)base;
  u8  tag = term_tag(ctr);
  u32 ari = (u32)(tag - C00);

  if ((u64)(*s_pos) + (u64)ari > (u64)UINT32_MAX) {
    sys_runtime_error("AOT stack overflow while pushing constructor fields");
  }

  u64 loc = term_val(ctr);
  for (u32 j = ari; j > 0; j--) {
    Term fld = heap_read(loc + (u64)(j - 1));
    aot_push_app_arg(stack, s_pos, base, fld);
  }
}

// Returns call depth.
fn u32 aot_call_depth(void) {
  return AOT_CALL_DEPTH;
}

// Calls one compiled fn.
fn Term aot_call_direct(HvmAotFn fun, u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  if (AOT_CALL_DEPTH >= AOT_MAX_DEPTH) {
    return aot_fallback_ref(ref_id, stack, s_pos, base);
  }

  AOT_CALL_DEPTH++;
  Term out = fun(stack, s_pos, base);
  AOT_CALL_DEPTH--;

  return out;
}

// Calls one compiled fn in expr position.
fn Term aot_call_expr(HvmAotFn fun, u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  Term out = aot_call_direct(fun, ref_id, stack, s_pos, base);
  return aot_close_apps(out, stack, s_pos, base);
}

// Calls one compiled ref.
fn Term aot_call_ref(u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  if (AOT_CALL_DEPTH >= AOT_MAX_DEPTH) {
    return aot_fallback_ref(ref_id, stack, s_pos, base);
  }

  HvmAotFn fun = AOT_FNS[ref_id];
  if (fun == NULL) {
    return aot_fallback_ref(ref_id, stack, s_pos, base);
  }

  AOT_CALL_DEPTH++;
  Term out = fun(stack, s_pos, base);
  AOT_CALL_DEPTH--;

  return out;
}

// Tries one compiled call.
fn int aot_try_call(u32 id, Term *stack, u32 *s_pos, u32 base, Term *out) {
  if (STEPS_ITRS_LIM != 0) {
    return 0;
  }

  if (id >= BOOK_CAP) {
    return 0;
  }

  HvmAotFn fun = AOT_FNS[id];
  if (fun == NULL) {
    return 0;
  }

  if (AOT_CALL_DEPTH >= AOT_MAX_DEPTH) {
    return 0;
  }

  AOT_CALL_DEPTH++;
  *out = fun(stack, s_pos, base);
  AOT_CALL_DEPTH--;
  return 1;
}

// Sanitizes one symbol.
fn char *aot_sanitize(const char *name) {
  size_t len = strlen(name);
  size_t cap = (len * 4) + 1;
  char *out  = malloc(cap);
  if (out == NULL) {
    return NULL;
  }

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    u8 c = (u8)name[i];
    if (isalnum(c) || c == '_') {
      out[j++] = (char)c;
      continue;
    }

    if (j + 4 >= cap) {
      free(out);
      return NULL;
    }

    static const char HEX[] = "0123456789ABCDEF";
    out[j++] = '_';
    out[j++] = 'x';
    out[j++] = HEX[(c >> 4) & 0xF];
    out[j++] = HEX[c & 0xF];
  }

  out[j] = '\0';
  return out;
}
