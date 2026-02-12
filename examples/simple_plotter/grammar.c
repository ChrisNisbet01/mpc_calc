#include "grammar.h"

#include "ast.h"
#include "ast_builder.h"
#include "ast_evaluator.h"
#include "function_definitions.h"

#include <stdio.h>

typedef struct make_functions_cb_ctx
{
    epc_parser_list * list;
    epc_parser_t * p_unary_functions;
    epc_parser_t * p_binary_functions;
} make_functions_cb_ctx;

static bool
make_functions_cb(function_t const * const func, void * const ctx)
{
    make_functions_cb_ctx * cb_ctx = ctx;
    epc_parser_list * list = cb_ctx->list;

    if (func->num_args == 1)
    {
        if (cb_ctx->p_unary_functions == NULL)
        {
            cb_ctx->p_unary_functions = epc_string_l(list, func->name, func->name);
        }
        else
        {
            epc_parser_t * fn = epc_string_l(list, func->name, func->name);
            cb_ctx->p_unary_functions = epc_or_l(list, "or_func", 2, cb_ctx->p_unary_functions, fn);
        }
    }
    else if (func->num_args == 2)
    {
        if (cb_ctx->p_binary_functions == NULL)
        {
            cb_ctx->p_binary_functions = epc_string_l(list, func->name, func->name);
        }
        else
        {
            epc_parser_t * fn = epc_string_l(list, func->name, func->name);
            cb_ctx->p_binary_functions = epc_or_l(list, "or_func", 2, cb_ctx->p_binary_functions, fn);
        }
    }

    return false;
}

static epc_parser_t *
make_functions_parser(epc_parser_list * list)
{
    epc_parser_t * functions = NULL;
    make_functions_cb_ctx cb_ctx = {.list = list};
    functions_foreach(make_functions_cb, &cb_ctx);

    if (cb_ctx.p_binary_functions != NULL)
    {
        functions = epc_passthru_l(list, "binary_functions", cb_ctx.p_binary_functions);
    }
    if (cb_ctx.p_unary_functions != NULL)
    {
        if (functions == NULL)
        {
            functions = epc_passthru_l(list, "unary_functions", cb_ctx.p_unary_functions);
        }
        else
        {
            functions = epc_or_l(list, "functions", 2, functions, cb_ctx.p_unary_functions);
        }
    }

    return functions;
}

epc_parser_t *
create_formula_grammar(
    epc_parser_list * list,
    size_t num_variables, variable_t const * variables,
    size_t num_constants, variable_t const * constants
)
{
    // Forward declarations for recursion
    epc_parser_t * expr_fwd   = epc_parser_allocate_l(list, "expr");
    epc_parser_t * term_fwd   = epc_parser_allocate_l(list, "term");
    epc_parser_t * factor_fwd = epc_parser_allocate_l(list, "factor");

    // Literals
    epc_parser_t * number_    = epc_double_l(list, "number");
    epc_parser_t * number     = epc_lexeme_l(list, "number", number_);
    epc_parser_set_ast_action(number_, AST_ACTION_CREATE_NUMBER_FROM_CONTENT);

    // Constants
    epc_parser_t * constant_ = NULL;
    {
        if (num_constants > 0 && constants != NULL)
        {
            variable_t const * c = &constants[0];
            constant_ = epc_string_l(list, c->name, c->name);

            for (size_t i = 1; i < num_constants; i++)
            {
                c = &constants[i];
                epc_parser_t * pconst = epc_string_l(list, c->name, c->name);
                constant_ = epc_or_l(list, c->name, 2, constant_, pconst);
            }
        }
        else
        {
            constant_ = epc_fail_l(list, "no constants", "no constants available");
        }
    }
    epc_parser_t * constant = epc_lexeme_l(list, "constant", constant_);
    epc_parser_set_ast_action(constant, AST_ACTION_CREATE_IDENTIFIER);

    // Variables
    epc_parser_t * variable_ = NULL;
    {
        if (num_variables > 0 && variables != NULL)
        {
            variable_t const * var = &variables[0];
            variable_ = epc_string_l(list, var->name, var->name);

            fprintf(stderr, "%s\n", var->name);

            for (size_t i = 1; i < num_variables; i++)
            {
                var = &variables[i];
                epc_parser_t * pvar = epc_string_l(list, var->name, var->name);
                variable_ = epc_or_l(list, var->name, 2, variable_, pvar);
            }
        }
        else
        {
            variable_ = epc_fail_l(list, "no functions", "no functions available");
        }
    }
    epc_parser_t * variable = epc_lexeme_l(list, "variable", variable_);
    epc_parser_set_ast_action(variable, AST_ACTION_CREATE_IDENTIFIER);

    // Operators
    epc_parser_t * add_op         = epc_char_l(list, "add", '+');
    epc_parser_t * sub_op         = epc_char_l(list, "sub", '-');
    epc_parser_t * add_sub_       = epc_or_l(list, "add_sub", 2, add_op, sub_op);
    epc_parser_t * add_sub        = epc_lexeme_l(list, "add_sub", add_sub_);
    epc_parser_set_ast_action(add_sub, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);

    epc_parser_t * mul_op         = epc_char_l(list, "mul", '*');
    epc_parser_t * div_op         = epc_char_l(list, "div", '/');
    epc_parser_t * mul_div_       = epc_or_l(list, "mul_div", 2, mul_op, div_op);
    epc_parser_t * mul_div        = epc_lexeme_l(list, "mul_div", mul_div_);
    epc_parser_set_ast_action(mul_div, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);

    // Parentheses
    epc_parser_t * lparen_        = epc_char_l(list, "(", '(');
    epc_parser_t * lparen         = epc_lexeme_l(list, "(", lparen_);
    epc_parser_t * rparen_        = epc_char_l(list, ")", ')');
    epc_parser_t * rparen         = epc_lexeme_l(list, ")", rparen_);
    epc_parser_t * expr_in_parens = epc_between_l(list, "parens", lparen, expr_fwd, rparen);

    // Function call
    epc_parser_t * function_call  = NULL;
    epc_parser_t * function_      = make_functions_parser(list);
    if (function_ != NULL)
    {
        epc_parser_set_ast_action(function_, AST_ACTION_CREATE_IDENTIFIER);
        epc_parser_t * function   = epc_lexeme_l(list, "function", function_);
        epc_parser_t * arg_delim_ = epc_char_l(list, ",", ',');
        epc_parser_t * arg_delim  = epc_lexeme_l(list, ",", arg_delim_);
        epc_parser_t * fn_lparen  = epc_char_l(list, "(", '(');
        epc_parser_t * fn_rparen_ = epc_char_l(list, ")", ')');
        epc_parser_t * fn_rparen  = epc_lexeme_l(list, ")", fn_rparen_);

        epc_parser_t * single_arg        = epc_lexeme_l(list, "single_expression_arg", expr_fwd);
        epc_parser_t * many_args         = epc_delimited_l(list, "one_or_more_args", single_arg, arg_delim);
        epc_parser_set_ast_action(many_args, AST_ACTION_COLLECT_CHILD_RESULTS);

        epc_parser_t * zero_or_more_args = epc_optional_l(list, "optional_args_list", many_args);
        epc_parser_set_ast_action(zero_or_more_args, AST_ACTION_PROMOTE_ARGS_LIST_AST_OR_EMPTY_LIST);

        epc_parser_t * args_in_parens    = epc_between_l(list, "args_in_parens", fn_lparen, zero_or_more_args, fn_rparen);
        function_call  = epc_and_l(list, "function_call", 2, function, args_in_parens);
        epc_parser_set_ast_action(function_call, AST_ACTION_CREATE_FUNCTION_CALL);
    }
    else
    {
        function_call = epc_fail_l(list, "no_functions", "no functions available");
    }

    // Base of factor: number | constant | ( expr )
    epc_parser_t * factor_def =
        epc_or_l(
           list,
           "primary",
            5, number, constant, variable, function_call, expr_in_parens
        );
    epc_parser_duplicate(factor_fwd, factor_def);

    // term = factor (mul_div factor)*
    epc_parser_t * term_def = epc_chainl1_l(list, "term", factor_def, mul_div);
    epc_parser_set_ast_action(term_def, AST_ACTION_BUILD_BINARY_EXPRESSION);
    epc_parser_duplicate(term_fwd, term_def);

    // expr = term (add_sub term)*
    epc_parser_t * expr_def = epc_chainl1_l(list, "expr", term_def, add_sub);
    epc_parser_set_ast_action(expr_def, AST_ACTION_BUILD_BINARY_EXPRESSION);
    epc_parser_duplicate(expr_fwd, expr_def);

    // Complete parser
    epc_parser_t *eoi       = epc_eoi_l(list, "eoi");
    epc_parser_t *complete  = epc_and_l(list, "formula", 2, expr_def, eoi);
    epc_parser_set_ast_action(complete, AST_ACTION_ASSIGN_ROOT);

    return complete;
}

void
parse_and_evaluate_result_cleanup(parse_and_evaluate_result_st * result)
{
    if (!result->success)
    {
        free((char *)result->message);
    }
    memset(result, 0, sizeof(*result));
}

static ast_builder_data_t
build_ast(epc_cpt_node_t * pt_node)
{
    ast_builder_data_t ast_builder_data;
    ast_builder_init(&ast_builder_data);

    epc_cpt_visitor_t ast_builder_visitor = {
        .enter_node = ast_builder_enter_node,
        .exit_node = ast_builder_exit_node,
        .user_data = &ast_builder_data
    };

    epc_cpt_visit_nodes(pt_node, &ast_builder_visitor);

    return ast_builder_data;
}

void
compile_context_cleanup(epc_compile_context_st * ctx)
{
    epc_parse_session_destroy(&ctx->parse_session);
    if (!ctx->success)
    {
        free((char *)ctx->message);
    }
    else
    {
        ast_node_free(ctx->ast);
    }
    memset(ctx, 0, sizeof(*ctx));
}

epc_compile_context_st
compile_expression(epc_parser_t * formula_parser, char const * input_expr)
{
    epc_compile_context_st result = {0};

    result.parse_session = epc_parse_input(formula_parser, input_expr);

    if (!result.parse_session.result.is_error)
    {
        ast_builder_data_t ast_builder_data;
        ast_builder_init(&ast_builder_data);

        ast_builder_data = build_ast(result.parse_session.result.data.success);

        if (ast_builder_data.has_error)
        {
            char * msg = NULL;
            int len = asprintf(&msg, "Error: %s", ast_builder_data.error_message);
            if (len < 0)
            {
                msg = strdup("memory allocation error");
            }
            result.message = msg;
        }
        else if (ast_builder_data.ast_root == NULL)
        {
            result.message = strdup("Error: No root AST assigned.");
        }
        else
        {
            result.success = true;
            result.ast = ast_builder_data.ast_root;
            ast_builder_data.ast_root = NULL;
        }
        ast_builder_cleanup(&ast_builder_data);
    }
    else
    {
        char *msg = NULL;
        int len = asprintf(&msg, "Error: %s at '%.*s' (expected '%s', found '%.*s')",
            result.parse_session.result.data.error->message,
            (int)(input_expr + strlen(input_expr) - result.parse_session.result.data.error->input_position),
            result.parse_session.result.data.error->input_position,
            result.parse_session.result.data.error->expected ? result.parse_session.result.data.error->expected : "N/A",
            (int)strlen(result.parse_session.result.data.error->found),
            result.parse_session.result.data.error->found ? result.parse_session.result.data.error->found : "N/A"
        );
        if (len < 0)
        {
            msg = strdup("memory allocation error");
        }
        result.message = msg;
    }

    return result;
}

parse_and_evaluate_result_st
parse_and_evaluate(
    epc_parser_t * formula_parser,
    char const * const input_expr,
    size_t num_variables,
    variable_t const * variables,
    size_t num_constants,
    variable_t const * constants
)
{
    parse_and_evaluate_result_st result = {0};
    epc_compile_context_st compile_result = compile_expression(formula_parser, input_expr);

    if (compile_result.success)
    {
        double calculated_result =
            evaluate_ast(compile_result.ast, variables, num_variables, constants, num_constants);

        result.success = true;
        result.value = calculated_result;
    }
    else
    {
        result.message = compile_result.message;
        compile_result.message = NULL;
    }

    compile_context_cleanup(&compile_result);

    return result;
}

