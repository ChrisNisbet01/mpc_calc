#pragma once

/*
 * @brief Parses and evaluates a mathematical formula.
 *
 * This function takes a formula string (e.g., "x * (x + 2)"), substitutes
 * the value for 'x', and computes the result.
 *
 * @param formula The mathematical formula to evaluate.
 * @param x The value to substitute for the variable 'x'.
 * @param result A pointer to a double where the result will be stored.
 * @return 0 on success, a non-zero value on parsing or evaluation error.
 */
int
parse_and_evaluate(char const * formula, double x, double * result);