// term/op2_u32.c
// ===============
//
// Applies one numeric OP2 code to two `u32` operands.

// OP2
// ---

// Computes one primitive numeric OP2 result.
fn u32 term_op2_u32(u32 opr, u32 a, u32 b) {
  if (__builtin_expect(opr == OP_SUB, 1)) {
    return a - b;
  }

  switch (opr) {
    case OP_ADD: {
      return a + b;
    }
    case OP_SUB: {
      return a - b;
    }
    case OP_MUL: {
      return a * b;
    }
    case OP_DIV: {
      return b != 0 ? a / b : 0;
    }
    case OP_MOD: {
      return b != 0 ? a % b : 0;
    }
    case OP_AND: {
      return a & b;
    }
    case OP_OR: {
      return a | b;
    }
    case OP_XOR: {
      return a ^ b;
    }
    case OP_LSH: {
      return a << b;
    }
    case OP_RSH: {
      return a >> b;
    }
    case OP_NOT: {
      return ~b;
    }
    case OP_EQ: {
      return a == b ? 1 : 0;
    }
    case OP_NE: {
      return a != b ? 1 : 0;
    }
    case OP_LT: {
      return a < b ? 1 : 0;
    }
    case OP_LE: {
      return a <= b ? 1 : 0;
    }
    case OP_GT: {
      return a > b ? 1 : 0;
    }
    case OP_GE: {
      return a >= b ? 1 : 0;
    }
    default: {
      return 0;
    }
  }
}
