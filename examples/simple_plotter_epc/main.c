#include "font_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for sprintf
#include <math.h>   // Required for fabsf
#include <stdarg.h> // Required for va_list in potential error messages.

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "easy_pc/easy_pc.h"
#include "ast.h"
#include "ast_builder.h"
#include "ast_evaluator.h"
#include "function_definitions.h"

#define INITIAL_SCREEN_WIDTH 1024
#define INITIAL_SCREEN_HEIGHT 768
#define GUI_AREA_HEIGHT 200 // Height reserved for GUI input fields at the top
#define GRAPH_PADDING 20    // Padding from window edges for the graph
#define MAX_INPUT_CHARS 32
#define MAX_FORMULA_CHARS 256
#define INPUT_FIELD_HEIGHT 30
#define INPUT_FIELD_WIDTH 100
#define LABEL_WIDTH 60
#define SPACING 10
#define BUTTON_WIDTH 150
#define BUTTON_HEIGHT 50
#define FONT_SIZE 24
#define AXIS_LABEL_PADDING 5

// --- GUI State Variables ---
static char xminText[MAX_INPUT_CHARS] = "-10.0";
static float xminVal = -10.0f;
static bool xminEditMode = false;

static char xmaxText[MAX_INPUT_CHARS] = "10.0";
static float xmaxVal = 10.0f;
static bool xmaxEditMode = false;

static char yminText[MAX_INPUT_CHARS] = "-10.0";
static float yminVal = -10.0f;
static bool yminEditMode = false;

static char ymaxText[MAX_INPUT_CHARS] = "10.0";
static float ymaxVal = 10.0f;
static bool ymaxEditMode = false;

static char stepText[MAX_INPUT_CHARS] = "0.1";
static float stepVal = 0.1f;
static bool stepEditMode = false;

static char formulaInputText[MAX_FORMULA_CHARS] = "x*x";
static bool formulaEditMode = false;

static bool logXScale = false; // New: Logarithmic X-axis scale state
static bool prevLogXScale = false; // New: Previous state of logXScale for change detection

static bool drawGraphPressed = false;

#define MAX_GRAPH_POINTS 10000 // Maximum number of points to allocate initially

// --- Graphing Data and Status ---
static char compileStatusBuffer[MAX_FORMULA_CHARS + 64] = "Enter formula and parameters";
static Vector2 * graphPoints = NULL;    // Dynamic array of points to draw
static int graphPointsCount = 0;
static float effectiveXRange = 0.0f; // Calculated X-axis range for display
static float effectiveYRange = 0.0f; // Calculated Y-axis range for display

// Function prototypes
static Vector2 WorldToScreen(Vector2 worldPoint, Rectangle graphRect);
static Vector2 ScreenToWorld(Vector2 screenPoint, Rectangle graphRect);

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
            cb_ctx->p_unary_functions = epc_parser_list_add(list, epc_string(func->name, func->name));
        }
        else
        {
            epc_parser_t * fn = epc_parser_list_add(list, epc_string(func->name, func->name));
            cb_ctx->p_unary_functions = epc_parser_list_add(list, epc_or("or_func", 2, cb_ctx->p_unary_functions, fn));
        }
    }
    else if (func->num_args == 2)
    {
        if (cb_ctx->p_binary_functions == NULL)
        {
            cb_ctx->p_binary_functions = epc_parser_list_add(list, epc_string(func->name, func->name));
        }
        else
        {
            epc_parser_t * fn = epc_parser_list_add(list, epc_string(func->name, func->name));
            cb_ctx->p_binary_functions = epc_parser_list_add(list, epc_or("or_func", 2, cb_ctx->p_binary_functions, fn));
        }
    }

    return false;
}

static epc_parser_t *
make_functions_parser(epc_parser_list * list)
{
    epc_parser_t * p_functions = NULL;
    make_functions_cb_ctx cb_ctx = {.list = list};
    functions_foreach(make_functions_cb, &cb_ctx);

    if (cb_ctx.p_binary_functions != NULL)
    {
        p_functions = epc_parser_list_add(list, epc_passthru("binary_functions", cb_ctx.p_binary_functions));
    }
    if (cb_ctx.p_unary_functions != NULL)
    {
        if (p_functions == NULL)
        {
            p_functions = epc_parser_list_add(list, epc_passthru("unary_functions", cb_ctx.p_unary_functions));
        }
        else
        {
            p_functions = epc_parser_list_add(list, epc_or("functions", 2, p_functions, cb_ctx.p_unary_functions));
        }
    }

    return p_functions;
}

static epc_parser_t *create_formula_grammar(epc_parser_list * list)
{
    epc_parser_t * ws             = epc_parser_list_add(list, epc_space("ws"));
    epc_parser_t *skip_ws         = epc_parser_list_add(list, epc_skip("skip_ws", ws));

    // Forward declarations for recursion
    epc_parser_t * expr_fwd       = epc_parser_list_add(list, epc_parser_allocate("expr"));
    epc_parser_t * term_fwd       = epc_parser_list_add(list, epc_parser_allocate("term"));
    epc_parser_t * factor_fwd     = epc_parser_list_add(list, epc_parser_allocate("factor"));

    // Literals
    epc_parser_t * number_        = epc_parser_list_add(list, epc_double("number"));
    epc_parser_set_ast_action(number_, AST_ACTION_CREATE_NUMBER_FROM_CONTENT);
    epc_parser_t * number         = epc_parser_list_add(list, epc_and("number", 2, skip_ws, number_));

    // Constants
    epc_parser_t * pi             = epc_parser_list_add(list, epc_string("pi", "pi"));
    epc_parser_t * e              = epc_parser_list_add(list, epc_string("e", "e"));

    epc_parser_t * constant_      = epc_parser_list_add(list, epc_or("constant", 2, pi, e));
    epc_parser_set_ast_action(constant_, AST_ACTION_CREATE_IDENTIFIER);
    epc_parser_t * constant       = epc_parser_list_add(list, epc_and("constant", 2, skip_ws, constant_));

    // Variables
    epc_parser_t * var_x          = epc_parser_list_add(list, epc_string("var_x", "x"));
    // TODO: Add variables here.
    epc_parser_t * variable_      = epc_parser_list_add(list, epc_or("variable", 1, var_x));
    epc_parser_set_ast_action(variable_, AST_ACTION_CREATE_IDENTIFIER);
    epc_parser_t * variable       = epc_parser_list_add(list, epc_and("variable", 2, skip_ws, variable_));

    // Operators
    epc_parser_t * add_op         = epc_parser_list_add(list, epc_char("add", '+'));
    epc_parser_t * sub_op         = epc_parser_list_add(list, epc_char("sub", '-'));
    epc_parser_t * add_sub_       = epc_parser_list_add(list, epc_or("add_sub", 2, add_op, sub_op));
    epc_parser_set_ast_action(add_sub_, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);
    epc_parser_t * add_sub        = epc_parser_list_add(list, epc_and("add_sub", 2, skip_ws, add_sub_));

    epc_parser_t * mul_op         = epc_parser_list_add(list, epc_char("mul", '*'));
    epc_parser_t * div_op         = epc_parser_list_add(list, epc_char("div", '/'));
    epc_parser_t * mul_div_       = epc_parser_list_add(list, epc_or("mul_div", 2, mul_op, div_op));
    epc_parser_set_ast_action(mul_div_, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);
    epc_parser_t * mul_div        = epc_parser_list_add(list, epc_and("mul_div", 2, skip_ws, mul_div_));

    // Parentheses
    epc_parser_t * lparen_        = epc_parser_list_add(list, epc_char("(", '('));
    epc_parser_t * lparen         = epc_parser_list_add(list, epc_and("(", 2, skip_ws, lparen_));
    epc_parser_t * rparen_        = epc_parser_list_add(list, epc_char(")", ')'));
    epc_parser_t * rparen         = epc_parser_list_add(list, epc_and(")", 2, skip_ws, rparen_));
    epc_parser_t * expr_in_parens = epc_parser_list_add(list, epc_between("parens", lparen, expr_fwd, rparen));

    // Function call
    epc_parser_t * function_      = make_functions_parser(list);
    epc_parser_set_ast_action(function_, AST_ACTION_CREATE_IDENTIFIER);
    epc_parser_t * function       = epc_parser_list_add(list, epc_and("function", 2, skip_ws, function_));
    epc_parser_t * arg_delim_     = epc_parser_list_add(list, epc_char(",", ','));
    epc_parser_t * arg_delim      = epc_parser_list_add(list, epc_and(",", 2, skip_ws, arg_delim_));
    epc_parser_t * fn_lparen      = epc_parser_list_add(list, epc_char("(", '('));
    epc_parser_t * fn_rparen_     = epc_parser_list_add(list, epc_char(")", ')'));
    epc_parser_t * fn_rparen      = epc_parser_list_add(list, epc_and(")", 2, skip_ws, fn_rparen_));

    epc_parser_t * single_arg     = epc_parser_list_add(list, epc_and("single_expression_arg", 2, skip_ws, expr_fwd));
    epc_parser_t * many_args      = epc_parser_list_add(list, epc_delimited("one_or_more_args", single_arg, arg_delim));
    epc_parser_set_ast_action(many_args, AST_ACTION_COLLECT_CHILD_RESULTS);
    epc_parser_t * zero_or_more_args = epc_parser_list_add(list, epc_optional("optional_args_list", many_args));
    epc_parser_set_ast_action(zero_or_more_args, AST_ACTION_PROMOTE_ARGS_LIST_AST_OR_EMPTY_LIST);
    epc_parser_t * args_in_parens = epc_parser_list_add(list, epc_between("args_in_parens", fn_lparen, zero_or_more_args, fn_rparen));
    epc_parser_t * function_call  = epc_parser_list_add(list, epc_and("function_call", 2, function, args_in_parens));
    epc_parser_set_ast_action(function_call, AST_ACTION_CREATE_FUNCTION_CALL);

    // Base of factor: number | constant | ( expr )
    epc_parser_t * factor         =
        epc_parser_list_add(list,
           epc_or(
               "primary",
                5,
                number,
                constant,
                variable,
                function_call,
                expr_in_parens
            )
        );

    // term = factor (mul_div factor)*
    epc_parser_t * muldiv_factor = epc_parser_list_add(list, epc_and("muldiv_factor", 2, mul_div, factor_fwd));
    epc_parser_set_ast_action(muldiv_factor, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * term_tail     = epc_parser_list_add(list, epc_many("term_tail", muldiv_factor));
    epc_parser_set_ast_action(term_tail, AST_ACTION_COLLECT_CHILD_RESULTS);
    epc_parser_t * term          = epc_parser_list_add(list, epc_and("term", 2, factor_fwd, term_tail));
    epc_parser_set_ast_action(term, AST_ACTION_BUILD_BINARY_EXPRESSION);

    // expr = term (add_sub term)*
    epc_parser_t * addsub_term   = epc_parser_list_add(list, epc_and("addsub_term", 2, add_sub, term_fwd));
    epc_parser_set_ast_action(addsub_term, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * expr_tail     = epc_parser_list_add(list, epc_many("expr_tail", addsub_term));
    epc_parser_set_ast_action(expr_tail, AST_ACTION_COLLECT_CHILD_RESULTS);
    epc_parser_t * expr          = epc_parser_list_add(list, epc_and("expr", 2, term_fwd, expr_tail));
    epc_parser_set_ast_action(expr, AST_ACTION_BUILD_BINARY_EXPRESSION);

    // Wire up recursion
    epc_parser_duplicate(term_fwd,   term);
    epc_parser_duplicate(factor_fwd, factor);
    epc_parser_duplicate(expr_fwd,   expr);

    // Complete parser
    epc_parser_t *eoi            = epc_parser_list_add(list, epc_eoi("eoi"));
    epc_parser_t *complete       = epc_parser_list_add(list, epc_and("formula", 2, expr, eoi));
    epc_parser_set_ast_action(complete, AST_ACTION_ASSIGN_ROOT);

    return complete;
}

typedef struct parse_and_evaluate_result_st
{
    bool success;
    union {
        double value;
        char * message;
    };
} parse_and_evaluate_result_st;

static void
parse_and_evaluate_result_cleanup(parse_and_evaluate_result_st * result)
{
    if (!result->success)
    {
        free(result->message);
    }
    memset(result, 0, sizeof(*result));
};

static parse_and_evaluate_result_st
parse_and_evaluate(
    epc_parser_t * formula_parser,
    char const * const input_expr,
    size_t num_variables,
    variable_t const * variables)
{
    parse_and_evaluate_result_st result = {0};

    //fprintf(stdout, "Parsing: \"%s\"\n", input_expr);

    epc_parse_session_t parse_session = epc_parse_input(formula_parser, input_expr);

    if (!parse_session.result.is_error)
    {
        char * cpt_str = epc_cpt_to_string(parse_session.internal_parse_ctx, parse_session.result.data.success, 0);

        if (cpt_str)
        {
            //fprintf(stdout, "Parse successful!\n");
            //fprintf(stdout, "--- CPT ---\n");
            //fprintf(stdout, "%s", cpt_str);
            //fprintf(stdout, "-----------\n");
            free(cpt_str);
        }

        ast_builder_data_t ast_builder_data;
        ast_builder_init(&ast_builder_data);

        epc_cpt_visitor_t ast_builder_visitor = {
            .enter_node = ast_builder_enter_node,
            .exit_node = ast_builder_exit_node,
            .user_data = &ast_builder_data
        };

        epc_cpt_visit_nodes(parse_session.result.data.success, &ast_builder_visitor);

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
            double calculated_result = evaluate_ast(ast_builder_data.ast_root, variables, num_variables);

            result.success = true;
            result.value = calculated_result;
        }
        ast_builder_cleanup(&ast_builder_data);
    }
    else
    {
        char *msg = NULL;
        int len = asprintf(&msg, "Error: %s at '%.*s' (expected '%s', found '%.*s')",
            parse_session.result.data.error->message,
            (int)(input_expr + strlen(input_expr) - parse_session.result.data.error->input_position),
            parse_session.result.data.error->input_position,
            parse_session.result.data.error->expected ? parse_session.result.data.error->expected : "N/A",
            (int)strlen(parse_session.result.data.error->found),
            parse_session.result.data.error->found ? parse_session.result.data.error->found : "N/A"
        );
        if (len < 0)
        {
            msg = strdup("memory allocation error");
        }
        result.message = msg;
    }

    // Destroy the parse session, which frees the internal parse context.
    epc_parse_session_destroy(&parse_session);

    return result;
}

static void
recalculate_graph(epc_parser_t * formula_grammar)
{
    // Cleanup previous formula and points
    if (NULL != graphPoints)
    {
        MemFree(graphPoints);
        graphPoints = NULL;
        graphPointsCount = 0;
    }

    effectiveXRange = xmaxVal - xminVal;
    effectiveYRange = ymaxVal - yminVal;

    if (effectiveXRange <= 0.0f || effectiveYRange <= 0.0f || stepVal <= 0.0f)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Invalid range or step value.");
        return;
    }

    // Parse formula once for structural correctness
    variable_t variables[1] = {
        {
            .name = "x",
            .value = 0,
        }
    };
    {
        // x doesn't matter for initial parse check
        parse_and_evaluate_result_st initial_parse_result =
            parse_and_evaluate(formula_grammar, formulaInputText, 1, variables);

        if (!initial_parse_result.success)
        {
            snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error compiling formula '%s': %s", formulaInputText, initial_parse_result.message);
            parse_and_evaluate_result_cleanup(&initial_parse_result);
            return;
        }
        parse_and_evaluate_result_cleanup(&initial_parse_result);
    }

    snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled.", formulaInputText);

    // Generate graph points
    int estimatedPoints = (int)(effectiveXRange / stepVal) + 1;

    graphPointsCount = 0;
    graphPoints = MemAlloc(estimatedPoints * sizeof(*graphPoints));
    if (NULL == graphPoints)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Not enough memory for graph points.");
        return;
    }

    for (int i = 0; i < estimatedPoints; ++i)
    {
        float current_x;

        current_x = xminVal + (float)i * stepVal;
        // Ensure x is positive for logarithmic calculations downstream
        if (logXScale && current_x <= 0.f)
        {
            continue;
        }
        if (current_x > xmaxVal)
        {
            break;
        }

        variable_t variables[1] = {
            {
                .name = "x",
                .value = current_x,
            }
        };

        parse_and_evaluate_result_st eval_result = parse_and_evaluate(formula_grammar, formulaInputText, 1, variables);

        if (!eval_result.success)
        {
            // If evaluation fails for a specific point, we just skip it
            // No need to update compileStatusBuffer here as it refers to compilation status
            parse_and_evaluate_result_cleanup(&eval_result);
            continue;
        }
        double y_double = eval_result.value;
        parse_and_evaluate_result_cleanup(&eval_result); // Cleanup after evaluation

        float y = y_double;

        if (isinf(y))
        {
            continue;
        }
        if (isnan(y))
        {
            continue;
        }
        if (y < yminVal)
        {
            continue;
        }
        if (y > ymaxVal)
        {
            continue;
        }

        // Dynamically reallocate if needed, in chunks
        if (graphPointsCount == estimatedPoints)
        {
            estimatedPoints *= 2; // Double the capacity
            Vector2 *newGraphPoints = MemRealloc(graphPoints, estimatedPoints * sizeof(*newGraphPoints));
            if (NULL == newGraphPoints)
            {
                snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Not enough memory during reallocation.");
                MemFree(graphPoints);
                graphPoints = NULL;
                graphPointsCount = 0;
                return;
            }
            graphPoints = newGraphPoints;
        }

        graphPoints[graphPointsCount].x = current_x;
        graphPoints[graphPointsCount].y = y;
        graphPointsCount++;
    }

    if (graphPointsCount == 0)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "No valid points to plot for '%s'", formulaInputText);
    }
}


// Function to convert world coordinates to screen coordinates
static Vector2
WorldToScreen(Vector2 worldPoint, Rectangle graphRect)
{
    Vector2 screenPoint = {0};
    // Calculate the scale factor for X and Y axes
    float scaleX, scaleY;

    if (logXScale)
    {
        // Ensure log inputs are positive
        float log_xmin = (xminVal > 0) ? log10f(xminVal) : log10f(0.0001f); // Clamp to a small positive value if xminVal is not positive
        float log_xmax = (xmaxVal > 0) ? log10f(xmaxVal) : log10f(1.0f); // Clamp to a small positive value if xmaxVal is not positive

        // Effective range in log space
        float log_effectiveXRange = log_xmax - log_xmin;
        scaleX = graphRect.width / log_effectiveXRange;

        // Transform worldPoint.x to log space
        float transformed_world_x = (worldPoint.x > 0) ? log10f(worldPoint.x) : log10f(0.0001f);

        screenPoint.x = graphRect.x + (transformed_world_x - log_xmin) * scaleX;
    }
    else
    {
        scaleX = graphRect.width / (xmaxVal - xminVal);
        screenPoint.x = graphRect.x + (worldPoint.x - xminVal) * scaleX;
    }

    scaleY = graphRect.height / (ymaxVal - yminVal);
    screenPoint.y = graphRect.y + graphRect.height - (worldPoint.y - yminVal) * scaleY; // Y-axis is inverted in screen coordinates

    return screenPoint;
}

// Function to convert screen coordinates to world coordinates (if needed)
static Vector2
ScreenToWorld(Vector2 screenPoint, Rectangle graphRect)
{
    Vector2 worldPoint = {0};
    float scaleX = graphRect.width / (xmaxVal - xminVal);
    float scaleY = graphRect.height / (ymaxVal - yminVal);

    worldPoint.x = xminVal + (screenPoint.x - graphRect.x) / scaleX;
    worldPoint.y = yminVal + (graphRect.y + graphRect.height - screenPoint.y) / scaleY;

    return worldPoint;
}


int
main(int argc, char ** argv)
{
    // Configure Window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT, "Function Plotter");
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, FONT_SIZE); // Use FONT_SIZE here

    Font font = LoadFontFromMemory(".ttf", iosevka_regular_ttf, iosevka_regular_ttf_len, FONT_SIZE, 0, 0);
    GuiSetFont(font);

    // Initial parsing of default formula
    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * formula_grammar = create_formula_grammar(list);

    recalculate_graph(formula_grammar);

    /* Main game loop */
    while (!WindowShouldClose())
    {
        // Update
        // Check for GUI input changes
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            xminEditMode = false;
            xmaxEditMode = false;
            yminEditMode = false;
            ymaxEditMode = false;
            stepEditMode = false;
            formulaEditMode = false;
        }

        // --- GUI Controls ---
        int currentScreenWidth = GetScreenWidth();
        int currentScreenHeight = GetScreenHeight();
        // Define common layout parameters
        float currentX = GRAPH_PADDING;
        float currentY = GRAPH_PADDING;

        // Draw
        BeginDrawing();

        ClearBackground(RAYWHITE);

        // --- Draw GUI Elements ---

        // XMIN input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "xmin:");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT}, xminText, MAX_INPUT_CHARS, xminEditMode))
        {
            xminEditMode = !xminEditMode;
            if (!xminEditMode) xminVal = (float)atof(xminText); // Parse value when editing ends
        }
        currentX += INPUT_FIELD_WIDTH + SPACING;

        // XMAX input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "xmax:");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT}, xmaxText, MAX_INPUT_CHARS, xmaxEditMode))
        {
            xmaxEditMode = !xmaxEditMode;
            if (!xmaxEditMode) xmaxVal = (float)atof(xmaxText);
        }
        currentX += INPUT_FIELD_WIDTH + SPACING;

        // YMIN input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "ymin:");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT}, yminText, MAX_INPUT_CHARS, yminEditMode))
        {
            yminEditMode = !yminEditMode;
            if (!yminEditMode) yminVal = (float)atof(yminText);
        }
        currentX += INPUT_FIELD_WIDTH + SPACING;

        // YMAX input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "ymax:");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT}, ymaxText, MAX_INPUT_CHARS, ymaxEditMode))
        {
            ymaxEditMode = !ymaxEditMode;
            if (!ymaxEditMode) ymaxVal = (float)atof(ymaxText);
        }
        currentX += INPUT_FIELD_WIDTH + SPACING;

        currentX = GRAPH_PADDING; // Reset X for next row
        currentY += INPUT_FIELD_HEIGHT + SPACING; // Move to next row

        // STEP input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "step:");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, INPUT_FIELD_WIDTH, INPUT_FIELD_HEIGHT}, stepText, MAX_INPUT_CHARS, stepEditMode))
        {
            stepEditMode = !stepEditMode;
            if (!stepEditMode)
            {
                stepVal = (float)atof(stepText);
                if (stepVal <= 0.0f) stepVal = 0.01f; // Ensure step is positive
                snprintf(stepText, MAX_INPUT_CHARS, "%.3f", stepVal); // Update text if changed
            }
        }
        currentX += INPUT_FIELD_WIDTH + SPACING;

        // Log X Scale checkbox
        if (GuiCheckBox((Rectangle){currentX, currentY, INPUT_FIELD_HEIGHT, INPUT_FIELD_HEIGHT}, "Log X Scale", &logXScale))
        {
            //logXScale = !logXScale;
            drawGraphPressed = true; // Trigger recalculation
        }
        currentX = GRAPH_PADDING; // Reset X for next row
        currentY += INPUT_FIELD_HEIGHT + SPACING; // Move to next row

        // Formula input
        GuiLabel((Rectangle){currentX, currentY, LABEL_WIDTH, INPUT_FIELD_HEIGHT}, "f(x):");
        currentX += LABEL_WIDTH;
        if (GuiTextBox((Rectangle){currentX, currentY, currentScreenWidth - currentX - GRAPH_PADDING - (BUTTON_WIDTH + SPACING), INPUT_FIELD_HEIGHT}, formulaInputText, MAX_FORMULA_CHARS, formulaEditMode))
        {
            formulaEditMode = !formulaEditMode;
        }

        // Draw Graph button
        currentX = currentScreenWidth - GRAPH_PADDING - BUTTON_WIDTH;
        if (GuiButton((Rectangle){currentX, currentY, BUTTON_WIDTH, BUTTON_HEIGHT}, "Draw Graph"))
        {
            drawGraphPressed = true;
        }

        // Compile status message
        DrawTextEx(font, compileStatusBuffer, (Vector2){GRAPH_PADDING, currentY + INPUT_FIELD_HEIGHT + SPACING}, FONT_SIZE, 1, compileStatusBuffer[0] == 'E' ? RED : DARKGRAY);


        // --- Draw Graph Area ---
        Rectangle graphRect = {GRAPH_PADDING, GUI_AREA_HEIGHT,
                               (float)currentScreenWidth - 2 * GRAPH_PADDING,
                               (float)currentScreenHeight - GUI_AREA_HEIGHT - GRAPH_PADDING};

        // Draw X and Y axes
        Vector2 originWorld = {0.0f, 0.0f};
        Vector2 originScreen = WorldToScreen(originWorld, graphRect);

        // Draw X-axis
        DrawLine(graphRect.x, originScreen.y, graphRect.x + graphRect.width, originScreen.y, BLACK);

        // Draw Y-axis
        DrawLine(originScreen.x, graphRect.y, originScreen.x, graphRect.y + graphRect.height, BLACK);

        // Draw X-axis ticks and labels
        if (logXScale)
        {
            float log_xmin = log10f(fmaxf(xminVal, 0.0001f)); // Ensure positive for log
            float log_xmax = log10f(fmaxf(xmaxVal, 0.0001f)); // Ensure positive for log

            for (float p = ceilf(log_xmin); p <= floorf(log_xmax); ++p)
            {
                float x_major = powf(10, p);

                if (x_major >= xminVal && x_major <= xmaxVal)
                {
                    Vector2 screenPos = WorldToScreen((Vector2){x_major, 0.0f}, graphRect);
                    DrawLine(screenPos.x,
                             graphRect.y,
                             screenPos.x,
                             graphRect.y + graphRect.height, LIGHTGRAY);
                    DrawTextEx(font, TextFormat("10^%.0f", p), (Vector2){screenPos.x - MeasureText(TextFormat("10^%.0f", p), FONT_SIZE/2) / 2, originScreen.y + 5 + AXIS_LABEL_PADDING}, FONT_SIZE/2, 1, DARKGRAY);
                }

                // Minor ticks within each decade
                if (p < floorf(log_xmax)) // Don't draw minor ticks beyond the last major
                {
                    for (int i = 2; i < 10; ++i)
                    {
                        float x_minor = i * powf(10, p);
                        if (x_minor >= xminVal && x_minor <= xmaxVal)
                        {
                            Vector2 screenPos = WorldToScreen((Vector2){x_minor, 0.0f}, graphRect);
                            DrawLine(screenPos.x, originScreen.y - 5, screenPos.x, originScreen.y + 5, LIGHTGRAY);
                        }
                    }
                }
            }
        }
        else // Linear X-axis
        {
            float xStep = effectiveXRange / 10.0f; // 10 major ticks
            for (float x = xminVal; x <= xmaxVal; x += xStep)
            {
                if (fabsf(x) < 1e-6) continue; // Skip label for 0.0 to avoid overlap

                Vector2 screenPos = WorldToScreen((Vector2){x, 0.0f}, graphRect);
                DrawLine(screenPos.x,
                         graphRect.y,
                         screenPos.x,
                         graphRect.y + graphRect.height, LIGHTGRAY);
                DrawTextEx(font, TextFormat("%.1f", x), (Vector2){screenPos.x - MeasureText(TextFormat("%.1f", x), FONT_SIZE/2) / 2, originScreen.y + 5 + AXIS_LABEL_PADDING}, FONT_SIZE/2, 1, DARKGRAY);
            }
        }

        // Draw Y-axis ticks and labels
        float yStep = effectiveYRange / 10.0f; // 10 major ticks
        for (float y = yminVal; y <= ymaxVal; y += yStep)
        {
            if (fabsf(y) < 1e-6) continue; // Skip label for 0.0 to avoid overlap

            Vector2 screenPos = WorldToScreen((Vector2){0.0f, y}, graphRect);
            DrawLine(graphRect.x, screenPos.y, graphRect.x + graphRect.width, screenPos.y, LIGHTGRAY);
            DrawTextEx(font, TextFormat("%.1f", y), (Vector2){originScreen.x + 5 + AXIS_LABEL_PADDING, screenPos.y - FONT_SIZE/4}, FONT_SIZE/2, 1, DARKGRAY);
        }

        // Draw the function graph
        if (graphPointsCount > 1)
        {
            for (int i = 0; i < graphPointsCount - 1; i++)
            {
                Vector2 screenPoint1 = WorldToScreen(graphPoints[i], graphRect);
                Vector2 screenPoint2 = WorldToScreen(graphPoints[i+1], graphRect);
                DrawLineV(screenPoint1, screenPoint2, BLUE);
            }
        }

        // Recalculate graph if button pressed or editing mode changed (for value parsing)
        bool const should_recalculate_graph = drawGraphPressed
            || !xminEditMode
            || !xmaxEditMode
            || !yminEditMode
            || !ymaxEditMode
            || !stepEditMode
            || !formulaEditMode
            || (logXScale != prevLogXScale);

        if (should_recalculate_graph)
        {
            recalculate_graph(formula_grammar);
            drawGraphPressed = false; // Reset button state
            prevLogXScale = logXScale; // Update previous state
        }

        EndDrawing();
    }

    // Clean up
    epc_parser_list_free(list);
    if (NULL != graphPoints)
    {
        MemFree(graphPoints);
        graphPoints = NULL;
    }
    UnloadFont(font);
    CloseWindow();

    return 0;
}