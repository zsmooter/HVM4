# HVM

HVM is a high-performance runtime for the [Interaction Calculus](docs/theory/interaction_calculus.md).

**NOTE: you're here before launch. Use at your own risk.**

## Building and Running

```bash
# Build
cd clang && clang -O2 -o main main.c
# On Linux, add: -ldl

# Run a file (use collapse mode by default)
./clang/main test/file.hvm -s -C10

# Run all tests
./test/_all_.sh

# Run benchmarks (from sibling bench repo)
cd ../bench && ./bench.ts --hvm-interpreted
```

Flags:
- `-s` shows performance stats
- `-D` prints each intermediate reduction step with interaction labels
- `-C10` collapses and flattens superpositions (limit to 10 lines)
- `--to-c` emits a standalone AOT C program to stdout
- `--as-c` emits + compiles + runs a standalone executable once
- `-o <path>`, `--output <path>` emits + compiles a standalone executable to a file path
- `--ffi <path>` loads one FFI shared library before parsing
- `--ffi-dir <path>` loads all FFI shared libraries in a directory before parsing

## Examples

```hvm
@main = ((@add 1) 2)
//3
```

```hvm
@main = (&{1, 2} + 10)
//11
//12
```

```hvm
@main = (! x &A= 3; (x₀ + x₁))
//6
```

## Documentation

- Theory: [docs/theory/interaction_calculus.md](docs/theory/interaction_calculus.md)
- Core language: [docs/hvm/core.md](docs/hvm/core.md)
- Memory layout: [docs/hvm/memory.md](docs/hvm/memory.md)
- Interaction rules: [docs/hvm/interactions/](docs/hvm/interactions/)
