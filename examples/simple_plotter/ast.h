#pragma once

#include "function_definitions.h"

typedef enum {
    AST_NODE_TYPE_PLACEHOLDER,
    AST_NODE_TYPE_NUMBER,
    AST_NODE_TYPE_OPERATOR,
    AST_NODE_TYPE_EXPRESSION,
    AST_NODE_TYPE_LIST,
    AST_NODE_TYPE_FUNCTION_CALL,
    AST_NODE_TYPE_IDENTIFIER,
} ast_node_type_t;

typedef struct ast_node_t ast_node_t;

typedef struct ast_list_node_t {
    ast_node_t* item;
    struct ast_list_node_t* next;
} ast_list_node_t;

typedef struct {
    ast_list_node_t* head;
    ast_list_node_t* tail;
    int count;
} ast_list_t;

typedef struct {
    const function_t* func_def;
    ast_list_t arguments;
} ast_function_call_t;

typedef struct {
    double value;
} ast_number_t;

typedef struct {
    char operator_char;
} ast_operator_t;

typedef struct {
    char operator_char;
} ast_unary_t;

typedef struct {
    ast_node_t* left;
    ast_node_t* operator_node;
    ast_node_t* right;
} ast_expression_t;

typedef struct {
    char const *name;
} ast_identifier_t;

struct ast_node_t {
    ast_node_type_t type;
    union {
        ast_number_t number;
        ast_operator_t op;
        ast_expression_t expression;
        ast_list_t list;
        ast_function_call_t function_call;
        ast_identifier_t identifier;
    } data;
};
