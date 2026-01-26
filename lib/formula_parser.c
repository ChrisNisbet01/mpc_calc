#include "formula_parser.h"
#include "utils.h"

#include <mpc.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal structure for FormulaAST
typedef enum
{
    FORMULA_AST_NODE_TYPE_NUMBER,
    FORMULA_AST_NODE_TYPE_VARIABLE,
    FORMULA_AST_NODE_TYPE_CONSTANT,
    FORMULA_AST_NODE_TYPE_BINARY_OPERATOR,
    FORMULA_AST_NODE_TYPE_UNARY_OPERATOR,
    FORMULA_AST_NODE_TYPE_FUNCTION_CALL,
} FormulaASTNodeType;

struct FormulaAST
{
    FormulaASTNodeType type;
    union
    {
        double number_value;
        char * variable_name; // owned by this struct
        struct
        {
            char * operator_name; // owned by this struct
            struct FormulaAST * left; // owned by this struct
            struct FormulaAST * right; // owned by this struct
        } binary_op;
        struct
        {
            char * operator_name; // owned by this struct
            struct FormulaAST * child; // owned by this struct
        } unary_op;
        struct
        {
            char * function_name; // owned by this struct
            struct FormulaAST ** args; // array of owned pointers
            size_t num_args;
        } function_call;
    } data;
};

// Forward declaration of internal AST destruction function
static void formula_ast_destroy_internal(FormulaAST * node);

/*
 * Forward declarations for AST evaluation.
 */
static EvalResult
eval_ast(struct FormulaAST const * const tree, double const x_value);

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

    /* Custom AST */
    FormulaAST * ast;

    /* Error message for parsing failures */
    char *parsing_error_message;
};

// Public API for retrieving the last parsing error
char const *
formula_get_last_error(Formula * const f)
{
    if (f && f->parsing_error_message) {
        return f->parsing_error_message;
    }
    return NULL;
}

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
    case EVAL_ERROR_UNKNOWN_VARIABLE:
        return "Unknown variable";
    case EVAL_ERROR_UNKNOWN_OPERATION:
        return "Unknown operation";
    case EVAL_ERROR_NULL_FORMULA:
        return "NULL formula";
    case EVAL_ERROR_INVALID_ARGUMENTS:
        return "Invalid arguments";
    case EVAL_ERROR_PARSING_FAILED:
        return "Parsing failed";
    case EVAL_ERROR_UNKNOWN:
    default:
        return "Unknown error";
    }
}

typedef struct constant_t
{
    char const * name;
    double value;
} constant_t;

typedef bool (*constants_foreach_cb)(constant_t const * constant, void * ctx);

static constant_t constants[] =
{
    {
        .name = "pi",
        .value = M_PI,
    },
    {
        .name = "e",
        .value = M_E,
    },
};

static constant_t const *
constants_foreach(constants_foreach_cb const cb, void * const user_ctx)
{
    for (size_t i = 0; i < ARRAY_SIZE(constants); i++)
    {
        constant_t const * const candidate = &constants[i];

        if (cb(candidate, user_ctx))
        {
            return candidate;
        }
    }

    return NULL;
}

static bool
constant_name_matches(constant_t const * const constant, void * const user_ctx)
{
    char const * const name = user_ctx;
    bool const name_matches = strcmp(constant->name, name) == 0;

    return name_matches;
}

static constant_t const *
constant_lookup_by_name(char const * const name)
{
    constant_t const * const constant =
        constants_foreach(constant_name_matches, (void *)name);

    return constant;
}

typedef double (*unary_func_t)(double v1);
typedef double (*binary_func_t)(double v1, double v2);

typedef struct function_t
{
    char const * name;
    size_t num_args;
    union {
        unary_func_t unary;
        binary_func_t binary;
    };
} function_t;

typedef bool (*functions_foreach_cb)(function_t const * func, void * ctx);

static function_t functions[] =
{
    {
        .name = "cos",
        .num_args = 1,
        .unary = cos,
    },
    {
        .name = "sin",
        .num_args = 1,
        .unary = sin,
    },
    {
        .name = "tan",
        .num_args = 1,
        .unary = tan,
    },
    {
        .name = "acos",
        .num_args = 1,
        .unary = acos,
    },
    {
        .name = "asin",
        .num_args = 1,
        .unary = asin,
    },
    {
        .name = "atan",
        .num_args = 1,
        .unary = atan,
    },
    {
        .name = "log10",
        .num_args = 1,
        .unary = log10,
    },
    {
        .name = "log",
        .num_args = 1,
        .unary = log,
    },
    {
        .name = "sqrt",
        .num_args = 1,
        .unary = sqrt,
    },
    {
        .name = "pow",
        .num_args = 2,
        .binary = pow,
    },
    {
        .name = "abs",
        .num_args = 1,
        .unary = fabs,
    },
    {
        .name = "round",
        .num_args = 1,
        .unary = round,
    },
    {
        .name = "ceil",
        .num_args = 1,
        .unary = ceil,
    },
    {
        .name = "floor",
        .num_args = 1,
        .unary = floor,
    },
    {
        .name = "exp",
        .num_args = 1,
        .unary = exp,
    },
    {
        .name = "min",
        .num_args = 2,
        .binary = fmin,
    },
    {
        .name = "max",
        .num_args = 2,
        .binary = fmax,
    },
};

static function_t const *
functions_foreach(functions_foreach_cb const cb, void * const user_ctx)
{
    for (size_t i = 0; i < ARRAY_SIZE(functions); i++)
    {
        function_t const * const candidate = &functions[i];

        if (cb(candidate, user_ctx))
        {
            return candidate;
        }
    }

    return NULL;
}

static bool
function_name_matches(function_t const * const func, void * const user_ctx)
{
    char const * const name = user_ctx;
    bool const name_matches = strcmp(func->name, name) == 0;

    return name_matches;
}

static function_t const *
function_lookup_by_name(char const * const name)
{
    function_t const * const func =
        functions_foreach(function_name_matches, (void *)name);

    return func;
}

static FormulaAST *
formula_ast_create_number(double value)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_NUMBER;
        node->data.number_value = value;
    }
    return node;
}

static FormulaAST *
formula_ast_create_variable(const char * name)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_VARIABLE;
        // Duplicate string as caller frees the passed variable
        node->data.variable_name = strdup(name);
        if (!node->data.variable_name)
        {
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_constant(const char * name)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_CONSTANT;
        // Duplicate string as caller frees the passed variable
        node->data.variable_name = strdup(name);
        if (!node->data.variable_name)
        {
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_binary_op(const char * op_name, FormulaAST * left, FormulaAST * right)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_BINARY_OPERATOR;
        node->data.binary_op.operator_name = strdup(op_name); // Duplicate string
        if (!node->data.binary_op.operator_name)
        {
            // Cleanup on allocation failure
            formula_ast_destroy_internal(left);
            formula_ast_destroy_internal(right);
            free(node);
            return NULL;
        }
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
    }
    return node;
}

static FormulaAST *
formula_ast_create_unary_op(const char * op_name, FormulaAST * child)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_UNARY_OPERATOR;
        // Duplicate string as caller frees the passed name.
        node->data.unary_op.operator_name = strdup(op_name);
        node->data.unary_op.child = child;
        if (!node->data.unary_op.operator_name)
        {
            // Cleanup on allocation failure
            formula_ast_destroy_internal(child);
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_function_call(const char * func_name, FormulaAST ** args, size_t num_args)
{
    FormulaAST * node = calloc(1, sizeof(*node));
    if (node)
    {
        node->type = FORMULA_AST_NODE_TYPE_FUNCTION_CALL;
        // Duplicate string as caller frees the passed name.
        node->data.function_call.function_name = strdup(func_name);
        if (!node->data.function_call.function_name)
        {
            // Cleanup on allocation failure
            for (size_t i = 0; i < num_args; ++i)
            {
                formula_ast_destroy_internal(args[i]);
            }
            free(args);
            free(node);
            return NULL;
        }
        node->data.function_call.args = args; // Takes ownership of the array of pointers
        node->data.function_call.num_args = num_args;
    }
    return node;
}

static void
formula_ast_destroy_internal(FormulaAST * node)
{
    if (NULL == node)
    {
        return;
    }

    switch (node->type)
    {
    case FORMULA_AST_NODE_TYPE_NUMBER:
        // No dynamic memory to free
        break;
    case FORMULA_AST_NODE_TYPE_CONSTANT:
    case FORMULA_AST_NODE_TYPE_VARIABLE:
        free(node->data.variable_name);
        break;
    case FORMULA_AST_NODE_TYPE_BINARY_OPERATOR:
        free(node->data.binary_op.operator_name);
        formula_ast_destroy_internal(node->data.binary_op.left);
        formula_ast_destroy_internal(node->data.binary_op.right);
        break;
    case FORMULA_AST_NODE_TYPE_UNARY_OPERATOR:
        free(node->data.unary_op.operator_name);
        formula_ast_destroy_internal(node->data.unary_op.child);
        break;
    case FORMULA_AST_NODE_TYPE_FUNCTION_CALL:
        free(node->data.function_call.function_name);
        for (size_t i = 0; i < node->data.function_call.num_args; ++i)
        {
            formula_ast_destroy_internal(node->data.function_call.args[i]);
        }
        free(node->data.function_call.args);
        break;
    }
    free(node);
}

// Public API wrapper for the internal destructor
void
formula_ast_destroy(FormulaAST * ast)
{
    formula_ast_destroy_internal(ast);
}

// MPC callback for numbers: converts a string to a double and creates a FormulaAST number node.
static mpc_val_t * mpc_make_float(mpc_val_t * val)
{
    double value = atof((char *)val);
    free(val);
    return formula_ast_create_number(value);
}

static mpc_val_t * mpc_make_integer(mpc_val_t * val)
{
    double value = atof((char *)val);
    free(val);
    return formula_ast_create_number(value);
}

// MPC callback for variables: creates a FormulaAST variable node from a string.
static mpc_val_t * mpc_make_variable(mpc_val_t * val)
{
    char * name = (char *)val;
    FormulaAST * node = formula_ast_create_variable(name);
    free(val);
    return node;
}

// MPC callback for constants: creates a FormulaAST variable node from a string (e.g., "pi", "e").
static mpc_val_t * mpc_make_constant(mpc_val_t * val)
{
    char * name = (char *)val;
    FormulaAST * node = formula_ast_create_constant(name);
    free(val);
    return node;
}

// MPC fold callback for unary operators like negation
static mpc_val_t * mpc_make_unary_op_fold(int n_args, mpc_val_t ** args)
{
    // Expects: [op_str, child_ast]
    if (n_args != 2)
    {
        if (args[0])
            free(args[0]);
        if (args[1])
            formula_ast_destroy_internal((FormulaAST *)args[1]);
        return NULL; // Error
    }
    char * op_name = (char *)args[0];
    FormulaAST * child = (FormulaAST *)args[1];
    FormulaAST * node = formula_ast_create_unary_op(op_name, child);
    free(op_name);
    return node;
}

// Struct to hold an operator string and its corresponding operand AST node
// This struct is internal to the handling of binary operations.
typedef struct
{
    char * op_name;
    FormulaAST * operand;
} BinaryOpPart;

// Destructor for BinaryOpPart
static void mpc_destroy_binary_op_part(mpc_val_t * val)
{
    BinaryOpPart * part = (BinaryOpPart *)val;
    if (part)
    {
        free(part->op_name);
        formula_ast_destroy_internal(part->operand);
        free(part);
    }
}

// MPC fold callback for the inner mpc_and (e.g., ("*" <factor>))
// It combines the operator string and the operand AST into a BinaryOpPart struct.
static mpc_val_t * mpc_make_binary_op_part(int n_args, mpc_val_t ** args)
{
    // Expects: [op_str, operand_ast]. n_args should be 2.
    if (n_args != 2)
    {
        // This should not happen with correct grammar
        if (args[0])
        {
            free(args[0]);
        }
        if (args[1])
        {
            formula_ast_destroy_internal((FormulaAST *)args[1]);
        }
        return NULL;
    }

    char * op_name = (char *)args[0];
    FormulaAST * operand_ast = (FormulaAST *)args[1];

    BinaryOpPart * part = calloc(1, sizeof(*part));
    if (!part)
    {
        free(op_name);
        formula_ast_destroy_internal(operand_ast);
        return NULL;
    }
    part->op_name = op_name;
    part->operand = operand_ast;

    return part; // Return BinaryOpPart* as mpc_val_t*
}

// Custom fold function for mpc_many in Term and Expr rules.
// This function collects a list of BinaryOpPart* into a dynamically sized array (void**)
// The returned mpc_val_t* will be a (BinaryOpPart**) cast to mpc_val_t*.
static mpc_val_t * mpc_collect_binary_op_parts(int n_args, mpc_val_t ** args)
{
    // args is an array of BinaryOpPart*
    // We need to return an array of BinaryOpPart* that the caller can iterate over.
    // This is essentially reimplementing a simplified vector.
    BinaryOpPart ** parts_array = calloc(n_args + 1, sizeof(*parts_array)); // +1 for count at index 0
    if (!parts_array)
    {
        for (int i = 0; i < n_args; ++i)
        {
            mpc_destroy_binary_op_part(args[i]);
        }
        return NULL;
    }

    // Store the count in the first element.
    parts_array[0] = (BinaryOpPart *)(long)n_args; // Cast int to void* using long to avoid warnings
    for (int i = 0; i < n_args; ++i)
    {
        parts_array[i + 1] = (BinaryOpPart *)args[i]; // Store the pointers
    }

    return parts_array;
}

// Destructor for the array returned by mpc_collect_binary_op_parts
static void mpc_destroy_collected_binary_op_parts(mpc_val_t * val)
{
    BinaryOpPart ** parts_array = (BinaryOpPart **)val;
    if (parts_array)
    {
        int count = (int)(long)parts_array[0]; // Retrieve count
        for (int i = 0; i < count; ++i)
        {
            mpc_destroy_binary_op_part(parts_array[i + 1]);
        }
        free(parts_array);
    }
}

// Custom fold function for mpc_sepby1 in function calls.
// This function collects a list of FormulaAST* for each argument into a dynamically sized array.
// The returned mpc_val_t* will be a (FormulaAST**) cast to mpc_val_t*.
// The first element of the array stores the count of arguments.
static mpc_val_t * mpc_collect_argument_asts(int n_args, mpc_val_t ** args)
{
    FormulaAST ** asts_array = calloc(n_args + 1, sizeof(*asts_array));
    if (!asts_array)
    {
        for (int i = 0; i < n_args; ++i)
        {
            formula_ast_destroy_internal(args[i]);
        }
        return NULL;
    }

    asts_array[0] = (FormulaAST *)(long)n_args; // Store count in the first element
    for (int i = 0; i < n_args; ++i)
    {
        asts_array[i + 1] = (FormulaAST *)args[i];
    }

    return asts_array;
}

// Destructor for the array returned by mpc_collect_argument_asts
static void mpc_destroy_collected_argument_asts(mpc_val_t * val)
{
    FormulaAST ** asts_array = (FormulaAST **)val;
    if (asts_array)
    {
        int count = (int)(long)asts_array[0]; // Retrieve count
        for (int i = 0; i < count; ++i)
        {
            formula_ast_destroy_internal(asts_array[i + 1]);
        }
        free(asts_array);
    }
}

// MPC fold callback for left-associative binary operations.
// args will be [initial_ast, collected_binary_op_parts_array]
// where collected_binary_op_parts_array is (BinaryOpPart**) from mpc_collect_binary_op_parts
static mpc_val_t * mpc_fold_left_associative_binary_op(int n_args, mpc_val_t ** args)
{
    // n_args will be 2: args[0] is initial_ast, args[1] is (BinaryOpPart**) from mpc_collect_binary_op_parts
    if (n_args != 2)
    {
        return NULL;
    }
    FormulaAST * left_ast = (FormulaAST *)args[0];
    BinaryOpPart ** parts_array = (BinaryOpPart **)args[1];

    if (parts_array)
    {
        int count = (int)(long)parts_array[0]; // Retrieve count
        for (int i = 0; i < count; ++i)
        {
            BinaryOpPart * part = parts_array[i + 1];
            left_ast = formula_ast_create_binary_op(part->op_name, left_ast, part->operand);
            free(part->op_name); // Free the operator string
            free(part); // Free the BinaryOpPart struct
        }
        free(parts_array); // Free the array holding the BinaryOpPart pointers
    }
    return left_ast;
}

// Temporary struct to hold parsed function call info before validation
typedef struct
{
    char * name;
    FormulaAST ** args; // Custom array from mpc_collect_argument_asts
} RawFunctionCallInfo;

// Fold function to create RawFunctionCallInfo struct
static mpc_val_t * mpc_make_raw_function_call(int n, mpc_val_t ** v)
{
    (void)n;
    RawFunctionCallInfo * info = calloc(1, sizeof(*info));
    if (info != NULL)
    {
        info->name = v[0]; // ident
        info->args = v[2]; // OptArgList result
    }

    free(v[1]); // '('
    free(v[3]); // ')'

    return info;

}

// Destructor for RawFunctionCallInfo
static void destroy_raw_function_call(mpc_val_t * val)
{
    RawFunctionCallInfo * info = val;

    if (!info)
        return;

    free(info->name);
    // The destructor for the argument itself is handled by mpc_check
    mpc_destroy_collected_argument_asts(info->args); // Use custom destructor for args
    free(info);
}

// mpc_check function to validate the function call
static int check_function_call(mpc_val_t ** val)
{
    RawFunctionCallInfo * info = *val;

    function_t const * func = function_lookup_by_name(info->name);
    size_t num_passed_args = info->args ? (size_t)(long)info->args[0] : 0;

    if (!func || func->num_args != num_passed_args)
    {
        return 0; // Failure
    }

    return 1; // Success
}

// mpc_apply function to convert RawFunctionCallInfo to FormulaAST
static mpc_val_t * make_function_ast_from_raw(mpc_val_t * val)
{
    RawFunctionCallInfo * info = val;
    size_t num_args = info->args ? (size_t)(long)info->args[0] : 0;
    FormulaAST ** arg_asts = NULL;
    if (num_args > 0)
    {
        arg_asts = malloc(num_args * sizeof(FormulaAST *));
        if (!arg_asts)
        {
            destroy_raw_function_call(info);
            return NULL;
        }
        for (size_t i = 0; i < num_args; i++)
        {
            arg_asts[i] = info->args[i + 1];
        }
    }

    FormulaAST * node = formula_ast_create_function_call(info->name, arg_asts, num_args);

    // Cleanup RawFunctionCallInfo, but not the ASTs it was pointing to
    free(info->name);
    if (info->args)
    {
        free(info->args); // Free the container, not the ASTs
    }
    free(info);

    return node;
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

    // Define individual parsers using mpc_* combinators and our custom callbacks
    // Numbers
    mpc_define(f->Float, mpc_apply(mpc_and(3, mpcf_strfold, mpc_digits(), mpc_char('.'), mpc_digits(), free, free), mpc_make_float));
    mpc_define(f->Int, mpc_apply(mpc_re("[0-9]+"), mpc_make_integer));
    // 'number' just passes through float or int AST
    mpc_define(f->Number, mpc_or(2, mpc_copy(f->Float), mpc_copy(f->Int)));

    // Variable
    mpc_define(
        f->Variable,
        mpc_apply(mpc_stripl(mpc_string("x")), mpc_make_variable)
    );

    // Constant
    mpc_parser_t * Constant_or = mpc_stripl(mpc_string(constants[0].name));
    for (size_t i = 1; i < ARRAY_SIZE(constants); i++)
    {
        Constant_or = mpc_or(2, Constant_or, mpc_stripl(mpc_string(constants[i].name)));
    }
    mpc_define(f->Constant, mpc_apply(Constant_or, mpc_make_constant));

    // Generic Function Call Parser
    // identifier '(' [ arg_list ] ')'
    // An arg_list is one or more expressions, separated by commas.
    mpc_parser_t * Ident = mpc_re("[a-zA-Z_][a-zA-Z0-9_]*");
    mpc_parser_t * open_paren = mpc_stripl(mpc_char('('));
    mpc_parser_t * close_paren = mpc_stripl(mpc_char(')'));
    mpc_parser_t * comma = mpc_stripl(mpc_char(','));

    mpc_parser_t * ArgList = mpc_sepby1(mpc_collect_argument_asts, comma, f->Expr);
    mpc_parser_t * OptArgList = mpc_maybe(ArgList);

    mpc_parser_t * RawFunctionCall =
        mpc_and(
            4,
            mpc_make_raw_function_call,
            Ident,
            open_paren,
            OptArgList,
            close_paren,
            free, free, (mpc_dtor_t)mpc_destroy_collected_argument_asts
        );

    mpc_parser_t * CheckedFunctionCall =
        mpc_check(
            RawFunctionCall,
            (mpc_dtor_t)destroy_raw_function_call,
            check_function_call,
            "invalid function call"
        );
    mpc_parser_t * FunctionCall = mpc_apply(CheckedFunctionCall, make_function_ast_from_raw);

    // Factor rules: order matters.
    mpc_define(
        f->Factor,
        mpc_or(6,
               mpc_and(2, (mpc_fold_t)mpc_make_unary_op_fold, mpc_stripl(mpc_char('-')), mpc_copy(f->Factor), free, (mpc_dtor_t)formula_ast_destroy), // Unary minus: -Factor
               FunctionCall, // Generic function call
               mpc_stripl(mpc_parens(mpc_copy(f->Expr), (mpc_dtor_t)formula_ast_destroy)),
               mpc_stripl(mpc_copy(f->Number)),
               mpc_stripl(mpc_copy(f->Constant)),
               mpc_stripl(mpc_copy(f->Variable))
              )
        );

    // Term (multiplication and division)
    // term : <factor> (('*' | '/') <factor>)* ;
    mpc_define(
        f->Term,
        mpc_and(
            2,
            mpc_fold_left_associative_binary_op,
            mpc_stripl(mpc_copy(f->Factor)), // The initial factor
            mpc_many(
                mpc_collect_binary_op_parts,
                mpc_and(
                    2,
                    mpc_make_binary_op_part,
                    mpc_stripl(mpc_oneof("*/")),
                    mpc_stripl(mpc_copy(f->Factor)),
                    free,
                    (mpc_dtor_t)formula_ast_destroy
                    )
                ), // Collects list of (op_str, factor_ast) parts into a custom array
            (mpc_dtor_t)mpc_destroy_collected_binary_op_parts // Destructor for collected parts
            ));


    // Expression (addition and subtraction)
    // expr : <term> (('+' | '-') <term>)* ;
    mpc_define(
        f->Expr,
        mpc_and(
            2,
            mpc_fold_left_associative_binary_op,
            mpc_copy(f->Term), // The initial term
            mpc_many(
                mpc_collect_binary_op_parts,
                mpc_and(
                    2,
                    mpc_make_binary_op_part,
                    mpc_stripl(mpc_oneof("+-")),
                    mpc_copy(f->Term),
                    free, (mpc_dtor_t)formula_ast_destroy)
                ),
            (mpc_dtor_t)mpc_destroy_collected_binary_op_parts // Destructor for collected parts
            )
        );

    // Formula (start and end of input)
    mpc_define(
        f->Formula,
        mpc_and(3,
                mpcf_snd_free,
                mpc_soi(),
                mpc_stripr(mpc_copy(f->Expr)),
                mpc_eoi(),
                mpcf_dtor_null,
                (mpc_dtor_t)formula_ast_destroy
               )
        ); // Keep only the Expr AST

    mpc_result_t parse_result = { 0 };

    if (mpc_parse("<formula>", formula, f->Formula, &parse_result))
    {
        // Now parse_result.output should directly be a FormulaAST*
        f->ast = (FormulaAST *)parse_result.output;
    }
    else
    {
        f->parsing_error_message = mpc_err_string(parse_result.error);
        f->ast = NULL; // Ensure AST is NULL if parsing fails
        mpc_err_delete(parse_result.error);
        // Note: f itself is returned, even if it contains a parsing error.
        // The caller must check formula_get_last_error() or formula->ast to determine success.
    }
    return f;
}

/*
 * See header file for documentation.
 */
EvalResult
formula_evaluate(Formula * const f, double const x)
{
    if (NULL == f)
    {
        return (EvalResult){ .error = EVAL_ERROR_NULL_FORMULA };
    }

    // If formula_compile failed, return the parsing error directly
    if (f->parsing_error_message != NULL) {
        return (EvalResult){ .error = EVAL_ERROR_PARSING_FAILED, .detailed_error_message = f->parsing_error_message };
    }

    return eval_ast(f->ast, x);
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

    formula_ast_destroy(f->ast);
    free(f->parsing_error_message);
    mpc_cleanup(
        9,
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
    free(f);
}

/*
 * See header file for documentation.
 */
int
parse_and_evaluate(char const * const formula, double const x, double * const result)
{
    Formula * const f = formula_compile(formula);

    // If formula_compile returns NULL, it's a memory allocation failure or similar
    if (f == NULL)
    {
        return -1;
    }

    // Check if formula_compile encountered a parsing error
    char const * const error_message = formula_get_last_error(f);

    if (error_message != NULL) {
        fprintf(stderr, "parsing error: %s\n", error_message);
        formula_cleanup(f); // Clean up the partially compiled formula
        return -1;
    }

    EvalResult const eval_result = formula_evaluate(f, x);
    if (eval_result.error != EVAL_ERROR_NONE)
    {
        *result = 0.0;
        formula_cleanup(f); // Clean up the compiled formula
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
 * @brief Recursively evaluates the Abstract Syntax Tree (AST).
 *
 * This function traverses the custom AST, performing calculations based on the
 * defined grammar rules and substituting the value for 'x'.
 *
 * @param tree The root of the AST or a sub-tree to evaluate.
 * @param x_value The numerical value to substitute for the variable 'x'.
 * @return The calculated numerical result of the AST.
 */
static EvalResult
eval_ast(FormulaAST const * const tree, double const x_value)
{
    if (NULL == tree)
    {
        return (EvalResult) { .error = EVAL_ERROR_UNKNOWN }; // Or specific error for null AST
    }

    switch (tree->type)
    {
    case FORMULA_AST_NODE_TYPE_NUMBER:
        return (EvalResult) { .value = tree->data.number_value, .error = EVAL_ERROR_NONE };
    case FORMULA_AST_NODE_TYPE_CONSTANT:
    {
        constant_t const * const constant = constant_lookup_by_name(tree->data.variable_name);

        if (constant == NULL)
        {
            return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_CONSTANT }; // For unknown constants
        }
        return (EvalResult) { .value = constant->value, .error = EVAL_ERROR_NONE };
    }
    case FORMULA_AST_NODE_TYPE_VARIABLE:
        if (strcmp(tree->data.variable_name, "x") == 0)
        {
            return (EvalResult) { .value = x_value, .error = EVAL_ERROR_NONE };
        }
        return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_VARIABLE }; // For unknown variables
    case FORMULA_AST_NODE_TYPE_BINARY_OPERATOR:
    {
        EvalResult left_res = eval_ast(tree->data.binary_op.left, x_value);
        if (left_res.error != EVAL_ERROR_NONE)
            return left_res;
        EvalResult right_res = eval_ast(tree->data.binary_op.right, x_value);
        if (right_res.error != EVAL_ERROR_NONE)
            return right_res;

        double left_val = left_res.value;
        double right_val = right_res.value;
        char const * op = tree->data.binary_op.operator_name;

        if (strcmp(op, "+") == 0)
            return (EvalResult) { .value = left_val + right_val, .error = EVAL_ERROR_NONE };
        else if (strcmp(op, "-") == 0)
            return (EvalResult) { .value = left_val - right_val, .error = EVAL_ERROR_NONE };
        else if (strcmp(op, "*") == 0)
            return (EvalResult) { .value = left_val * right_val, .error = EVAL_ERROR_NONE };
        else if (strcmp(op, "/") == 0)
        {
            if (right_val == 0.0)
                return (EvalResult) { .error = EVAL_ERROR_DIVISION_BY_ZERO };
            return (EvalResult) { .value = left_val / right_val, .error = EVAL_ERROR_NONE };
        }
        return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_OPERATION };
    }
    case FORMULA_AST_NODE_TYPE_UNARY_OPERATOR:
    {
        EvalResult child_res = eval_ast(tree->data.unary_op.child, x_value);
        if (child_res.error != EVAL_ERROR_NONE)
            return child_res;
        double child_val = child_res.value;
        char const * op = tree->data.unary_op.operator_name;

        if (strcmp(op, "-") == 0)
            return (EvalResult) { .value = -child_val, .error = EVAL_ERROR_NONE };
        return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_OPERATION };
    }
    case FORMULA_AST_NODE_TYPE_FUNCTION_CALL:
    {
        char const * func_name = tree->data.function_call.function_name;
        size_t num_args = tree->data.function_call.num_args;
        FormulaAST * const * args = tree->data.function_call.args;

        function_t const * func = function_lookup_by_name(func_name);
        if (func == NULL)
        {
            return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_OPERATION };
        }
        if (func->num_args != num_args)
        {
            return (EvalResult) { .error = EVAL_ERROR_INVALID_ARGUMENTS };
        }
        // Evaluate arguments
        EvalResult * arg_results = calloc(num_args, sizeof(EvalResult));
        if (!arg_results)
            return (EvalResult) { .error = EVAL_ERROR_UNKNOWN };

        for (size_t i = 0; i < num_args; ++i)
        {
            arg_results[i] = eval_ast(args[i], x_value);
            if (arg_results[i].error != EVAL_ERROR_NONE)
            {
                EvalResult res = arg_results[i];
                free(arg_results);
                return res;
            }
        }

        // Single argument functions
        if (num_args == 1)
        {
            double arg_val = arg_results[0].value;
            double const res = func->unary(arg_val);
            free(arg_results);
            return (EvalResult) { .value = res, .error = EVAL_ERROR_NONE };
        }
        if (num_args == 2)
        {
            double const arg1_val = arg_results[0].value;
            double const arg2_val = arg_results[1].value;
            double const res = func->binary(arg1_val, arg2_val);
            free(arg_results);
            return (EvalResult) { .value = res, .error = EVAL_ERROR_NONE };
        }
        free(arg_results);
        return (EvalResult) { .error = EVAL_ERROR_UNKNOWN_OPERATION };
    }
        // No default or FORMULA_AST_NODE_TYPE_NEGATIVE_NUMBER as it's handled as UNARY_OPERATOR
    }
    return (EvalResult) { .error = EVAL_ERROR_UNKNOWN };
}
