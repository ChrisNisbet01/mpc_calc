#include <formula_parser.h>

#include "mpc/mpc.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Internal structure for FormulaAST
typedef enum {
    FORMULA_AST_NODE_TYPE_NUMBER,
    FORMULA_AST_NODE_TYPE_VARIABLE,
    FORMULA_AST_NODE_TYPE_BINARY_OPERATOR,
    FORMULA_AST_NODE_TYPE_UNARY_OPERATOR,
    FORMULA_AST_NODE_TYPE_FUNCTION_CALL,
    FORMULA_AST_NODE_TYPE_NEGATIVE_NUMBER, // For handling negative numbers parsed as unary ops
} FormulaASTNodeType;

struct FormulaAST {
    FormulaASTNodeType type;
    union {
        double number_value;
        char *variable_name; // owned by this struct
        struct {
            char *operator_name; // owned by this struct
            struct FormulaAST *left; // owned by this struct
            struct FormulaAST *right; // owned by this struct
        } binary_op;
        struct {
            char *operator_name; // owned by this struct
            struct FormulaAST *child; // owned by this struct
        } unary_op;
        struct {
            char *function_name; // owned by this struct
            struct FormulaAST **args; // array of owned pointers
            size_t num_args;
        } function_call;
    } data;
    // Potentially add line/column information here for error reporting
};

// Forward declaration of internal AST destruction function
static void formula_ast_destroy_internal(FormulaAST *node);


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
};

static FormulaAST *
formula_ast_create_number(double value)
{
    FormulaAST *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = FORMULA_AST_NODE_TYPE_NUMBER;
        node->data.number_value = value;
    }
    return node;
}

static FormulaAST *
formula_ast_create_variable(const char *name)
{
    FormulaAST *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = FORMULA_AST_NODE_TYPE_VARIABLE;
        node->data.variable_name = strdup(name); // Duplicate string, ownership transferred
        if (!node->data.variable_name) {
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_binary_op(const char *op_name, FormulaAST *left, FormulaAST *right)
{
    FormulaAST *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = FORMULA_AST_NODE_TYPE_BINARY_OPERATOR;
        node->data.binary_op.operator_name = strdup(op_name); // Duplicate string
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
        if (!node->data.binary_op.operator_name) {
            // Cleanup on allocation failure
            formula_ast_destroy_internal(left);
            formula_ast_destroy_internal(right);
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_unary_op(const char *op_name, FormulaAST *child)
{
    FormulaAST *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = FORMULA_AST_NODE_TYPE_UNARY_OPERATOR;
        node->data.unary_op.operator_name = strdup(op_name); // Duplicate string
        node->data.unary_op.child = child;
        if (!node->data.unary_op.operator_name) {
            // Cleanup on allocation failure
            formula_ast_destroy_internal(child);
            free(node);
            return NULL;
        }
    }
    return node;
}

static FormulaAST *
formula_ast_create_function_call(const char *func_name, FormulaAST **args, size_t num_args)
{
    FormulaAST *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = FORMULA_AST_NODE_TYPE_FUNCTION_CALL;
        node->data.function_call.function_name = strdup(func_name); // Duplicate string
        node->data.function_call.args = args; // Takes ownership of the array of pointers
        node->data.function_call.num_args = num_args;
        if (!node->data.function_call.function_name) {
            // Cleanup on allocation failure
            for (size_t i = 0; i < num_args; ++i) {
                formula_ast_destroy_internal(args[i]);
            }
            free(args);
            free(node);
            return NULL;
        }
    }
    return node;
}

static void
formula_ast_destroy_internal(FormulaAST *node)
{
    if (NULL == node) {
        return;
    }

    switch (node->type) {
        case FORMULA_AST_NODE_TYPE_NUMBER:
            // No dynamic memory to free
            break;
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
            for (size_t i = 0; i < node->data.function_call.num_args; ++i) {
                formula_ast_destroy_internal(node->data.function_call.args[i]);
            }
            free(node->data.function_call.args);
            break;
        case FORMULA_AST_NODE_TYPE_NEGATIVE_NUMBER:
            // Assuming this is handled as a unary operation with an operator name
            free(node->data.unary_op.operator_name); // If there's a distinct operator string
            formula_ast_destroy_internal(node->data.unary_op.child);
            break;
    }
    free(node);
}

// Public API wrapper for the internal destructor
void
formula_ast_destroy(FormulaAST *ast)
{
    formula_ast_destroy_internal(ast);
}

// MPC callback for numbers: converts a string to a double and creates a FormulaAST number node.
static mpc_val_t *mpc_make_number(mpc_val_t *val) {
    double value = atof((char *)val);
    free(val); // Free the string value from MPC
    return formula_ast_create_number(value);
}

// MPC callback for variables: creates a FormulaAST variable node from a string.
static mpc_val_t *mpc_make_variable(mpc_val_t *val) {
    char *name = (char *)val;
    FormulaAST *node = formula_ast_create_variable(name);
    free(val); // Free the string value from MPC
    return node;
}

// MPC callback for constants: creates a FormulaAST variable node from a string (e.g., "pi", "e").
static mpc_val_t *mpc_make_constant(mpc_val_t *val) {
    char *name = (char *)val;
    FormulaAST *node = formula_ast_create_variable(name); // Treat constants as variables for evaluation.
    free(val); // Free the string value from MPC
    return node;
}

// MPC callback for binary operations (mpc_fold_t)
// Assumes a flat list of (left_operand_AST, operator_string, right_operand_AST, operator_string, right_operand_AST...)
// The function is responsible for freeing all input mpc_val_t's that are consumed and not part of the returned AST.
static mpc_val_t *mpc_make_binary_op(int n_args, mpc_val_t **args) {
    FormulaAST *left_ast = (FormulaAST *)args[0]; // First operand AST. This will be returned, so don't free its mpc_val_t.

    for (int i = 1; i < n_args; i += 2) {
        char *op_name = (char *)args[i]; // Operator string. This is consumed and copied.
        FormulaAST *right_ast = (FormulaAST *)args[i+1]; // Right operand AST. This is consumed.

        left_ast = formula_ast_create_binary_op(op_name, left_ast, right_ast);
        free(op_name); // Free the operator string (mpc_val_t was a char*, we strdup'd it)
        // No need to free right_ast here as it's now owned by left_ast
    }
    return left_ast; // The `mpc_val_t` returned is a `FormulaAST*` which is now the root of this sub-tree.
                     // The original `args[0]` (left_ast) was either the initial value or a new binary op node.
                     // All other `mpc_val_t*`s in `args` (operators and right_ast) are now conceptually handled.
                     // No further explicit free needed in `mpc_fold_t` if children are 'moved'.
}

// MPC callback for function calls (mpc_fold_t)
// E.g., for "pow(expr, expr)": [ str("pow"), char("("), expr1_ast, char(","), expr2_ast, char(")") ]
static mpc_val_t *mpc_make_function_call(int n_args, mpc_val_t **args) {
    char *func_name_str = (char *)args[0]; // Function name string

    // Calculate the number of arguments.
    // Structure: func_name, '(', arg1, (',', arg2)*, ')'
    // n_args:
    // func() -> 3 (func_name, '(', ')') -> num_func_args = 0
    // func(arg1) -> 4 (func_name, '(', arg1, ')') -> num_func_args = 1
    // func(arg1, arg2) -> 6 (func_name, '(', arg1, ',', arg2, ')') -> num_func_args = 2
    size_t num_func_args = 0;
    if (n_args > 3) { // If there are arguments (e.g., func(arg1, ...))
        num_func_args = (size_t)((n_args - 3 + 1) / 2); // N_args-3: removes func_name, '(', ')'. Add 1 for the first arg. Divide by 2 to account for comma+arg pairs.
    }


    FormulaAST **func_ast_args = NULL;
    if (num_func_args > 0) {
        func_ast_args = calloc(num_func_args, sizeof(FormulaAST *));
        if (!func_ast_args) {
            // Handle error: free everything that would have been freed below.
            free(func_name_str);
            for (int i = 1; i < n_args; ++i) { free(args[i]); } // Free all char* literals
            return NULL;
        }

        size_t current_arg_idx = 0;
        for (int i = 2; i < n_args - 1; i += 2) { // Arguments start at index 2, then 4, 6...
            func_ast_args[current_arg_idx++] = (FormulaAST *)args[i];
        }
    }

    FormulaAST *node = formula_ast_create_function_call(func_name_str, func_ast_args, num_func_args);

    free(func_name_str); // Free the original string content from the function name parser

    // Free all character literals from MPC that are now consumed
    for (int i = 1; i < n_args; ++i) {
        // If args[i] is NOT one of the actual FormulaAST arguments, then it's a character literal to free.
        bool is_func_ast_arg = false;
        for (size_t j = 0; j < num_func_args; ++j) {
            if ((FormulaAST *)args[i] == func_ast_args[j]) {
                is_func_ast_arg = true;
                break;
            }
        }
        if (!is_func_ast_arg) {
            free(args[i]);
        }
    }

    return node;
}

// Struct to hold an operator string and its corresponding operand AST node
// This struct is internal to the handling of binary operations.
typedef struct {
    char *op_name;
    FormulaAST *operand;
} BinaryOpPart;

// Destructor for BinaryOpPart
static void mpc_destroy_binary_op_part(mpc_val_t *val) {
    BinaryOpPart *part = (BinaryOpPart *)val;
    if (part) {
        free(part->op_name);
        formula_ast_destroy_internal(part->operand);
        free(part);
    }
}

// MPC fold callback for the inner mpc_and (e.g., ("*" <factor>))
// It combines the operator string and the operand AST into a BinaryOpPart struct.
static mpc_val_t *mpc_make_binary_op_part(int n_args, mpc_val_t **args) {
    // Expects: [op_str, operand_ast]. n_args should be 2.
    if (n_args != 2) {
        // This should not happen with correct grammar
        if (args[0]) free(args[0]);
        if (args[1]) formula_ast_destroy_internal((FormulaAST*)args[1]);
        return NULL;
    }

    char *op_name = (char *)args[0]; // Takes ownership from mpc parser
    FormulaAST *operand_ast = (FormulaAST *)args[1]; // Takes ownership from mpc parser

    BinaryOpPart *part = malloc(sizeof(BinaryOpPart));
    if (!part) {
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
static mpc_val_t *mpc_collect_binary_op_parts(int n_args, mpc_val_t **args) {
    // args is an array of BinaryOpPart*
    // We need to return an array of BinaryOpPart* that the caller can iterate over.
    // This is essentially reimplementing a simplified vector.
    BinaryOpPart **parts_array = calloc(n_args + 1, sizeof(void*)); // +1 for count at index 0
    if (!parts_array) {
        for (int i = 0; i < n_args; ++i) {
            mpc_destroy_binary_op_part(args[i]);
        }
        return NULL;
    }

    // Store the count in the first element.
    parts_array[0] = (BinaryOpPart *)(long)n_args; // Cast int to void* using long to avoid warnings
    for (int i = 0; i < n_args; ++i) {
        parts_array[i+1] = (BinaryOpPart *)args[i]; // Store the pointers
    }

    return (mpc_val_t *)parts_array; // Return as mpc_val_t*
}

// Destructor for the array returned by mpc_collect_binary_op_parts
static void mpc_destroy_collected_binary_op_parts(mpc_val_t *val) {
    BinaryOpPart **parts_array = (BinaryOpPart **)val;
    if (parts_array) {
        int count = (int)(long)parts_array[0]; // Retrieve count
        for (int i = 0; i < count; ++i) {
            mpc_destroy_binary_op_part(parts_array[i+1]);
        }
        free(parts_array);
    }
}

// Struct to hold an operator and its corresponding operand AST node
typedef struct {
    char *op_name;
    FormulaAST *operand;
} OpOperandPair;

// MPC callback to create an OpOperandPair (for use with mpc_and)
static mpc_val_t *mpc_make_op_operand_pair(int n_args, mpc_val_t **args) {
    // Expects: [op_str, operand_ast]. n_args should be 2.
    if (n_args != 2) {
        // Handle error, this shouldn't happen with correct grammar
        // free(args[0]); // Free operator string
        // formula_ast_destroy_internal((FormulaAST *)args[1]); // Free operand AST
        return NULL; // Should not happen with correct grammar, but for safety
    }

    char *op_name = (char *)args[0];
    FormulaAST *operand_ast = (FormulaAST *)args[1];

    OpOperandPair *pair = malloc(sizeof(OpOperandPair));
    if (!pair) {
        free(op_name);
        formula_ast_destroy_internal(operand_ast);
        return NULL;
    }
    pair->op_name = op_name; // Take ownership of the string
    pair->operand = operand_ast; // Take ownership of the AST node
    return pair;
}

// Destructor for OpOperandPair (for mpc_dtor_t)
static void mpc_destroy_op_operand_pair(mpc_val_t *val) {
    OpOperandPair *pair = (OpOperandPair *)val;
    if (pair) {
        free(pair->op_name);
        formula_ast_destroy_internal(pair->operand);
        free(pair);
    }
}

// MPC fold callback for left-associative binary operations.
// args will be [initial_ast, collected_binary_op_parts_array]
// where collected_binary_op_parts_array is (BinaryOpPart**) from mpc_collect_binary_op_parts
static mpc_val_t *mpc_fold_left_associative_binary_op(int n_args, mpc_val_t **args) {
    // n_args will be 2: args[0] is initial_ast, args[1] is (BinaryOpPart**) from mpc_collect_binary_op_parts
    FormulaAST *left_ast = (FormulaAST *)args[0];
    BinaryOpPart **parts_array = (BinaryOpPart **)args[1];

    if (parts_array) {
        int count = (int)(long)parts_array[0]; // Retrieve count
        for (int i = 0; i < count; ++i) {
            BinaryOpPart *part = parts_array[i+1];
            left_ast = formula_ast_create_binary_op(part->op_name, left_ast, part->operand);
            free(part->op_name); // Free the operator string
            free(part); // Free the BinaryOpPart struct
        }
        free(parts_array); // Free the array holding the BinaryOpPart pointers
    }
    return left_ast;
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

    // Define individual parsers using mpc_* combinators and our custom callbacks

    // Numbers
    mpc_define(f->Float, mpc_apply((mpc_apply_t)mpc_make_number, mpc_re("-?[0-9]+\\.[0-9]+")));
    mpc_define(f->Int, mpc_apply((mpc_apply_t)mpc_make_number, mpc_re("-?[0-9]+")));
    mpc_define(f->Number, mpc_or(2, mpc_copy(f->Float), mpc_copy(f->Int))); // 'number' just passes through float or int AST

    // Variable
    mpc_define(f->Variable, mpc_apply((mpc_apply_t)mpc_make_variable, mpc_string("x")));

    // Constant (treated as variables for evaluation as per current eval logic)
    mpc_define(f->Constant, mpc_apply((mpc_apply_t)mpc_make_constant, mpc_or(2, mpc_string("pi"), mpc_string("e"))));

    // Factor rules: order matters (longest match first)
    mpc_define(f->Factor, mpc_or(12, // Increased count due to added function variants
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("log10"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // log10(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("log"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // log(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("acos"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // acos(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("asin"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // asin(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("atan"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // atan(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("cos"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // cos(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("sin"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // sin(expr)
        mpc_and(4, (mpc_fold_t)mpc_make_function_call, mpc_string("tan"), mpc_char('('), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy), // tan(expr)
        mpc_and(6, (mpc_fold_t)mpc_make_function_call, mpc_string("pow"), mpc_char('('), mpc_copy(f->Expr), mpc_char(','), mpc_copy(f->Expr), mpc_char(')'), free, free, (mpc_dtor_t)formula_ast_destroy, free, (mpc_dtor_t)formula_ast_destroy), // pow(expr, expr)
        mpc_parens(mpc_copy(f->Expr), (mpc_dtor_t)formula_ast_destroy), // Parenthesized expression
        mpc_copy(f->Number),    // Number (must be after functions to avoid partial matches)
        mpc_copy(f->Constant),  // Constant
        mpc_copy(f->Variable)   // Variable
    ));


    // Term (multiplication and division)
    // term : <factor> (('*' | '/') <factor>)* ;
    mpc_define(f->Term, mpc_and(2, (mpc_fold_t)mpc_fold_left_associative_binary_op,
        mpc_copy(f->Factor), // The initial factor
        mpc_many((mpc_fold_t)mpc_collect_binary_op_parts, mpc_and(2, (mpc_fold_t)mpc_make_binary_op_part, mpc_oneof("*/"), mpc_copy(f->Factor), free, (mpc_dtor_t)formula_ast_destroy)), // Collects list of (op_str, factor_ast) parts into a custom array
        (mpc_dtor_t)formula_ast_destroy // Destructor for initial factor if parsing fails
    ));


    // Expression (addition and subtraction)
    // expr : <term> (('+' | '-') <term>)* ;
    mpc_define(f->Expr, mpc_and(2, (mpc_fold_t)mpc_fold_left_associative_binary_op,
        mpc_copy(f->Term), // The initial term
        mpc_many((mpc_fold_t)mpc_collect_binary_op_parts, mpc_and(2, (mpc_fold_t)mpc_make_binary_op_part, mpc_oneof("+-"), mpc_copy(f->Term), free, (mpc_dtor_t)formula_ast_destroy)), // Collects list of (op_str, term_ast) parts into a custom array
        (mpc_dtor_t)formula_ast_destroy // Destructor for initial term if parsing fails
    ));

    // Formula (start and end of input)
    mpc_define(f->Formula, mpc_and(3, mpcf_snd_free, mpc_soi(), mpc_copy(f->Expr), mpc_eoi(), mpcf_dtor_null, (mpc_dtor_t)formula_ast_destroy)); // Keep only the Expr AST


    mpc_result_t parse_result;

    if (mpc_parse("<input>", formula, f->Formula, &parse_result))
    {
        // Now parse_result.output should directly be a FormulaAST*
        f->ast = (FormulaAST *)parse_result.output;
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
        return (EvalResult){.error = EVAL_ERROR_UNKNOWN}; // Or specific error for null AST
    }

    switch (tree->type) {
        case FORMULA_AST_NODE_TYPE_NUMBER:
            return (EvalResult){.value = tree->data.number_value, .error = EVAL_ERROR_NONE};
        case FORMULA_AST_NODE_TYPE_VARIABLE:
            if (strcmp(tree->data.variable_name, "x") == 0) {
                return (EvalResult){.value = x_value, .error = EVAL_ERROR_NONE};
            } else if (strcmp(tree->data.variable_name, "pi") == 0) {
                return (EvalResult){.value = M_PI, .error = EVAL_ERROR_NONE};
            } else if (strcmp(tree->data.variable_name, "e") == 0) {
                return (EvalResult){.value = M_E, .error = EVAL_ERROR_NONE};
            } else {
                return (EvalResult){.error = EVAL_ERROR_UNKNOWN_CONSTANT}; // For unknown variables
            }
        case FORMULA_AST_NODE_TYPE_BINARY_OPERATOR:
            {
                EvalResult left_res = eval_ast(tree->data.binary_op.left, x_value);
                if (left_res.error != EVAL_ERROR_NONE) return left_res;
                EvalResult right_res = eval_ast(tree->data.binary_op.right, x_value);
                if (right_res.error != EVAL_ERROR_NONE) return right_res;

                double left_val = left_res.value;
                double right_val = right_res.value;
                char const *op = tree->data.binary_op.operator_name;

                if (strcmp(op, "+") == 0) return (EvalResult){.value = left_val + right_val, .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "-") == 0) return (EvalResult){.value = left_val - right_val, .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "*") == 0) return (EvalResult){.value = left_val * right_val, .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "/") == 0) {
                    if (right_val == 0.0) return (EvalResult){.error = EVAL_ERROR_DIVISION_BY_ZERO};
                    return (EvalResult){.value = left_val / right_val, .error = EVAL_ERROR_NONE};
                }
                return (EvalResult){.error = EVAL_ERROR_UNKNOWN_OPERATION};
            }
        case FORMULA_AST_NODE_TYPE_UNARY_OPERATOR:
            {
                EvalResult child_res = eval_ast(tree->data.unary_op.child, x_value);
                if (child_res.error != EVAL_ERROR_NONE) return child_res;
                double child_val = child_res.value;
                char const *op = tree->data.unary_op.operator_name;

                if (strcmp(op, "cos") == 0) return (EvalResult){.value = cos(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "sin") == 0) return (EvalResult){.value = sin(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "tan") == 0) return (EvalResult){.value = tan(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "acos") == 0) return (EvalResult){.value = acos(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "asin") == 0) return (EvalResult){.value = asin(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "atan") == 0) return (EvalResult){.value = atan(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "log") == 0) return (EvalResult){.value = log(child_val), .error = EVAL_ERROR_NONE};
                else if (strcmp(op, "log10") == 0) return (EvalResult){.value = log10(child_val), .error = EVAL_ERROR_NONE};
                return (EvalResult){.error = EVAL_ERROR_UNKNOWN_OPERATION};
            }
        case FORMULA_AST_NODE_TYPE_FUNCTION_CALL:
            {
                char const *func_name = tree->data.function_call.function_name;
                size_t num_args = tree->data.function_call.num_args;
                FormulaAST * const *args = tree->data.function_call.args;

                if (strcmp(func_name, "pow") == 0) {
                    if (num_args != 2) return (EvalResult){.error = EVAL_ERROR_UNKNOWN_OPERATION}; // Pow expects 2 args
                    EvalResult arg1_res = eval_ast(args[0], x_value);
                    if (arg1_res.error != EVAL_ERROR_NONE) return arg1_res;
                    EvalResult arg2_res = eval_ast(args[1], x_value);
                    if (arg2_res.error != EVAL_ERROR_NONE) return arg2_res;
                    return (EvalResult){.value = pow(arg1_res.value, arg2_res.value), .error = EVAL_ERROR_NONE};
                }
                return (EvalResult){.error = EVAL_ERROR_UNKNOWN_OPERATION};
            }
        case FORMULA_AST_NODE_TYPE_NEGATIVE_NUMBER: // This type might be redundant if '-' is just a unary op
             {
                EvalResult child_res = eval_ast(tree->data.unary_op.child, x_value);
                if (child_res.error != EVAL_ERROR_NONE) return child_res;
                return (EvalResult){.value = -child_res.value, .error = EVAL_ERROR_NONE};
            }
    }
    return (EvalResult){.error = EVAL_ERROR_UNKNOWN};
}

