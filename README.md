# Language Specification: C-Subset

## 1. Types & Memory
* **Primitives**: `int` (32-bit signed), `char` (8-bit), `void` (functions with no return value).
* **Pointers**: Multi-level indirection (e.g., `int***`). Managed via `&` (address-of) and `*` (dereference).
* **Arrays**: Single-dimensional, fixed-size. Syntax `arr[i]` decays to scaled pointer arithmetic.
* **Constraints**: Strict typing enforced. No implicit casting or promotion. Structs/unions are excluded.
* **Scope**: Supports local stack variables and global static variables.

## 2. Syntax & Control Flow
* **Statements**: Semicolon-terminated. Blocks bounded by `{}`.
* **Conditionals**: `if (expr) { ... } else { ... }`
* **Loops**: `while (expr) { ... }`
* **Jumps**: `break;`, `continue;`, and `return <expr>;`
* **Booleans**: Evaluated strictly on zero (`0` as false) vs. non-zero (any other value as true).

## 3. Operators
* **Unary**: `&`, `*`, `-`, `!`, `~`
* **Binary**: `*`, `/`, `%`, `+`, `-`
* **Comparison**: `<`, `>`, `<=`, `>=`, `==`, `!=`
* **Logical**: `&&`, `||` (with short-circuiting)
* **Assignment**: `=`

## 4. Target Architecture
* Complies with standard **ARM64** procedure call rules.
* **Arguments**: First eight scalar/pointer parameters passed via registers `x0`-`x7`.
* **Return**: Values returned in register `x0`.