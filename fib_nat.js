// fib_nat.js
// ==========
//
// Direct JavaScript translation of `fib_nat.hvm`.
// Uses Peano constructors as plain objects.

// Constructors
// ------------

// Builds `#ZER{}`
function ZER() {
  return { $: "ZER" };
}

// Builds `#SUC{pred}`
function SUC(pred) {
  return { $: "SUC", pred };
}

// Utils
// -----

// Builds one Peano nat from one JS number
function nat(n) {
  var out = ZER();
  while (n > 0) {
    out = SUC(out);
    n = n - 1;
  }
  return out;
}

// Program
// -------

// Translates `@add`
function add(a, b) {
  while (true) {
    switch (a.$) {
      case "ZER": {
        return b;
      }
      case "SUC": {
        var a = a.pred;
        var b = SUC(b);
        continue;
      }
      default: {
        throw new Error("invalid nat");
      }
    }
  }
}

// Translates `@fib`
function fib(n) {
  switch (n.$) {
    case "ZER": {
      return ZER();
    }
    case "SUC": {
      var n = n.pred;
      switch (n.$) {
        case "ZER": {
          return SUC(ZER());
        }
        case "SUC": {
          var p   = n.pred;
          var p_0 = p;
          var p_1 = p;
          return add(fib(SUC(p_0)), fib(p_1));
        }
        default: {
          throw new Error("invalid nat");
        }
      }
    }
    default: {
      throw new Error("invalid nat");
    }
  }
}

// Translates `@u32`
function u32(n, acc) {
  while (true) {
    switch (n.$) {
      case "ZER": {
        return acc;
      }
      case "SUC": {
        var n   = n.pred;
        var acc = 1 + acc;
        continue;
      }
      default: {
        throw new Error("invalid nat");
      }
    }
  }
}

// Main
// ----

// Runs `@main`
function main() {
  var out = u32(fib(nat(34)), 0);
  console.log(out);
}

main();
