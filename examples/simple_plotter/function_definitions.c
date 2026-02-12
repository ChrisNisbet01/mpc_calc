#include "function_definitions.h"
#include <string.h> // For strcmp
#include <math.h>   // For sin, cos, tan, etc.
#include <stdlib.h> // For fabs

// Helper macro for array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static function_t functions[] =
{
    {
        .name = "cos",
        .num_args = 1,
        .func_ptr.unary = cos,
    },
    {
        .name = "sin",
        .num_args = 1,
        .func_ptr.unary = sin,
    },
    {
        .name = "tan",
        .num_args = 1,
        .func_ptr.unary = tan,
    },
    {
        .name = "acos",
        .num_args = 1,
        .func_ptr.unary = acos,
    },
    {
        .name = "asin",
        .num_args = 1,
        .func_ptr.unary = asin,
    },
    {
        .name = "atan",
        .num_args = 1,
        .func_ptr.unary = atan,
    },
    {
        .name = "log10",
        .num_args = 1,
        .func_ptr.unary = log10,
    },
    {
        .name = "log",
        .num_args = 1,
        .func_ptr.unary = log,
    },
    {
        .name = "sqrt",
        .num_args = 1,
        .func_ptr.unary = sqrt,
    },
    {
        .name = "pow",
        .num_args = 2,
        .func_ptr.binary = pow,
    },
    {
        .name = "abs",
        .num_args = 1,
        .func_ptr.unary = fabs,
    },
    {
        .name = "round",
        .num_args = 1,
        .func_ptr.unary = round,
    },
    {
        .name = "ceil",
        .num_args = 1,
        .func_ptr.unary = ceil,
    },
    {
        .name = "floor",
        .num_args = 1,
        .func_ptr.unary = floor,
    },
    {
        .name = "exp",
        .num_args = 1,
        .func_ptr.unary = exp,
    },
    // The example code used 'fmin' and 'fmax' but those are from c99.
    // For broader compatibility, let's stick to standard C functions where possible,
    // or provide simple custom implementations if absolutely necessary.
    // For now, omitting min/max as they are not standard C functions in the same way.
    // {
    //     .name = "min",
    //     .num_args = 2,
    //     .func_ptr.binary = fmin,
    // },
    // {
    //     .name = "max",
    //     .num_args = 2,
    //     .func_ptr.binary = fmax,
    // },
};

static bool
function_name_matches(function_t const * const func, void * const user_ctx)
{
    char const * const name = (char const *)user_ctx;
    bool const name_matches = strcmp(func->name, name) == 0;

    return name_matches;
}

// Internal helper for iterating through functions (can be expanded if needed)
function_t const *
functions_foreach(bool (*cb)(function_t const * func, void * ctx), void * const user_ctx)
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

function_t const *
function_lookup_by_name(char const * const name)
{
    function_t const * const func =
        functions_foreach(function_name_matches, (void *)name);

    return func;
}
