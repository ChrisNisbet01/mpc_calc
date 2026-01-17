#include "formula_parser.h"

#include "mpc/mpc.h"

#include <math.h>
#include <stdbool.h>
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
        case EVAL_ERROR_UNKNOWN_OPERATION:
            return "Unknown operation";
        case EVAL_ERROR_NULL_FORMULA:
            return "NULL formula";
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
        "  float    : /-?[0-9]+\\.[0-9]+/ ;  "
        "  int      : /-?[0-9]+/ ;  "
        "  number   : <float> | <int> ;  "
        "  variable : \"x\" ;  "
        "  constant : \"pi\" | \"e\" ;  "
        "  factor   : <number> | <constant> | <variable> | '(' <expr> ')' |  "
              "\"cos\" '(' <expr> ')' | \"sin\" '(' <expr> ')' |  "
              "\"tan\" '(' <expr> ')' | \"asin\" '(' <expr> ')' |  "
              "\"acos\" '(' <expr> ')' | \"atan\" '(' <expr> ')' |  "
              "\"pow\" '(' <expr> ',' <expr> ')' | \"log10\" '(' <expr> ')' |  "
              "\"log\" '(' <expr> ')' ;  "
        "  term     : <factor> (('*' | '/') <factor>)* ;  "
        "  expr     : <term> (('+' | '-') <term>)* ;  "
        "  formula  : /^/ <expr> /$/ ;  ",
        f->Float,
        f->Int,
        f->Number,
        f->Variable,
        f->Constant,
        f->Factor,
        f->Term,
        f->Expr,
        f->Formula
    );

    mpc_result_t parse_result;

    if (mpc_parse("<input>", formula, f->Formula, &parse_result))
    {
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
        return (EvalResult){.error = EVAL_ERROR_NULL_FORMULA};
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


static EvalResult
number_handler(mpc_ast_t const * const tree, double const x_value)
{
    (void)x_value;
    return (EvalResult){.value = atof(tree->contents), .error = EVAL_ERROR_NONE};
}

static EvalResult
variable_handler(mpc_ast_t const * const tree, double const x_value)
{
    (void)tree;
    return (EvalResult){.value = x_value, .error = EVAL_ERROR_NONE};
}

static EvalResult
constant_handler(mpc_ast_t const * const tree, double const x_value)
{
    (void)x_value;
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
        return (EvalResult){.error = EVAL_ERROR_UNKNOWN_CONSTANT};
    }
}

static EvalResult
oparen_handler(mpc_ast_t const * const tree, double const x_value)
{
    return eval_ast(tree->children[1], x_value);
}

static EvalResult
cos_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = cos(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
sin_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = sin(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
tan_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = tan(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
acos_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = acos(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
asin_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = asin(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
atan_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = atan(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
pow_handler(mpc_ast_t const * const tree, double const x_value)
{
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

static EvalResult
log_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = log(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
log10_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult const arg = eval_ast(tree->children[2], x_value);
    if (arg.error != EVAL_ERROR_NONE)
    {
        return arg;
    }
    return (EvalResult){.value = log10(arg.value), .error = EVAL_ERROR_NONE};
}

static EvalResult
expr_handler(mpc_ast_t const * const tree, double const x_value)
{
    EvalResult left_operand_res = eval_ast(tree->children[0], x_value);
    if (left_operand_res.error != EVAL_ERROR_NONE)
    {
        return left_operand_res;
    }
    double left_operand = left_operand_res.value;

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
            if (0.0 == right_operand)
            {
                return (EvalResult){.error = EVAL_ERROR_DIVISION_BY_ZERO};
            }
            left_operand /= right_operand;
        }
        else
        {
            return (EvalResult){.error = EVAL_ERROR_UNKNOWN_OPERATION};
        }
    }

    return (EvalResult){.value = left_operand, .error = EVAL_ERROR_NONE};
}

typedef struct eval_tag_st
{
    char const * name;
    EvalResult (*handler)(mpc_ast_t const * tree, double x_value);
} eval_tag_st;

static eval_tag_st const tag_handlers[] =
{
    {.name = "float", .handler = number_handler},
    {.name = "int", .handler = number_handler},
    {.name = "variable", .handler = variable_handler},
    {.name = "constant", .handler = constant_handler},
    {0},
};

static eval_tag_st const contents_handlers[] =
{
    {.name = "(", .handler = oparen_handler},
    {.name = "acos", .handler = acos_handler}, /* Must come before the sin/cos/tan cases. */
    {.name = "asin", .handler = asin_handler},
    {.name = "atan", .handler = atan_handler},
    {.name = "cos", .handler = cos_handler},
    {.name = "sin", .handler = sin_handler},
    {.name = "tan", .handler = tan_handler},
    {.name = "pow", .handler = pow_handler},
    {.name = "log10", .handler = log10_handler}, /* Must come before "log" as "log" is a substring of "log10". */
    {.name = "log", .handler = log_handler},
    {0},
};

static eval_tag_st const *
eval_tag_handler_lookup(char const * const name)
{
    for (size_t i = 0; tag_handlers[i].name != NULL; ++i)
    {
        /*
         * NB: Must NOT be an exact match as the tags have other characters in them
         * (e.g. "factor|variable|eval_error_to_string").
         */
        bool const name_matches = strstr(name, tag_handlers[i].name) != NULL;

        if (name_matches)
        {
            return &tag_handlers[i];
        }
    }
    return NULL;
}

static eval_tag_st const *
eval_contents_handler_lookup(char const * const name)
{
    for (size_t i = 0; contents_handlers[i].name != NULL; ++i)
    {
        bool const name_matches = strcmp(name, contents_handlers[i].name) == 0;

        if (name_matches)
        {
            return &contents_handlers[i];
        }
    }
    return NULL;
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
    eval_tag_st const * handler = eval_tag_handler_lookup(tree->tag);
    if (NULL != handler)
    {
        return handler->handler(tree, x_value);
    }

    handler = eval_contents_handler_lookup(tree->children[0]->contents);
    if (NULL != handler)
    {
        return handler->handler(tree, x_value);
    }

    return expr_handler(tree, x_value);
}

