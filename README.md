# GPC — a small C compiler for ARM64

GPC compiles a hand-picked subset of C straight to **ARM64 (AArch64) machine code**. It's written from scratch in C23 with no dependencies: no lexer generators, no LLVM, no external assembler. Just arenas, bitvectors, and a lot of quads.

This README is the reference for the language and the compiler. It describes what GPC compiles and how.

---

## The language

It's a small, honest C: enough to write real loops, pointer code and data structures, without the dark corners.

### Types
| | |
|---|---|
| `int` | 32-bit signed |
| `char` | 8-bit |
| `void` | function return type, and `void*` |
| Pointers | any depth (`int***`), via `&` and `*`; 8 bytes |
| Arrays | 1-D, fixed size (`int a[16];`); `a[i]` lowers to scaled pointer arithmetic |
| Structs | `struct name { ... };` with `.` and `->`, struct pointers, structs as members |

`int` and `char` convert implicitly. Explicit casts like `(char*)p` or `(struct node*)p` work too.

### Statements
- `if` / `else`
- `while`, `for (init; cond; step)`
- `switch` / `case` / `default`
- `break`, `continue`, `return expr;`
- Function definitions and calls, with up to 8 arguments in registers
- Global and local variables, with block scoping

### Operators
| Kind | Operators |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%`, unary `-` |
| Bitwise | `&` `\|` `^` `~` `<<` `>>` |
| Comparison | `<` `>` `<=` `>=` `==` `!=` |
| Logical | `&&` `\|\|` (short-circuiting), `!` |
| Assignment | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |
| Increment | `++` `--` |
| Pointer / member | `&` `*` `[]` `.` `->` |

Precedence and associativity follow C.

### Literals & comments
- Integers in decimal, hex (`0x1F`) or octal (`017`)
- Character literals: `'a'`, `'\n'`, `'\0'`
- String literals: `"hello"`, stored in read-only data as `char` arrays
- `// line` and `/* block */` comments

### Out of scope
Floats, unions, `typedef`, multi-dimensional arrays, varargs, and the preprocessor.

---

## How it works

```
source ─► tokens ─► AST ─► 3-address IR ─► CFG + liveness ─► register allocation ─► A64 machine code
```

**1. Lexing** (`frontend/lexer`)
A single pass over the source buffer produces tokens and skips comments. Keywords are found with a binary search over a sorted keyword table. Multi-character operators (`<<=`, `->`, `!=`, …) are combined from single-character tokens as they're read.

**2. Parsing** (`frontend/parser/astGen.c`)
Statements are parsed by recursive descent, and expressions by precedence climbing. Symbols are resolved while parsing, so every identifier node already points at its symbol, its type, and a virtual register. Struct definitions are recorded in a type table with member offsets and sizes, aligned C-style.

**3. Three-address IR** (`frontend/parser/3opIrGen.c`)
The AST is linearized into *quads* (`op, o1, o2, o3`) over an unlimited supply of virtual registers:
- `&&` and `||` become branches, so short-circuiting falls out naturally.
- `for` and `while` lower to the same label and branch shape.
- `switch` lowers to compare-and-jump chains.
- Compound assignments expand to `x = x op y`.
- Array indexing and `->` become explicit address math with `LOAD`/`STORE`.

**4. Register allocation** (`backend/regAllocator`)
A Chaitin–Briggs graph-colouring allocator:
- **CFG construction**: quads are split into basic blocks at labels and jumps, and the blocks are walked in post-order.
- **Liveness**: each block's use/def/live-in/live-out sets are fixed-width bitvectors, iterated to a fixed point.
- **Interference graph**: built from live ranges, then coloured with ARM64's general-purpose registers.
- **Spilling**: when colouring fails, the cheapest range is spilled and the whole pass reruns. Spill cost is weighted by loop depth (`8^depth`), so values in inner loops stay in registers.
- **ABI hints**: arguments and return values prefer `x0`–`x7`. Values live across a call are saved and restored around it.
- **Cleanup**: a peephole pass drops moves that became `xN = xN` after colouring.

**5. Address-taken variables**
Most variables live only in registers. Once a variable has its address taken (`&x`, or a struct that's accessed through a pointer), it gets a stack slot and uses **write-through**. It's reloaded before each read and stored after each write, so the register and memory copies always agree. It's simple and always correct, and a good baseline to optimize from.

**6. ARM64 binary generation** (`backend/codegen`)
GPC writes A64 machine code itself, with no assembler involved.
- **Frame layout**: each function gets a frame of locals, arrays, spill slots and address-taken slots. Callee-saved registers (`x19`–`x28`) and `fp`/`lr` are saved only when the function uses them, and the frame is 16-byte aligned.
  - The prologue is `stp x29, x30, [sp, #-N]!` and `mov x29, sp`. The epilogue mirrors it, then `ret`.
- **Lowering**: each quad becomes one or a few instructions.
  - Arithmetic maps directly: `add`, `sub`, `mul`, `sdiv`, `and`, `orr`, `eor`, `lsl`, `asr`.
  - `%` becomes `sdiv` + `msub`.
  - Comparisons become `cmp` + `cset` or `b.cond`.
  - Memory access is `ldr`/`str`, or `ldrb`/`strb` for `char`.
  - Calls are `bl`.
- **Encoding**: every A64 instruction is exactly 32 bits. An encoder is a fixed opcode with register and immediate fields OR'd into place, with no variable-length decoding to worry about.
- **Immediates**:
  - Small constants fit in `add`/`sub`'s 12-bit immediate, optionally shifted by 12.
  - Wider ones are built with `movz` plus up to three `movk` instructions, 16 bits at a time.
  - Load and store offsets use the scaled unsigned-offset form when they fit.
- **Branches and calls**: forward targets aren't known yet when a branch is emitted. Each branch is emitted with a zero offset and its location is recorded. Once every label's address is known, a fixup pass patches in the real offsets: `imm19` for `b.cond`/`cbz`, `imm26` for `b`/`bl`.
- **Data**: globals and string literals go into a data section and are addressed with `adrp` + `add`.

---

## Under the hood

- **Arena allocation everywhere.** AST nodes, symbols and quads live in growable arenas. Short-lived per-function data goes on a chunked scratchpad with save/restore checkpoints, so each spill round can throw away everything it built in one call.
- **Bitvectors** for liveness, so sets are unioned and compared a word at a time.
- **Stage snapshots.** Every run writes the token list, AST, IR, CFG/liveness and the allocated IR to `src/tester/lastRun/`. That makes it easy to diff a change against a saved test suite.

---

## Building & running

Requires GCC (MSYS2 UCRT64), CMake and Ninja.

```bat
bld             :: release build    -> build\GPC.exe
bld -g          :: debug build      -> build-debug\GPC.exe (-O0 -g3)
compile file.c  :: run the compiler on file.c
debug file.c    :: run the debug build under gdb
```

---