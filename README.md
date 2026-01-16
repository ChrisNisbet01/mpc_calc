# Multiple Variable Support in Formula Parser

This document outlines the changes and effort required to extend the formula parser to support multiple variables (e.g., `x`, `y`, `z`) instead of the current single variable `x`.

## 1. Grammar Modifications

*   **Effort:** Low
*   **Change:** The `variable` rule in the `mpc` grammar currently defined as `"x"` would need to be expanded.
    *   **Option A (Predefined list):** `variable : "x" | "y" | "z" ;` (Hardcoding a few variables).
    *   **Option B (General identifier):** `variable : /[a-zA-Z_][a-zA-Z0-9_]*/ ;` (Allows user-defined variable names, more flexible).

## 2. `eval_ast` Function Modifications

*   **Effort:** Medium
*   **Change:** The `eval_ast` function currently takes a single `double x_value` parameter. To support multiple variables, this would need to be replaced with a data structure that can map variable names to their corresponding values.
    *   **Solution:** An additional parameter, such as a pointer to a `VariableMap` (a hash map, an array of `(name, value)` pairs, or a custom struct), would be passed to `eval_ast`.
    *   When `eval_ast` encounters a `variable` node in the AST, it would look up `tree->contents` (the variable's name) in this `VariableMap` to retrieve its current numerical value.

## 3. `formula_evaluate` Function Modifications

*   **Effort:** Medium
*   **Change:** The `formula_evaluate` function's signature would need to be updated to accept the `VariableMap` data structure instead of a single `double x`.
    *   **Solution:** `int formula_evaluate(Formula * f, VariableMap const * var_values, double * result);`
    *   This `VariableMap` would then be passed down to the `eval_ast` function.

## 4. `FormulaContext` Structure and `formula_compile` Function

*   **`FormulaContext`:** No direct changes to the structure are required, as it stores compilation-time artifacts (parsers, AST), not evaluation-time variable values.
*   **`formula_compile`:** Low-Medium effort. Its core functionality of parsing the formula wouldn't change. However, it *could* be extended to validate variable names if a general identifier rule is used, or to extract a list of variables found in the formula for user convenience.

## 5. `parse_and_evaluate` Function

*   **Effort:** Low
*   **Change:** If this wrapper function is maintained, it would need to be updated to accept variable assignments, construct the `VariableMap`, and then pass it to the new `formula_evaluate` signature.

## 6. Test Modifications

*   **Effort:** Medium-High
*   **Change:**
    *   All existing tests that currently rely on the single `x` variable would need to be updated to use the new `VariableMap` structure when calling `formula_evaluate`.
    *   New test cases would be required to verify:
        *   Parsing and evaluating formulas with multiple distinct variables (e.g., `"x + y * z"`).
        *   Correct behavior when different values are provided for different variables.
        *   Error handling for formulas containing variables that are not defined in the `VariableMap`.

## 7. Main Application Modifications

*   **Effort:** Low-Medium
*   **Change:** The `main.c` (or equivalent application entry point) would need to be updated to:
    *   Create and populate instances of the `VariableMap` data structure.
    *   Pass this `VariableMap` to `formula_evaluate` calls.
    *   If interactive, handle user input for multiple variable assignments.

## Summary

Implementing multiple variable support is a **Medium** level of effort. The core work involves creating a flexible `VariableMap` data structure and integrating it throughout the evaluation pipeline, followed by a significant update to the test suite to ensure correctness and robustness.
