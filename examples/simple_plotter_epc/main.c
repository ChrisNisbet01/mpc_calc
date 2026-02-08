#include "font_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for sprintf
#include <math.h>   // Required for fabsf
#include <stdarg.h> // Required for va_list in potential error messages.

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#include "easy_mpc/easy_mpc.h"
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

typedef struct make_functions_cb_ctx
{
    epc_parser_t * p_unary_functions;
    epc_parser_t * p_binary_functions;
} make_functions_cb_ctx;

// Function prototypes
static void recalculate_graph(void);
static Vector2 WorldToScreen(Vector2 worldPoint, Rectangle graphRect);
static Vector2 ScreenToWorld(Vector2 screenPoint, Rectangle graphRect);


static bool
make_functions_cb(function_t const * const func, void * const ctx)
{
    make_functions_cb_ctx * cb_ctx = ctx;

    if (func->num_args == 1)
    {
        if (cb_ctx->p_unary_functions == NULL)
        {
            cb_ctx->p_unary_functions = p_string(func->name);
        }
        else
        {
            cb_ctx->p_unary_functions =
                p_or(2, cb_ctx->p_unary_functions, p_string(func->name));
        }
    }
    else if (func->num_args == 2)
    {
        if (cb_ctx->p_binary_functions == NULL)
        {
            cb_ctx->p_binary_functions = p_string(func->name);
        }
        else
        {
            cb_ctx->p_binary_functions =
                p_or(2, cb_ctx->p_binary_functions, p_string(func->name));
        }
    }

    return false;
}

static epc_parser_t *
make_functions_parser(void)
{
    epc_parser_t * p_functions = NULL;
    make_functions_cb_ctx cb_ctx = {0};
    functions_foreach(make_functions_cb, &cb_ctx);

    if (cb_ctx.p_binary_functions != NULL)
    {
        p_functions = cb_ctx.p_binary_functions;
    }
    if (cb_ctx.p_unary_functions != NULL)
    {
        if (p_functions == NULL)
        {
            p_functions = cb_ctx.p_unary_functions;
        }
        else
        {
            p_functions = p_or(2, p_functions, cb_ctx.p_unary_functions);
        }
    }

    if (p_functions != NULL)
    {
        epc_parser_set_name(p_functions, "functions");
    }

    return p_functions;
}

static epc_parser_t *
create_formula_grammar(void)
{
    epc_parser_t * p_double_parser = p_double();
    epc_parser_set_name(p_double_parser, "number");
    epc_parser_set_ast_action(p_double_parser, AST_ACTION_CREATE_NUMBER_FROM_CONTENT);

    epc_parser_t * p_const_pi = p_string("pi");
    epc_parser_set_name(p_const_pi, "pi");
    epc_parser_t * p_const_e = p_string("e");
    epc_parser_set_name(p_const_e, "e");
    epc_parser_t * p_constants = p_or(2, p_const_pi, p_const_e);
    epc_parser_set_name(p_constants, "constants");
    epc_parser_set_ast_action(p_constants, AST_ACTION_CREATE_IDENTIFIER);
    epc_parser_t * oparen = p_char('(');
    epc_parser_t * cparen = p_char(')');

    epc_parser_t * p_var_x = p_string("x");
    epc_parser_set_name(p_var_x, "var_x");

    epc_parser_t * p_variables = p_or(1, p_var_x);
    epc_parser_set_name(p_variables, "variable");
    epc_parser_set_ast_action(p_variables, AST_ACTION_CREATE_IDENTIFIER);

    // Additive operators
    epc_parser_t * p_plus_char = p_char('+');
    epc_parser_t * p_minus_char = p_char('-');
    epc_parser_t * p_add_sub_op = p_or(2, p_plus_char, p_minus_char);
    epc_parser_set_name(p_add_sub_op, "add_sub_op");
    epc_parser_set_ast_action(p_add_sub_op, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);

    // Multiplicative operators
    epc_parser_t * p_multiply_char = p_char('*');
    epc_parser_t * p_divide_char = p_char('/');
    epc_parser_t * p_mul_div_op = p_or(2, p_multiply_char, p_divide_char);
    epc_parser_set_name(p_mul_div_op, "mul_div_op");
    epc_parser_set_ast_action(p_mul_div_op, AST_ACTION_CREATE_OPERATOR_FROM_CHAR);

    epc_parser_t * spaces = p_many(p_space());

    epc_parser_t * p_functions = make_functions_parser();
    epc_parser_set_ast_action(p_functions, AST_ACTION_CREATE_IDENTIFIER);

    // Forward declaration for recursive grammar
    epc_parser_t * p_expression_fwd = epc_parser_allocate("expression_fwd");

    // Argument list parser using new combinators
    // Parser for a single expression argument (potentially surrounded by spaces)
    epc_parser_t * p_single_expression_arg = p_and(3, spaces, p_expression_fwd, spaces);
    epc_parser_set_name(p_single_expression_arg, "single_expression_arg");
    epc_parser_set_ast_action(p_single_expression_arg, AST_ACTION_PROMOTE_LAST_CHILD_AST);

    // A parser for one or more arguments separated by commas, allowing spaces around them.
    // E.g., "expr", "expr, expr"
    epc_parser_t * p_one_or_more_args = p_delimited(p_single_expression_arg, p_char(','));
    epc_parser_set_name(p_one_or_more_args, "one_or_more_args");
    epc_parser_set_ast_action(p_one_or_more_args, AST_ACTION_COLLECT_CHILD_RESULTS);

    // The actual argument list for a function call can be empty (e.g., func())
    epc_parser_t * p_args_list_optional = p_optional(p_one_or_more_args);
    epc_parser_set_name(p_args_list_optional, "args_list");
    epc_parser_set_ast_action(p_args_list_optional, AST_ACTION_PROMOTE_LAST_CHILD_AST);

    // Arguments enclosed in parentheses, using p_between for conciseness
    epc_parser_t * p_args_in_parens = p_between(oparen, p_args_list_optional, cparen);
    epc_parser_set_name(p_args_in_parens, "args_in_parens");
    epc_parser_set_ast_action(p_args_in_parens, AST_ACTION_PROMOTE_LAST_CHILD_AST);

    epc_parser_t * p_function_call = p_and(
        2,
        p_functions,
        p_args_in_parens
    );
    epc_parser_set_name(p_function_call, "function_call");
    epc_parser_set_ast_action(p_function_call, AST_ACTION_CREATE_FUNCTION_CALL);
    epc_parser_t * p_expression_in_parens = p_between(
        p_and(2, oparen, spaces),
        p_expression_fwd,
        p_and(2, spaces, cparen)
    );
    epc_parser_set_name(p_expression_in_parens, "expression_in_parens");

    epc_parser_t * p_factor = p_or(
        5,
        p_double_parser,
        p_constants,
        p_variables,
        p_function_call,
        p_expression_in_parens
    );
    epc_parser_set_ast_action(p_factor, AST_ACTION_PROMOTE_LAST_CHILD_AST);

    epc_parser_t * term_rest_unit = p_and(
        4,
        spaces,
        p_mul_div_op,
        spaces,
        p_factor
    );
    epc_parser_set_name(term_rest_unit, "term_rest_unit");
    epc_parser_set_ast_action(term_rest_unit, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * p_term_suffix = p_many(term_rest_unit);
    epc_parser_set_name(p_term_suffix, "term_suffix");
    epc_parser_set_ast_action(p_term_suffix, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * p_term = p_and(2, p_factor, p_term_suffix);
    epc_parser_set_name(p_term, "term");
    epc_parser_set_ast_action(p_term, AST_ACTION_BUILD_BINARY_EXPRESSION);

    epc_parser_t * expression_rest_unit = p_and(
        4,
        spaces,
        p_add_sub_op,
        spaces,
        p_term
    );
    epc_parser_set_name(expression_rest_unit, "expression_rest_unit");
    epc_parser_set_ast_action(expression_rest_unit, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * p_expression_suffix = p_many(expression_rest_unit);
    epc_parser_set_name(p_expression_suffix, "expression_suffix");
    epc_parser_set_ast_action(p_expression_suffix, AST_ACTION_COLLECT_CHILD_RESULTS);

    epc_parser_t * p_expression = p_and(2, p_term, p_expression_suffix);
    epc_parser_set_name(p_expression, "expression");
    epc_parser_set_ast_action(p_expression, AST_ACTION_BUILD_BINARY_EXPRESSION);

    epc_parser_duplicate(p_expression_fwd, p_expression);

    epc_parser_t * complete_expression = p_and(2, p_expression, p_eoi());
    epc_parser_set_name(complete_expression, "complete_expression");
    epc_parser_set_ast_action(complete_expression, AST_ACTION_ASSIGN_ROOT);

    return complete_expression;
}

typedef struct parse_and_evaluate_result_st
{
    bool success;
    union {
        double value;
        char * message;
    };
} parse_and_evaulate_result_st;

static void
parse_and_evaluate_result_cleanup(parse_and_evaulate_result_st * result)
{
    if (!result->success)
    {
        free(result->message);
    }
    memset(result, 0, sizeof(*result));
};

static parse_and_evaulate_result_st
parse_and_evaluate_formula(
    char const * const input_expr,
    double x_value)
{
    parse_and_evaulate_result_st result = {0};
    epc_parser_t * formula_grammar = create_formula_grammar();

    epc_parse_session_t parse_session = epc_parse_input(formula_grammar, input_expr);

    if (!parse_session.result.is_error)
    {
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
            result.message = strdup(ast_builder_data.error_message);
        }
        else if (ast_builder_data.ast_root == NULL)
        {
            result.message = strdup("Error: No root AST assigned.");
        }
        else
        {
            variable_t variables[1] = {
                {
                    .name = "x",
                    .value = x_value,
                }
            };
            double calculated_result = evaluate_ast(ast_builder_data.ast_root, variables, 1);

            result.success = true;
            result.value = calculated_result;
        }
        ast_builder_cleanup(&ast_builder_data);
    }
    else
    {
        char *msg = NULL;
        size_t input_len = strlen(input_expr);
        const char *error_pos_str = parse_session.result.data.error->input_position;
        size_t error_pos_len = (error_pos_str && error_pos_str >= input_expr && error_pos_str <= input_expr + input_len) ?
                               (input_expr + input_len - error_pos_str) : 0;


        int len = asprintf(&msg, "Error: %s at '%.*s' (expected '%s', found '%.*s')",
            parse_session.result.data.error->message,
            (int)error_pos_len,
            error_pos_str ? error_pos_str : "",
            parse_session.result.data.error->expected ? parse_session.result.data.error->expected : "N/A",
            (int)strlen(parse_session.result.data.error->found),
            parse_session.result.data.error->found ? parse_session.result.data.error->found : "N/A"
        );
        if (len < 0)
        {
            msg = strdup("memory allocation error in error message");
        }
        result.message = msg;
    }

    epc_parse_session_destroy(&parse_session);

    return result;
}




static void
recalculate_graph(void)
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
    parse_and_evaulate_result_st initial_parse_result = parse_and_evaluate_formula(formulaInputText, 0.0f); // x doesn't matter for initial parse check

    if (!initial_parse_result.success)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error compiling formula '%s': %s", formulaInputText, initial_parse_result.message);
        parse_and_evaluate_result_cleanup(&initial_parse_result);
        return;
    }
    snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled.", formulaInputText);

    // Generate graph points
    int estimatedPoints = (int)(effectiveXRange / stepVal) + 1;

    graphPointsCount = 0;
    graphPoints = MemAlloc(estimatedPoints * sizeof(*graphPoints));
    if (NULL == graphPoints)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Not enough memory for graph points.");
        parse_and_evaluate_result_cleanup(&initial_parse_result); // Cleanup for the initial parse
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

        parse_and_evaulate_result_st eval_result = parse_and_evaluate_formula(formulaInputText, current_x);

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
                parse_and_evaluate_result_cleanup(&initial_parse_result); // Cleanup for the initial parse
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

    parse_and_evaluate_result_cleanup(&initial_parse_result); // Final cleanup for the initial parse
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
    recalculate_graph(); // Use the new function

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
            recalculate_graph();
            drawGraphPressed = false; // Reset button state
            prevLogXScale = logXScale; // Update previous state
        }

        EndDrawing();
    }

    // Clean up
    if (NULL != graphPoints)
    {
        MemFree(graphPoints);
        graphPoints = NULL;
    }
    UnloadFont(font);
    CloseWindow();

    return 0;
}