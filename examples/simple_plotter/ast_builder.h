#pragma once

#include "ast.h"
#include "easy_pc/easy_pc.h"

// AST Action Definitions for Semantic Actions
typedef enum {
    AST_ACTION_NONE,
    AST_ACTION_CREATE_NUMBER_FROM_CONTENT, // Create AST_TYPE_NUMBER from parser content (long long)
    AST_ACTION_CREATE_OPERATOR_FROM_CHAR,  // Create AST_TYPE_OPERATOR from parser content (char)
    AST_ACTION_CREATE_UNARY_FROM_CHAR_OR_PLUS, // CREATE AST_TYPE UNARY from parser content if present, else UNARY +
    AST_ACTION_COLLECT_CHILD_RESULTS,      // Collect all AST results from successful children into a list
    AST_ACTION_BUILD_BINARY_EXPRESSION,    // Build binary expression from (left, op_list, right_list)
    AST_ACTION_PROMOTE_LAST_CHILD_AST,     // For structural nodes that just pass through one child's AST
    AST_ACTION_PROMOTE_ARGS_LIST_AST_OR_EMPTY_LIST, // For structural nodes that just pass through one child's AST or an empty list is no child AST is found.
    AST_ACTION_CREATE_IDENTIFIER,          // Create an identifier node (for functions, constants, variables)
    AST_ACTION_CREATE_FUNCTION_CALL,       // For function_call = name '(' args ')'
    AST_ACTION_ASSIGN_ROOT,                // Sets the root node of the AST. (Should be the top of the node stack)
} ast_action_type_t;

#define AST_BUILDER_MAX_STACK_SIZE 128 // Max depth of CPT traversal / nested expressions

// Context for the AST builder, holds the stack and final AST root
typedef struct {
    ast_node_t* stack[AST_BUILDER_MAX_STACK_SIZE];
    int stack_size;
    ast_node_t* ast_root; // The final constructed AST root

    // Error handling
    bool has_error;
    char error_message[256];
} ast_builder_data_t;

// Visitor callbacks
void
ast_builder_enter_node(epc_cpt_node_t* node, void* user_data);

void
ast_builder_exit_node(epc_cpt_node_t* node, void* user_data);

// Function to initialize the AST builder data
void
ast_builder_init(ast_builder_data_t* data);

// Function to clean up the AST builder data after an evaulation
void
ast_builder_cleanup(ast_builder_data_t * data);


// Function to free the root AST node returned after building the AST tree
void
ast_node_free(ast_node_t * node);

