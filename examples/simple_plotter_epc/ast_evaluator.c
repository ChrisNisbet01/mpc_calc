#include "ast_evaluator.h"
#include "ast.h"
#include "function_definitions.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double
evaluate_ast_recursive(
    ast_node_t* node,
    const variable_t* variables, size_t var_count,
    const variable_t* constants, size_t const_count
)
{
    if (!node) {
        fprintf(stderr, "Error: NULL AST node during evaluation.\n");
        return NAN;
    }

    switch (node->type) {
        case AST_NODE_TYPE_NUMBER:
            return node->data.number.value;

        case AST_NODE_TYPE_IDENTIFIER: {
            const char* name = node->data.identifier.name;
            // Check for built-in constants first
            for (size_t i = 0; i < const_count; ++i) {
                if (strcmp(name, constants[i].name) == 0) {
                    return constants[i].value;
                }
            }
            // Check for user-defined variables
            for (size_t i = 0; i < var_count; ++i) {
                if (strcmp(name, variables[i].name) == 0) {
                    return variables[i].value;
                }
            }
            fprintf(stderr, "Error: Unknown identifier '%s'.\n", name);
            return NAN;
        }

        case AST_NODE_TYPE_EXPRESSION: {
            double left_val = evaluate_ast_recursive(
                node->data.expression.left, variables, var_count, constants, const_count);
            if (isnan(left_val))
            {
                return NAN;
            }

            char op_char = node->data.expression.operator_node->data.op.operator_char;

            double right_val = evaluate_ast_recursive(
                node->data.expression.right, variables, var_count, constants, const_count);
            if (isnan(right_val))
            {
                return NAN;
            }

            switch (op_char) {
                case '+': return left_val + right_val;
                case '-': return left_val - right_val;
                case '*': return left_val * right_val;
                case '/':
                    if (right_val == 0.0)
                    {
                        fprintf(stderr, "Error: Division by zero.\n");
                        return NAN;
                    }
                    return left_val / right_val;
                default:
                    fprintf(stderr, "Error: Unknown operator '%c'.\n", op_char);
                    return NAN;
            }
        }

        case AST_NODE_TYPE_FUNCTION_CALL: {
            const function_t* func_def = node->data.function_call.func_def;
            ast_list_t* args_list = &node->data.function_call.arguments;

            double evaluated_args[func_def->num_args];
            ast_list_node_t* current_arg_node = args_list->head;
            for (size_t i = 0; i < func_def->num_args; ++i) {
                if (!current_arg_node) {
                     fprintf(stderr, "Error: Mismatch between expected and actual number of arguments for '%s'.\n", func_def->name);
                     return NAN;
                }
                double arg_val = evaluate_ast_recursive(
                    current_arg_node->item, variables, var_count, constants, const_count);
                if (isnan(arg_val))
                {
                    return NAN;
                }
                evaluated_args[i] = arg_val;
                current_arg_node = current_arg_node->next;
            }

            if (func_def->num_args == 1) {
                return func_def->func_ptr.unary(evaluated_args[0]);
            } else if (func_def->num_args == 2) {
                return func_def->func_ptr.binary(evaluated_args[0], evaluated_args[1]);
            } else {
                fprintf(stderr, "Error: Function '%s' has unsupported number of arguments (%zu).\n",
                        func_def->name, func_def->num_args);
                return NAN;
            }
        }

        case AST_NODE_TYPE_OPERATOR:
            fprintf(stderr, "Error: Attempted to evaluate an operator node directly.\n");
            return NAN;

        case AST_NODE_TYPE_PLACEHOLDER:
            fprintf(stderr, "Error: Evaluated a PLACEHOLDER-type node.\n");
            return NAN;

        default:
            fprintf(stderr, "Error: Unknown AST node type: %d.\n", node->type);
            return NAN;
    }
}

double
evaluate_ast(
    ast_node_t* node,
    const variable_t* variables, size_t var_count,
    const variable_t* constants, size_t const_count
)
{
    return evaluate_ast_recursive(node, variables, var_count, constants, const_count);
}
