#include "formula_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for sprintf
#include <math.h>   // Required for fabsf

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

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

static bool drawGraphPressed = false;

#define MAX_GRAPH_POINTS 10000 // Maximum number of points to allocate initially

// --- Graphing Data and Status ---
static char compileStatusBuffer[MAX_FORMULA_CHARS + 64] = "Enter formula and parameters";
static Formula * compiledFormula = NULL; // Compiled formula context
static Vector2 * graphPoints = NULL;    // Dynamic array of points to draw
static int graphPointsCount = 0;
static float effectiveXRange = 0.0f; // Calculated X-axis range for display
static float effectiveYRange = 0.0f; // Calculated Y-axis range for display

// Function prototypes
static void recalculate_graph(void);
static Vector2 WorldToScreen(Vector2 worldPoint, Rectangle graphRect);
static Vector2 ScreenToWorld(Vector2 screenPoint, Rectangle graphRect);


// --- Helper Functions Implementations ---
static void
recalculate_graph(void)
{
    // Cleanup previous formula and points
    if (NULL != compiledFormula)
    {
        formula_cleanup(compiledFormula);
        compiledFormula = NULL;
    }
    if (NULL != graphPoints)
    {
        MemFree(graphPoints);
        graphPoints = NULL;
        graphPointsCount = 0;
    }

    // Parse and compile new formula
    compiledFormula = formula_compile(formulaInputText);
    if (NULL == compiledFormula)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error compiling formula '%s'", formulaInputText);
        return;
    }
    snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled. Plotting...", formulaInputText);

    // Calculate effective ranges for axes based on user input for centering at 0,0
    effectiveXRange = xmaxVal - xminVal;
    effectiveYRange = ymaxVal - yminVal;

    if (effectiveXRange <= 0.0f || effectiveYRange <= 0.0f || stepVal <= 0.0f)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Invalid range or step value.");
        formula_cleanup(compiledFormula);
        compiledFormula = NULL;
        return;
    }

    // Generate graph points
    int estimatedPoints = (int)(effectiveXRange / stepVal) + 1;
    if (estimatedPoints > 0)
    {
        graphPoints = (Vector2 *)MemAlloc(estimatedPoints * sizeof(Vector2));
        if (NULL == graphPoints)
        {
            snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Not enough memory for graph points.");
            formula_cleanup(compiledFormula);
            compiledFormula = NULL;
            return;
        }

        float x = xminVal;
        for (int i = 0; x <= xmaxVal; ++i)
        {
            double y_double;
            formula_evaluate(compiledFormula, x, &y_double);
            float y = (float)y_double;

            if (!isinf(y) && !isnan(y) && y >= yminVal && y <= ymaxVal)
            {
                graphPoints[graphPointsCount].x = x;
                graphPoints[graphPointsCount].y = y;
                graphPointsCount++;

                // Dynamically reallocate if needed, in chunks
                if (graphPointsCount == estimatedPoints)
                {
                    estimatedPoints *= 2; // Double the capacity
                    Vector2 *newGraphPoints = (Vector2 *)MemRealloc(graphPoints, estimatedPoints * sizeof(Vector2));
                    if (NULL == newGraphPoints)
                    {
                        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error: Not enough memory during reallocation.");
                        formula_cleanup(compiledFormula);
                        compiledFormula = NULL;
                        MemFree(graphPoints);
                        graphPoints = NULL;
                        graphPointsCount = 0;
                        return;
                    }
                    graphPoints = newGraphPoints;
                }
            }
            x += stepVal;
        }
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
    float scaleX = graphRect.width / (xmaxVal - xminVal);
    float scaleY = graphRect.height / (ymaxVal - yminVal);

    // Translate and scale the world point
    screenPoint.x = graphRect.x + (worldPoint.x - xminVal) * scaleX;
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

        // Recalculate graph if button pressed or editing mode changed (for value parsing)
        if (drawGraphPressed || !xminEditMode || !xmaxEditMode || !yminEditMode || !ymaxEditMode || !stepEditMode || !formulaEditMode)
        {
            recalculate_graph();
            drawGraphPressed = false; // Reset button state
        }


        // Draw
        BeginDrawing();

        ClearBackground(RAYWHITE);

        // --- Draw GUI Elements ---
        currentX = GRAPH_PADDING;
        currentY = GRAPH_PADDING;

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
        DrawText(compileStatusBuffer, GRAPH_PADDING, currentY + INPUT_FIELD_HEIGHT + SPACING, FONT_SIZE, compileStatusBuffer[0] == 'E' ? RED : DARKGRAY);


        // --- Draw Graph Area ---
        Rectangle graphRect = {GRAPH_PADDING, GUI_AREA_HEIGHT, 
                               (float)currentScreenWidth - 2 * GRAPH_PADDING, 
                               (float)currentScreenHeight - GUI_AREA_HEIGHT - GRAPH_PADDING};

        DrawRectangleLinesEx(graphRect, 1, BLACK); // Bounding box for the graph area

        // Draw X and Y axes
        Vector2 originWorld = {0.0f, 0.0f};
        Vector2 originScreen = WorldToScreen(originWorld, graphRect);

        DrawLine(graphRect.x, originScreen.y, graphRect.x + graphRect.width, originScreen.y, BLACK); // X-axis
        DrawLine(originScreen.x, graphRect.y, originScreen.x, graphRect.y + graphRect.height, BLACK); // Y-axis

        // Draw X-axis ticks and labels
        float xStep = effectiveXRange / 10.0f; // 10 major ticks
        for (float x = xminVal; x <= xmaxVal; x += xStep)
        {
            if (fabsf(x) < 1e-6) continue; // Skip label for 0.0 to avoid overlap

            Vector2 screenPos = WorldToScreen((Vector2){x, 0.0f}, graphRect);
            DrawLine(screenPos.x, originScreen.y - 5, screenPos.x, originScreen.y + 5, BLACK);
            DrawText(TextFormat("%.1f", x), screenPos.x - MeasureText(TextFormat("%.1f", x), FONT_SIZE/2) / 2, originScreen.y + 5 + AXIS_LABEL_PADDING, FONT_SIZE/2, DARKGRAY);
        }

        // Draw Y-axis ticks and labels
        float yStep = effectiveYRange / 10.0f; // 10 major ticks
        for (float y = yminVal; y <= ymaxVal; y += yStep)
        {
            if (fabsf(y) < 1e-6) continue; // Skip label for 0.0 to avoid overlap

            Vector2 screenPos = WorldToScreen((Vector2){0.0f, y}, graphRect);
            DrawLine(originScreen.x - 5, screenPos.y, originScreen.x + 5, screenPos.y, BLACK);
            DrawText(TextFormat("%.1f", y), originScreen.x + 5 + AXIS_LABEL_PADDING, screenPos.y - FONT_SIZE/4, FONT_SIZE/2, DARKGRAY);
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

        EndDrawing();
    }

    // Clean up
    if (NULL != compiledFormula)
    {
        formula_cleanup(compiledFormula);
        compiledFormula = NULL;
    }
    if (NULL != graphPoints)
    {
        MemFree(graphPoints);
        graphPoints = NULL;
    }
    CloseWindow();

    return 0;
}