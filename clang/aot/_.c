// HVM AOT Runtime Core
// ====================
//
// This module exposes the tiny runtime surface used by generated AOT code.
//
// Model:
// - Each compiled definition is one stack-entry function: `F_<name>`.
// - Generated code is direct tree-shaped C with explicit fallback points.
// - Deopt returns residual runtime terms (ALO/app chains), never rewinds state.

// Type
// ----

typedef Term (*HvmAotFn)(Term *stack, u32 *s_pos, u32 base);

// Build-time runtime config embedded into generated AOT executables.
typedef struct {
  u32            threads;
  int            debug;
  RuntimeEvalCfg eval;
  u32            ffi_len;
  RuntimeFfiLoad ffi[RUNTIME_FFI_MAX];
} AotBuildCfg;

// Duplication pair used by generated code.
typedef struct {
  Term dp0;
  Term dp1;
} Dups;

fn Term wnf(Term term);

// Runtime
// -------

// Per-definition compiled entrypoint table (BOOK id -> native function).
static HvmAotFn AOT_FNS[BOOK_CAP] = {0};

// AOT runtime limits.
#define AOT_SUB_CAP   16
#define AOT_MAX_DEPTH 4096

// Thread-local compiled-call depth for recursion cutoff.
static _Thread_local u32 AOT_CALL_DEPTH = 0;

// Counts one interaction on compiled paths.
fn void aot_itrs_inc(void) {
  if (ITRS_ENABLED) {
    ITRS++;
  }
}

// Adds a known number of interactions on compiled paths.
fn void aot_itrs_add(u64 amount) {
  if (ITRS_ENABLED) {
    ITRS += amount;
  }
}

// Fallback
// --------

// Rebuilds one ALO node from an existing substitution-list head.
fn Term aot_fallback_alo_ls(u64 tm_loc, u16 len, u64 ls_loc) {
  if (len == 0) {
    return term_new(0, ALO, 0, tm_loc);
  }
  return term_new_alo(ls_loc, len, tm_loc);
}

// Reduces one term to WNF using the caller's current stack position.
fn Term aot_eval(Term term, u32 *s_pos) {
  WNF_S_POS = *s_pos;
  Term out  = wnf(term);
  *s_pos    = WNF_S_POS;
  return out;
}

// Pops one APP frame and returns its argument.
// Returns 0 when the stack top is not an APP frame.
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

// Pushes one synthetic APP frame carrying `arg`.
// Runtime-errors on stack overflow.
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

// Pushes all constructor fields as APP arguments (left-to-right).
// Contract: `ctr` is already known to be a constructor.
// Runtime-errors on stack overflow.
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

// Calls
// -----

// Returns current compiled recursion depth.
fn u32 aot_call_depth(void) {
  return AOT_CALL_DEPTH;
}

// Calls one known compiled function pointer with depth guard.
fn Term aot_call_direct(HvmAotFn fun, u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  if (AOT_CALL_DEPTH >= AOT_MAX_DEPTH) {
    return term_new_ref(ref_id);
  }

  AOT_CALL_DEPTH++;
  Term out = fun(stack, s_pos, base);
  AOT_CALL_DEPTH--;

  return out;
}

// Calls one compiled ref using current stack slice, else returns residual REF application.
fn Term aot_call_ref(u32 ref_id, Term *stack, u32 *s_pos, u32 base) {
  if (AOT_CALL_DEPTH >= AOT_MAX_DEPTH) {
    return term_new_ref(ref_id);
  }

  HvmAotFn fun = AOT_FNS[ref_id];
  if (fun == NULL) {
    return term_new_ref(ref_id);
  }

  AOT_CALL_DEPTH++;
  Term out = fun(stack, s_pos, base);
  AOT_CALL_DEPTH--;

  return out;
}

// Dispatch
// --------

// Tries to execute a compiled function for a REF; returns 0 when absent.
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

// Utils
// -----

// Converts a symbol name into a filesystem-safe alnum/_ identifier.
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
