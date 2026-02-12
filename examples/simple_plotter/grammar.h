#pragma once

#include "ast_evaluator.h"

#include "easy_pc/easy_pc.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct parse_and_evaluate_result_st
{
    bool success;
    union {
        double value;
        char const * message;
    };
} parse_and_evaluate_result_st;

typedef struct epc_compile_context_st
{
    bool success;
    epc_parse_session_t parse_session;
    union
    {
        ast_node_t * ast;
        char const * message;
    };
} epc_compile_context_st;


epc_parser_t *
create_formula_grammar(
    epc_parser_list * list,
    size_t num_variables, variable_t const * variables,
    size_t num_constants, variable_t const * constants
);

void
parse_and_evaluate_result_cleanup(parse_and_evaluate_result_st * result);

parse_and_evaluate_result_st
parse_and_evaluate(
    epc_parser_t * formula_parser,
    char const * const input_expr,
    size_t num_variables,
    variable_t const * variables,
    size_t num_constants,
    variable_t const * constants
);

void
compile_context_cleanup(epc_compile_context_st * ctx);

epc_compile_context_st
compile_expression(epc_parser_t * formula_parser, char const * input_expr);

