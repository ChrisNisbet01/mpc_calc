#include "font_data.h"
#include "grammar.h"

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

static void
recalculate_graph(ast_node_t * ast, size_t num_constants, variable_t * constants)
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

        // XXX - Assuming that the variables list here is the same as the one
        // used when constructing the grammar.
        variable_t variables[1] = {
            {
                .name = "x",
                .value = current_x,
            }
        };

        double y =  evaluate_ast(ast, variables, 1, constants, num_constants);

        if (isinf(y))
        {
            y = ymaxVal * 100.0;
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

    variable_t constants[2] = {
        {
            .name = "pi",
            .value = M_PI,
        },
        {
            .name = "e",
            .value = M_E,
        },
    };

    // Initial parsing of default formula
    variable_t variables[1] = {
        {
            .name = "x",
            .value = 0,
        }
    };

    epc_parser_list * list = epc_parser_list_create();
    epc_parser_t * formula_grammar = create_formula_grammar(list, 1, variables, 2, constants);
    epc_compile_context_st compile_context;
    compile_context = compile_expression(formula_grammar, formulaInputText);

    if (compile_context.success && compile_context.ast != NULL)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled.", formulaInputText);
        recalculate_graph(compile_context.ast, 2, constants);
    }
    else
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer),
                 "Error compiling formula '%s': %s", formulaInputText, compile_context.message);
    }

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
            compile_context_cleanup(&compile_context);
            compile_context = compile_expression(formula_grammar, formulaInputText);

            if (compile_context.success && compile_context.ast != NULL)
            {
                snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled.", formulaInputText);
                recalculate_graph(compile_context.ast, 2, constants);
            }
            else
            {
                snprintf(compileStatusBuffer, sizeof(compileStatusBuffer),
                         "Error compiling formula '%s': %s", formulaInputText, compile_context.message);
            }
            drawGraphPressed = false; // Reset button state
            prevLogXScale = logXScale; // Update previous state
        }

        EndDrawing();
    }

    // Clean up
    compile_context_cleanup(&compile_context);
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