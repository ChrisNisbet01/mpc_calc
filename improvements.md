# Suggested Structural and Functional Improvements for the Library

## Structural Improvements:

### 1. More Granular Error Handling (Location Information) (Done)
*   **Problem:** The current `EvalError` enum provides general error types but lacks specific location information (e.g., "division by zero at character 50"). This makes debugging complex formulas difficult.
*   **Suggestion:** Modify `EvalResult` to include a `char *error_message` field for dynamically generated, descriptive error messages, or `int line_number`, `int column_number` to pinpoint the error's location within the formula string. This would require careful memory management for allocated error strings or passing context down the AST.

### 2. Abstract Syntax Tree (AST) Decoupling (Done)
*   **Problem:** The `mpc_ast_t` is directly exposed and used throughout `eval_ast` and its handlers. This couples the evaluation logic tightly to the `mpc` library's internal AST representation.
*   **Suggestion:** Introduce an intermediate, library-specific AST representation. Instead of parsing into the MPC AST, parse directly into custom `FormulaAST`
 nodes. This decouples the evaluation logic from `mpc`, making it easier to swap out `mpc` for another parser generator in the future or to extend the AST with custom node types without being constrained by `mpc`'s design.

### 3. Dynamic Handler Registration
*   **Problem:** The `constant` and `function` arrays are static. Adding new functions or operations requires modifying and recompiling `formula_parser.c`.
*   **Suggestion:** Implement a dynamic registration mechanism for handlers. This could involve a function like 
`formula_register_constant(const char *constant_name, EvalResult (*handler_func)(mpc_ast_t const *tree, double x_value))`. 
This would allow users of the library (or future extensions) to add custom functions or constants at runtime without modifying the core library code.

### 4. Context Object for `eval_ast`
*   **Problem:** The `x_value` is passed explicitly to every `eval_ast` call. If you wanted to add support for multiple variables (`y`, `z`) or other context-dependent data, the function signature would become cumbersome.
*   **Suggestion:** Introduce an `EvalContext` struct that holds the `x_value`, a symbol table for other variables/constants, error reporting mechanisms, or any other global state needed during evaluation. Pass a pointer to this context object (`EvalContext *ctx`) to `eval_ast` and all handlers.

## Functional Improvements:

### 1. Support for More Functions and Operators
*   **Problem:** The library currently supports a limited set of mathematical functions (trig, log, pow) and basic arithmetic operations.
*   **Suggestion:** Extend the grammar and add handlers for:
    *   **Constants:** `infinity`, `nan`
    *   **Operators:** Modulo (`%`), integer division (`//`), bitwise operations (`&`, `|`, `^`, `~`, `<<`, `>>`)
    *   **Functions:** `abs()`, `round()`, `ceil()`, `floor()`, `exp()`, `min()`, `max()`, `sign()`, `fact()` (factorial).
    *   **Ternary operator:** `condition ? true_expr : false_expr`

### 2. Variable Assignment and Scope
*   **Problem:** Currently, only `x` is supported as a variable, and its value is passed directly. There's no mechanism for defining or using other named variables within a formula or across multiple evaluations.
*   **Suggestion:** Implement a symbol table within the `FormulaContext` (or the suggested `EvalContext`) to store variable names and their associated values. This would allow for more complex expressions like `let y = x*x; y += 2;` or `a=10; b=20; a+b;`.

### 3. Unit System Support
*   **Problem:** All calculations are currently dimensionless.
*   **Suggestion:** Integrate a basic unit system. This would involve parsing units, performing unit conversions, and ensuring dimensional consistency during calculations (e.g., `10m + 5s` would be an error). This is a significant undertaking but could be very powerful for scientific or engineering applications.

### 4. Error Recovery and Reporting
*   **Problem:** Upon the first error, evaluation stops.
*   **Suggestion:** For parsing errors, allow for "soft" errors where parsing continues to find more errors (useful for providing multiple feedback points to the user). For evaluation, perhaps a mode where `NAN` is propagated instead of an immediate stop for non-critical errors.

### 5. Performance Optimizations
*   **Problem:** For very complex or frequently evaluated formulas, the current AST traversal might become a bottleneck.
*   **Suggestion:**
    *   **JIT Compilation:** For extreme performance, consider just-in-time compilation of the AST into machine code (complex, but very fast).
    *   **Intermediate Code Generation:** Translate the AST into a simpler bytecode that can be interpreted more efficiently than direct AST traversal.
