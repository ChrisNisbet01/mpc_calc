#ifndef FUNCTION_DEFINITIONS_H
#define FUNCTION_DEFINITIONS_H

#include <stddef.h> // For size_t
#include <stdbool.h> // For bool

// Function pointer types for unary and binary operations
typedef double (*unary_func_t)(double v1);
typedef double (*binary_func_t)(double v1, double v2);

// Structure to hold function metadata and pointers
typedef struct function_t
{
    char const * name;
    size_t num_args;
    union {
        unary_func_t unary;
        binary_func_t binary;
    } func_ptr; // Renamed to avoid conflict with 'union' keyword if struct member name is 'union'
} function_t;

function_t const *
functions_foreach(bool (*cb)(function_t const * func, void * ctx), void * user_ctx);

// Lookup function prototype
function_t const * function_lookup_by_name(char const * const name);

#endif // FUNCTION_DEFINITIONS_H
