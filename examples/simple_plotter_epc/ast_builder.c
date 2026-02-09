#include "ast_builder.h"
#include "ast.h"
#include "function_definitions.h"

#include <stdbool.h>
#include <stdlib.h> // For strtod
#include <string.h> // For strncpy, memset
#include <stdio.h>  // For fprintf (debug)
#include <stdarg.h> // For va_list, etc.
#include <math.h> // for M_PI, M_E

// Add this at the top of the file for debug context
#define AST_DEBUG_PRINT_ENABLE 0 // Set to 1 to enable debug prints

#if AST_DEBUG_PRINT_ENABLE
#define AST_DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define AST_DEBUG_PRINT(...) do {} while (0)
#endif

// --- Error Handling ---

static void set_error(ast_builder_data_t * data, epc_cpt_node_t * pt_node, const char * format, ...)
{
    if (data->has_error)
    {
        return; // Don't overwrite the first error
    }

    data->has_error = true;
    /* TODO: Is there a memory leak here if ast_root is non-NULL? */
    data->ast_root = NULL; // Ensure ast_root is NULL on error

    va_list args;
    va_start(args, format);
    char specific_message[128];
    vsnprintf(specific_message, sizeof(specific_message), format, args);
    va_end(args);

    if (pt_node)
    {
        snprintf(data->error_message, sizeof(data->error_message),
                 "AST build error at node '%.*s': %s",
                 (int)pt_node->len, pt_node->content,
                 specific_message);
    }
    else
    {
        snprintf(data->error_message, sizeof(data->error_message),
                 "AST build error: %s", specific_message);
    }
}


// --- Helper Functions for ast_node_t management ---
static void
ast_node_free(ast_node_t * node)
{
    if (node == NULL)
    {
        return;
    }

    switch(node->type)
    {
        case AST_NODE_TYPE_NUMBER:
        case AST_NODE_TYPE_OPERATOR:
        case AST_NODE_TYPE_PLACEHOLDER:
            /* Nothing to do. */
            break;

        case AST_NODE_TYPE_EXPRESSION:
            ast_node_free(node->data.expression.left);
            ast_node_free(node->data.expression.operator_node);
            ast_node_free(node->data.expression.right);
            break;

        case AST_NODE_TYPE_LIST:
        {
            if (node->data.list.count > 0)
            {
                ast_list_node_t * item = node->data.list.head;
                while (item != NULL)
                {
                    ast_list_node_t * next_item = item->next;
                    ast_node_free(item->item);
                    free(item);
                    item = next_item;
                }
            }
            break;
        }

        case AST_NODE_TYPE_FUNCTION_CALL:
        {
            ast_list_t * list = &node->data.function_call.arguments;
            ast_list_node_t * item = list->head;

            while (item != NULL)
            {
                ast_list_node_t * next_item = item->next;
                ast_node_free(item->item);
                free(item);
                item = next_item;
            }
            break;
        }

        case AST_NODE_TYPE_IDENTIFIER:
            free((char *)node->data.identifier.name);
            break;

    }

    free(node);
}

static ast_node_t *
ast_node_alloc(ast_builder_data_t * data, ast_node_type_t node_type)
{
    if (data->has_error)
    {
        return NULL;
    }
    ast_node_t * node = calloc(1, sizeof(*node));
    if (node == NULL)
    {
        set_error(data, NULL, "Failed to allocate AST node");
    }
    else
    {
        node->type = node_type;
    }

    return node;
}

static void
ast_stack_push(ast_builder_data_t * data, ast_node_t * node)
{
    if (data->has_error)
    {
        return;
    }
    if (node == NULL)
    {
        set_error(data, NULL, "attempted to push NULL node onto AST stack");
    }
    AST_DEBUG_PRINT("[AST_STACK] PUSH (before): size=%d, node=%p, type=%d\n", data->stack_size, node, node ? node->type : -1);
    if (data->stack_size < AST_BUILDER_MAX_STACK_SIZE)
    {
        data->stack[data->stack_size++] = node;
    }
    else
    {
        set_error(data, NULL, "AST Builder Stack Overflow");
    }
    AST_DEBUG_PRINT("[AST_STACK] PUSH (after): size=%d\n", data->stack_size);
}

static ast_node_t *
ast_stack_pop(ast_builder_data_t * data)
{
    if (data->has_error)
    {
        return NULL;
    }
    AST_DEBUG_PRINT("[AST_STACK] POP (before): size=%d\n", data->stack_size);
    if (data->stack_size > 0)
    {
        ast_node_t * node = data->stack[--data->stack_size];
        AST_DEBUG_PRINT("[AST_STACK] POP (after): size=%d, node=%p, type=%d\n", data->stack_size, node, node ? node->type : -1);
        return node;
    }
    set_error(data, NULL, "AST Builder Stack Underflow");
    return NULL;
}

// Helper functions for AST list management
static void
ast_list_init(ast_list_t * list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

static void
ast_list_append(ast_builder_data_t * data, ast_list_t * list, ast_node_t * item)
{
    if (data->has_error)
        return;
    ast_list_node_t * new_node = (ast_list_node_t *)calloc(1, sizeof(*new_node));
    if (new_node == NULL)
    {
        set_error(data, NULL, "Failed to allocate list node");
        return;
    }
    new_node->item = item;
    new_node->next = NULL;

    if (list->tail)
    {
        list->tail->next = new_node;
    }
    else
    {
        list->head = new_node;
    }
    list->tail = new_node;
    list->count++;
}

static bool
is_placeholder_node(ast_node_t * node)
{
    return node == NULL || node->type == AST_NODE_TYPE_PLACEHOLDER;
}

static void
push_placeholder_node(ast_builder_data_t * data)
{
    ast_node_t * placeholder_node =
        ast_node_alloc(data, AST_NODE_TYPE_PLACEHOLDER);
    if (placeholder_node == NULL)
    {
        return;
    }
    ast_stack_push(data, placeholder_node);
}

// --- AST Builder Functions ---

void ast_builder_init(ast_builder_data_t * data)
{
    memset(data, 0, sizeof(*data));
}

void
ast_builder_cleanup(ast_builder_data_t * data)
{
    data->has_error = false;
    if (data->stack_size > 0)
    {
        fprintf(stderr,
                "Warning: AST builder stack not empty - still has %u AST nodes pushed\n",
                data->stack_size
        );
        for (int i = 0; i < data->stack_size; i++)
        {
            ast_node_free(data->stack[i]);
        }
    }

    data->stack_size = 0;
    ast_node_free(data->ast_root);
    data->ast_root = NULL;
}

void
ast_builder_enter_node(epc_cpt_node_t * pt_node, void * user_data)
{
    ast_builder_data_t * data = (ast_builder_data_t *)user_data;
    if (data->has_error || pt_node == NULL)
    {
        return;
    }

    epc_ast_semantic_action_t config = epc_node_config_get(pt_node);

    AST_DEBUG_PRINT("[AST_BUILDER] ENTER Node: tag='%s', name='%s', content='%.*s', len=%zu, action=%d\n",
                    pt_node->tag, pt_node->name, (int)pt_node->len, pt_node->content, pt_node->len, config.action);

    switch (config.action)
    {
    case AST_ACTION_CREATE_NUMBER_FROM_CONTENT:
    {
        ast_node_t * num_node = ast_node_alloc(data, AST_NODE_TYPE_NUMBER);
        if (num_node == NULL)
        {
            return;
        }
        char num_str_buf[pt_node->len + 1];
        strncpy(num_str_buf, pt_node->content, pt_node->len);
        num_str_buf[pt_node->len] = '\0';
        num_node->data.number.value = strtod(num_str_buf, NULL);
        ast_stack_push(data, num_node);
        break;
    }
    case AST_ACTION_CREATE_OPERATOR_FROM_CHAR:
    {
        ast_node_t * op_node = ast_node_alloc(data, AST_NODE_TYPE_OPERATOR);
        if (op_node == NULL)
        {
            return;
        }
        op_node->data.op.operator_char = pt_node->content[0];
        ast_stack_push(data, op_node);
        break;
    }
    case AST_ACTION_CREATE_IDENTIFIER:
    case AST_ACTION_COLLECT_CHILD_RESULTS:
    case AST_ACTION_BUILD_BINARY_EXPRESSION:
    case AST_ACTION_PROMOTE_LAST_CHILD_AST:
    case AST_ACTION_PROMOTE_LAST_CHILD_AST_OR_EMPTY_LIST:
    case AST_ACTION_ASSIGN_ROOT:
    case AST_ACTION_CREATE_FUNCTION_CALL:
    {
        push_placeholder_node(data);
        break;
    }

    case AST_ACTION_NONE:
        break;
    }
}


static ast_node_t *
build_binary_tree(ast_builder_data_t * data, ast_node_t * first_operand, ast_list_t * op_operand_pairs)
{
    if (data->has_error || first_operand == NULL)
    {
        return NULL;
    }

    ast_node_t * current_expr_node = first_operand;
    ast_list_node_t * current_pair_node = op_operand_pairs->head;

    while (current_pair_node != NULL)
    {
        ast_node_t * op_and_operand_list = current_pair_node->item;
        if (op_and_operand_list == NULL
            || op_and_operand_list->type != AST_NODE_TYPE_LIST
            || op_and_operand_list->data.list.count != 2)
        {
            if (current_expr_node != first_operand)
            {
                ast_node_free(current_expr_node);
            }
            set_error(data, NULL, "Malformed op_operand_pair list item");
            return NULL;
        }

        ast_node_t * op_node = op_and_operand_list->data.list.head->item;
        ast_node_t * next_operand_node = op_and_operand_list->data.list.head->next->item;

        if (!op_node || op_node->type != AST_NODE_TYPE_OPERATOR || !next_operand_node)
        {
            if (current_expr_node != first_operand)
            {
                ast_node_free(current_expr_node);
            }
            set_error(data, NULL, "Missing operator or operand in pair");
            return NULL;
        }

        ast_node_t * new_expr_node = ast_node_alloc(data, AST_NODE_TYPE_EXPRESSION);
        if (new_expr_node == NULL)
        {
            if (current_expr_node != first_operand)
            {
                ast_node_free(current_expr_node);
            }
            return NULL;
        }
        new_expr_node->data.expression.left = current_expr_node;

        new_expr_node->data.expression.operator_node = op_node;
        op_and_operand_list->data.list.head->item = NULL;

        new_expr_node->data.expression.right = next_operand_node;
        op_and_operand_list->data.list.head->next->item = NULL;

        current_expr_node = new_expr_node;
        current_pair_node = current_pair_node->next;
    }
    return current_expr_node;
}


void ast_builder_exit_node(epc_cpt_node_t * pt_node, void * user_data)
{
    ast_builder_data_t * data = (ast_builder_data_t *)user_data;
    if (data->has_error || pt_node == NULL)
    {
        return;
    }
    epc_ast_semantic_action_t config = epc_node_config_get(pt_node);

    AST_DEBUG_PRINT("[AST_BUILDER] EXIT Node: tag='%s', name='%s', content='%.*s', len=%zu, action=%d\n",
                    pt_node->tag, pt_node->name, (int)pt_node->len, pt_node->content, pt_node->len, config.action);

    switch (config.action)
    {
    case AST_ACTION_BUILD_BINARY_EXPRESSION:
    {
        ast_node_t * op_operand_pairs_list_node = ast_stack_pop(data);
        if (op_operand_pairs_list_node == NULL)
        {
            return;
        }
        ast_node_t * initial_operand_node = ast_stack_pop(data);
        if (initial_operand_node == NULL)
        {
            ast_node_free(op_operand_pairs_list_node);
            return;
        }
        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            set_error(data, pt_node, "Internal error: bad placeholder for BUILD_BINARY_EXPRESSION");
            ast_node_free(op_operand_pairs_list_node);
            ast_node_free(initial_operand_node);
            ast_node_free(own_placeholder);
            return;
        }

        ast_node_free(own_placeholder);

        ast_node_t * final_expression_node;

        if (op_operand_pairs_list_node
            && op_operand_pairs_list_node->type == AST_NODE_TYPE_LIST
            && op_operand_pairs_list_node->data.list.count > 0)
        {
            final_expression_node = build_binary_tree(
                data,
                initial_operand_node,
                &op_operand_pairs_list_node->data.list
            );
        }
        else
        {
            final_expression_node = initial_operand_node;
        }

        if (final_expression_node != NULL)
        {
            ast_stack_push(data, final_expression_node);
            ast_node_free(op_operand_pairs_list_node);
        }
        else
        {
            ast_node_free(initial_operand_node);
            ast_node_free(op_operand_pairs_list_node);
            if (!data->has_error)
            {
                set_error(data, pt_node, "Failed to build binary expression");
            }
        }
        break;
    }

    case AST_ACTION_COLLECT_CHILD_RESULTS:
    {
        ast_node_t * temp_collected_items[AST_BUILDER_MAX_STACK_SIZE];
        size_t temp_count = 0;

        ast_node_t * popped_node;
        while (!data->has_error
               && (popped_node = ast_stack_pop(data)) != NULL
               && !is_placeholder_node(popped_node))
        {
            temp_collected_items[temp_count++] = popped_node;
        }
        if (data->has_error)
        {
            for (size_t i = 0; i < temp_count; i++)
            {
                ast_node_free(temp_collected_items[i]);
            }
            return;
        }

        if (popped_node == NULL)
        {
            for (size_t i = 0; i < temp_count; i++)
            {
                ast_node_free(temp_collected_items[i]);
            }
            set_error(data, pt_node, "Internal error: bad placeholder for COLLECT_CHILD_RESULTS");
            return;
        }
        /* popped_node will be the placeholder node. */
        ast_node_free(popped_node);

        ast_node_t * collected_list_node = ast_node_alloc(data, AST_NODE_TYPE_LIST);
        if (collected_list_node == NULL)
        {
            for (size_t i = 0; i < temp_count; i++)
            {
                ast_node_free(temp_collected_items[i]);
            }
            return;
        }
        ast_list_init(&collected_list_node->data.list);

        for (size_t i = 0; i < temp_count; ++i)
        {
            ast_list_append(data, &collected_list_node->data.list, temp_collected_items[temp_count - 1 - i]);
        }
        ast_stack_push(data, collected_list_node);
        break;
    }

    case AST_ACTION_PROMOTE_LAST_CHILD_AST_OR_EMPTY_LIST:
    {
        ast_node_t * child_ast = ast_stack_pop(data);
        if (child_ast == NULL)
        {
            return;
        }

        if (is_placeholder_node(child_ast))
        {
            /* Then must be an empty args list. */
            ast_node_t * collected_list_node = ast_node_alloc(data, AST_NODE_TYPE_LIST);
            if (collected_list_node == NULL)
            {
                ast_node_free(child_ast);
                return;
            }

            ast_list_init(&collected_list_node->data.list);
            ast_stack_push(data, collected_list_node);
            return;
        }

        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            ast_node_free(own_placeholder);
            ast_node_free(child_ast);
            set_error(data, pt_node, "Internal error: bad placeholder for PROMOTE_LAST_CHILD_AST_OR_EMPTY_LIST");
            return;
        }

        ast_node_free(own_placeholder);

        ast_stack_push(data, child_ast);
        break;
    }

    case AST_ACTION_PROMOTE_LAST_CHILD_AST:
    {
        ast_node_t * child_ast = ast_stack_pop(data);
        if (child_ast == NULL)
        {
            return;
        }

        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            ast_node_free(own_placeholder);
            ast_node_free(child_ast);
            set_error(data, pt_node, "Internal error: bad placeholder for PROMOTE_LAST_CHILD_AST");
            return;
        }

        ast_node_free(own_placeholder);

        ast_stack_push(data, child_ast);
        break;
    }

    case AST_ACTION_ASSIGN_ROOT:
    {
        ast_node_t * child_ast = ast_stack_pop(data);
        if (child_ast == NULL)
        {
            return;
        }
        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            ast_node_free(child_ast);
            ast_node_free(own_placeholder);
            set_error(data, pt_node, "Internal error: bad placeholder for ASSIGN_ROOT");
            return;
        }

        ast_node_free(own_placeholder);

        data->ast_root = child_ast;
        break;
    }

    case AST_ACTION_CREATE_FUNCTION_CALL:
    {
        ast_node_t * args_list_node = ast_stack_pop(data);
        if (args_list_node == NULL || args_list_node->type != AST_NODE_TYPE_LIST)
        {
            ast_node_free(args_list_node);
            set_error(data, pt_node, "Expected arguments list on stack");
            return;
        }
        ast_node_t * func_name_node = ast_stack_pop(data);
        if (func_name_node == NULL || func_name_node->type != AST_NODE_TYPE_IDENTIFIER)
        {
            ast_node_free(args_list_node);
            ast_node_free(func_name_node);
            set_error(data, pt_node, "Expected function name identifier on stack");
            return;
        }
        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            ast_node_free(args_list_node);
            ast_node_free(func_name_node);
            ast_node_free(own_placeholder);
            set_error(data, pt_node, "Internal error: bad placeholder for CREATE_FUNCTION_CALL");
            return;
        }

        ast_node_free(own_placeholder);

        const char * func_name_str = func_name_node->data.identifier.name;
        const function_t * func_def = function_lookup_by_name(func_name_str);

        if (!func_def)
        {
            ast_node_free(args_list_node);
            ast_node_free(func_name_node);
            set_error(data, pt_node, "Unknown function '%s'", func_name_str);
            return;
        }
        if (func_def->num_args != (size_t)args_list_node->data.list.count)
        {
            ast_node_free(args_list_node);
            ast_node_free(func_name_node);
            set_error(data, pt_node, "Function '%s' expects %zu args, got %d", func_def->name, func_def->num_args, args_list_node->data.list.count);
            return;
        }

        ast_node_t * func_call_node = ast_node_alloc(data, AST_NODE_TYPE_FUNCTION_CALL);
        if (func_call_node == NULL)
        {
            ast_node_free(args_list_node);
            ast_node_free(func_name_node);
            return;
        }
        func_call_node->data.function_call.func_def = func_def;
        func_call_node->data.function_call.arguments = args_list_node->data.list;

        args_list_node->data.list.head = NULL;
        args_list_node->data.list.tail = NULL;
        args_list_node->data.list.count = 0;
        ast_node_free(args_list_node);
        ast_node_free(func_name_node);

        ast_stack_push(data, func_call_node);
        break;
    }

    case AST_ACTION_CREATE_IDENTIFIER:
    {
        ast_node_t * own_placeholder = ast_stack_pop(data);

        if (own_placeholder == NULL || !is_placeholder_node(own_placeholder))
        {
            ast_node_free(own_placeholder);
            set_error(data, pt_node, "Internal error: bad placeholder for CREATE_IDENTIFIER");
            return;
        }

        ast_node_free(own_placeholder);

        ast_node_t * ident_node = ast_node_alloc(data, AST_NODE_TYPE_IDENTIFIER);
        if (ident_node == NULL)
        {
            return;
        }
        ident_node->data.identifier.name = strndup(pt_node->content, pt_node->len);
        ast_stack_push(data, ident_node);
        break;
    }

    case AST_ACTION_NONE:
    case AST_ACTION_CREATE_NUMBER_FROM_CONTENT:
    case AST_ACTION_CREATE_OPERATOR_FROM_CHAR:
        break;
    }
}
