#pragma once

/*
 * Opaque pointer to the internal formula context structure.
 */
struct FormulaContext;
typedef struct FormulaContext Formula;

/**
 * @brief "Compiles" a formula string into a reusable context.
 *
 * This function parses the formula and prepares an Abstract Syntax Tree (AST)
 * for multiple evaluations.
 *
 * @param formula The mathematical formula string to compile.
 * @return A pointer to a Formula context on success, NULL on failure.
 */
Formula *
formula_compile(char const * formula);

/**
 * @brief Evaluates a pre-compiled formula with a given value for 'x'.
 *
 * @param f A pointer to a valid Formula context, returned by formula_compile.
 * @param x The value to substitute for the variable 'x'.
 * @param result A pointer to a double where the evaluation result will be stored.
 * @return 0 on success, a non-zero value on evaluation error.
 */
int
formula_evaluate(Formula * f, double x, double * result);

/**
 * @brief Cleans up and frees all resources associated with a Formula context.
 *
 * @param f A pointer to a valid Formula context.
 */
void
formula_cleanup(Formula * f);


/*
 * @brief All-in-one function to parse and evaluate a mathematical formula.
 *
 * This function is a wrapper around compile, evaluate, and cleanup.
 *
 * @param formula The mathematical formula to evaluate.
 * @param x The value to substitute for the variable 'x'.
 * @param result A pointer to a double where the result will be stored.
 * @return 0 on success, a non-zero value on parsing or evaluation error.
 */
int
parse_and_evaluate(char const * formula, double x, double * result);
