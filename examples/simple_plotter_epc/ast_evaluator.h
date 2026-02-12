#pragma once

#include "ast.h"
#include <stddef.h>

typedef struct {
    const char* name;
    double value;
} variable_t;

double
evaluate_ast(
    ast_node_t* node,
    const variable_t* variables, size_t var_count,
    const variable_t* constants, size_t const_count
);

