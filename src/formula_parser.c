#include "formula_parser.h"

#include "mpc/mpc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
 * Forward declarations for AST evaluation.
 */
static EvalResult
eval_ast(mpc_ast_t const * const tree, double const x_value);


/*
 * The internal structure for a compiled formula context.
 * This is intentionally kept hidden from the user in the .c file.
 */
struct FormulaContext
{
    /* MPC Parsers */
    mpc_parser_t * Float;
    mpc_parser_t * Int;
    mpc_parser_t * Number;
    mpc_parser_t * Variable;
    mpc_parser_t * Constant;
    mpc_parser_t * Factor;
    mpc_parser_t * Term;
    mpc_parser_t * Expr;
    mpc_parser_t * Formula;

    /* Parsed AST */
    mpc_ast_t * ast;
};

char const *
eval_error_to_string(EvalError error)
{
    switch (error)
    {
        case EVAL_ERROR_NONE:
            return "No error";
        case EVAL_ERROR_DIVISION_BY_ZERO:
            return "Division by zero";
        case EVAL_ERROR_UNKNOWN_CONSTANT:
            return "Unknown constant";
        case EVAL_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

/*
 * See header file for documentation.
 */
Formula *
formula_compile(char const * const formula)
{
    Formula * f = calloc(1, sizeof(*f));

    if (NULL == f)
    {
        return NULL;
    }

    /*
     * Define the grammar for mathematical expressions.
     */
    f->Float     = mpc_new("float");
    f->Int       = mpc_new("int");
    f->Number    = mpc_new("number");
    f->Variable  = mpc_new("variable");
    f->Constant  = mpc_new("constant");
    f->Factor    = mpc_new("factor");
    f->Term      = mpc_new("term");
    f->Expr      = mpc_new("expr");
    f->Formula   = mpc_new("formula");

    mpca_lang(MPCA_LANG_DEFAULT,
        "  float    : /-?[0-9]+\\.[0-9]+/ ;                                                                                                                                    "
        "  int      : /-?[0-9]+/ ;                                                                                                                                            "
        "  number   : <float> | <int> ;                                                                                                                                       "
        "  variable : \"x\" ;                                                                                                                                                 "
        "  constant : \"pi\" | \"e\" ;                                                                                                                                        "
        "  factor   : <number> | <constant> | <variable> | '(' <expr> ')' | \"cos\" '(' <expr> ')' | \"sin\" '(' <expr> ')' | \"tan\" '(' <expr> ')' | \"asin\" '(' <expr> ')' | \"acos\" '(' <expr> ')' | \"atan\" '(' <expr> ')' | \"pow\" '(' <expr> ',' <expr> ')' | \"log10\" '(' <expr> ')' | \"log\" '(' <expr> ')' ;  "
        "  term     : <factor> (('*' | '/') <factor>)* ;                                                                                                                       "
        "  expr     : <term> (('+' | '-') <term>)* ;                                                                                                                          "
        "  formula  : /^/ <expr> /$/ ;                                                                                                                                        ",
        f->Float, f->Int, f->Number, f->Variable, f->Constant, f->Factor, f->Term, f->Expr, f->Formula);

    mpc_result_t parse_result;

    if (mpc_parse("<input>", formula, f->Formula, &parse_result))
    {
        //mpc_ast_print(parse_result.output); /* Debugging line to print AST */
        f->ast = parse_result.output;
        return f;
    }
    else
    {
        mpc_err_print(parse_result.error);
        mpc_err_delete(parse_result.error);
        /* Cleanup the parsers before freeing the context */
        mpc_cleanup(9, f->Float, f->Int, f->Number, f->Variable, f->Constant, f->Factor, f->Term, f->Expr, f->Formula);
        free(f);
        return NULL;
    }
}

/*
 * See header file for documentation.
 */
EvalResult
formula_evaluate(Formula * const f, double const x)
{
    if (NULL == f)
    {
        return (EvalResult){.value = 0.0, .error = EVAL_ERROR_UNKNOWN};
    }

    return eval_ast(f->ast->children[1], x);
}

/*
 * See header file for documentation.
 */
void
formula_cleanup(Formula * f)
{
    if (NULL == f)
    {
        return;
    }

    mpc_ast_delete(f->ast);
    mpc_cleanup(9, f->Float, f->Int, f->Number, f->Variable, f->Constant, f->Factor, f->Term, f->Expr, f->Formula);
    free(f);
}

/*
 * See header file for documentation.
 */
int
parse_and_evaluate(char const * const formula, double const x, double * const result)
{
    Formula * const f = formula_compile(formula);

    if (NULL == f)
    {
        return -1;
    }

    EvalResult const eval_result = formula_evaluate(f, x);
    if (eval_result.error != EVAL_ERROR_NONE)
    {
        *result = 0.0;
        return -1;
    }
    else
    {
        *result = eval_result.value;
    }


    formula_cleanup(f);
    return 0;
}


/*
 * @brief Recursively evaluates the Abstract Syntax Tree (AST) generated by mpc.
 *
 * This function traverses the AST, performing calculations based on the
 * defined grammar rules and substituting the value for 'x'.
 *
 * @param tree The root of the AST or a sub-tree to evaluate.
 * @param x_value The numerical value to substitute for the variable 'x'.
 * @return The calculated numerical result of the AST.
 */
static EvalResult
eval_ast(mpc_ast_t const * const tree, double const x_value)
{
    /*
     * If the node is a number (float or int), it is a terminal leaf. Return its value.
     */
    if ((0 != strstr(tree->tag, "float")) || (0 != strstr(tree->tag, "int")))
    {
        return (EvalResult){.value = atof(tree->contents), .error = EVAL_ERROR_NONE};
    }
    if (0 != strstr(tree->tag, "variable"))
    {
        return (EvalResult){.value = x_value, .error = EVAL_ERROR_NONE};
    }
    if (0 != strstr(tree->tag, "constant"))
    {
        if (strcmp(tree->contents, "pi") == 0)
        {
            return (EvalResult){.value = M_PI, .error = EVAL_ERROR_NONE};
        }
        else if (strcmp(tree->contents, "e") == 0)
        {
            return (EvalResult){.value = M_E, .error = EVAL_ERROR_NONE};
        }
        else
        {
            return (EvalResult){.value = 0.0, .error = EVAL_ERROR_UNKNOWN_CONSTANT};
        }
    }

    /*
     * If the expression is wrapped in parentheses, e.g., "(<expr>)", its value is
     * the evaluation of the inner expression, which is the second child.
     */
    if (0 == strcmp(tree->children[0]->contents, "("))
    {
        return eval_ast(tree->children[1], x_value);
    }

    /*
     * Check if the node is a function call, like "cos(<expr>)".
     */
    if (0 == strcmp(tree->children[0]->contents, "cos"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = cos(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "sin"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = sin(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "tan"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = tan(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "asin"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = asin(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "acos"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = acos(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "atan"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = atan(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "pow"))
    {
        /* The arguments are at index 2 and 4 */
        EvalResult const arg1 = eval_ast(tree->children[2], x_value);
        if (arg1.error != EVAL_ERROR_NONE)
        {
            return arg1;
        }
        EvalResult const arg2 = eval_ast(tree->children[4], x_value);
        if (arg2.error != EVAL_ERROR_NONE)
        {
            return arg2;
        }
        return (EvalResult){.value = pow(arg1.value, arg2.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "log"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = log(arg.value), .error = EVAL_ERROR_NONE};
    }

    if (0 == strcmp(tree->children[0]->contents, "log10"))
    {
        /* The argument is the third child (index 2) */
        EvalResult const arg = eval_ast(tree->children[2], x_value);
        if (arg.error != EVAL_ERROR_NONE)
        {
            return arg;
        }
        return (EvalResult){.value = log10(arg.value), .error = EVAL_ERROR_NONE};
    }

    /*
     * Otherwise, it's an operator expression. Evaluate the first child as the
     * starting value.
     */
    EvalResult left_operand_res = eval_ast(tree->children[0], x_value);
    if (left_operand_res.error != EVAL_ERROR_NONE)
    {
        return left_operand_res;
    }
    double left_operand = left_operand_res.value;

    /*
     * Iterate over the remaining children, which come in (operator, operand) pairs.
     */
    for (int i = 1; i < tree->children_num; i += 2)
    {
        mpc_ast_t const * const operator_node = tree->children[i];
        mpc_ast_t const * const right_operand_node = tree->children[i + 1];

        char const * const op = operator_node->contents;
        EvalResult const right_operand_res = eval_ast(right_operand_node, x_value);
        if (right_operand_res.error != EVAL_ERROR_NONE)
        {
            return right_operand_res;
        }
        double const right_operand = right_operand_res.value;

        if (0 == strcmp(op, "+"))
        {
            left_operand += right_operand;
        }
        else if (0 == strcmp(op, "-"))
        {
            left_operand -= right_operand;
        }
        else if (0 == strcmp(op, "*"))
        {
            left_operand *= right_operand;
        }
        else if (0 == strcmp(op, "/"))
        {
            /*
             * Basic division by zero check.
             */
            if (0.0 == right_operand)
            {
                return (EvalResult){.value = 0.0, .error = EVAL_ERROR_DIVISION_BY_ZERO};
            }

            left_operand /= right_operand;
        }
    }

    return (EvalResult){.value = left_operand, .error = EVAL_ERROR_NONE};
}
