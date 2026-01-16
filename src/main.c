#include "formula_parser.h"

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char ** argv)
{
    char const * formula_str = "x * x + 2 * x + 1";
    Formula * formula = NULL;
    double result = 0.0;
    int ret = 0;

    printf("Compiling formula: \"%s\"\n", formula_str);
    formula = formula_compile(formula_str);

    if (NULL == formula)
    {
        fprintf(stderr, "Failed to compile formula.\n");
        return -1;
    }

    /* Evaluate with x = 2.0 */
    double const x1 = 2.0;

    ret = formula_evaluate(formula, x1, &result);
    if (0 == ret)
    {
        printf("Result for x = %.2f: %.2f\n", x1, result);
    }
    else
    {
        fprintf(stderr, "Failed to evaluate formula for x = %.2f\n", x1);
    }

    /* Evaluate with x = 3.0 */
    double const x2 = 3.0;

    ret = formula_evaluate(formula, x2, &result);
    if (0 == ret)
    {
        printf("Result for x = %.2f: %.2f\n", x2, result);
    }
    else
    {
        fprintf(stderr, "Failed to evaluate formula for x = %.2f\n", x2);
    }

    /* Clean up */
    formula_cleanup(formula);

    return 0;
}