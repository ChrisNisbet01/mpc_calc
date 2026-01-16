#include "formula_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for sprintf

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

#define INITIAL_SCREEN_WIDTH 1024
#define INITIAL_SCREEN_HEIGHT 768
#define GUI_AREA_HEIGHT 200 // Height reserved for GUI input fields at the top
#define GRAPH_PADDING 20    // Padding from window edges for the graph
#define MAX_INPUT_CHARS 32
#define MAX_FORMULA_CHARS 256
#define BUTTON_WIDTH 150
#define BUTTON_HEIGHT 50

// State variables for GUI input fields
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

// --- Calculation results (global to simplify drawing, will be updated based on input) ---
static char compileStatusBuffer[MAX_FORMULA_CHARS + 64] = "Enter formula and parameters";
static Formula * compiledFormula = NULL; // Compiled formula context
static Vector2 * graphPoints = NULL;    // Dynamic array of points to draw
static int graphPointsCount = 0;
static float effectiveXRange = 0.0f; // Calculated X-axis range for display
static float effectiveYRange = 0.0f; // Calculated Y-axis range for display

// --- Helper Functions ---
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

    // Compile new formula
    compiledFormula = formula_compile(formulaInputText);
    if (NULL == compiledFormula)
    {
        snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Error compiling formula '%s'", formulaInputText);
        return;
    }
    snprintf(compileStatusBuffer, sizeof(compileStatusBuffer), "Formula '%s' compiled. Plotting...", formulaInputText);

    // Calculate effective ranges for axes based on user input for centering at 0,0
    effectiveXRange = fmaxf(fabsf(xminVal), fabsf(xmaxVal));
    effectiveYRange = fmaxf(fabsf(yminVal), fabsf(ymaxVal));
    if (effectiveXRange == 0.0f) effectiveXRange = 1.0f; // Avoid division by zero
    if (effectiveYRange == 0.0f) effectiveYRange = 1.0f; // Avoid division by zero

    // Generate graph points
    // (Actual point generation will be added here later)
    graphPointsCount = 0; // Temporarily reset
}


int
main(int argc, char ** argv)
{
    // Configure Window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INITIAL_SCREEN_WIDTH, INITIAL_SCREEN_HEIGHT, "Function Plotter");
    SetTargetFPS(60);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

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
        float inputFieldHeight = 30;
        float inputFieldWidth = 100;
        float labelWidth = 60;
        float spacing = 10;
        float currentX = GRAPH_PADDING;
        float currentY = GRAPH_PADDING;

        // XMIN input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "xmin:");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, xminText, MAX_INPUT_CHARS, xminEditMode))
        {
            xminEditMode = !xminEditMode;
            if (!xminEditMode) xminVal = (float)atof(xminText); // Parse value when editing ends
        }
        currentX += inputFieldWidth + spacing;

        // XMAX input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "xmax:");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, xmaxText, MAX_INPUT_CHARS, xmaxEditMode))
        {
            xmaxEditMode = !xmaxEditMode;
            if (!xmaxEditMode) xmaxVal = (float)atof(xmaxText);
        }
        currentX += inputFieldWidth + spacing;

        // YMIN input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "ymin:");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, yminText, MAX_INPUT_CHARS, yminEditMode))
        {
            yminEditMode = !yminEditMode;
            if (!yminEditMode) yminVal = (float)atof(yminText);
        }
        currentX += inputFieldWidth + spacing;

        // YMAX input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "ymax:");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, ymaxText, MAX_INPUT_CHARS, ymaxEditMode))
        {
            ymaxEditMode = !ymaxEditMode;
            if (!ymaxEditMode) ymaxVal = (float)atof(ymaxText);
        }
        currentX += inputFieldWidth + spacing;

        currentX = GRAPH_PADDING; // Reset X for next row
        currentY += inputFieldHeight + spacing; // Move to next row

        // STEP input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "step:");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, stepText, MAX_INPUT_CHARS, stepEditMode))
        {
            stepEditMode = !stepEditMode;
            if (!stepEditMode)
            {
                stepVal = (float)atof(stepText);
                if (stepVal <= 0.0f) stepVal = 0.01f; // Ensure step is positive
                snprintf(stepText, MAX_INPUT_CHARS, "%.3f", stepVal); // Update text if changed
            }
        }
        currentX += inputFieldWidth + spacing;

        // Formula input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "f(x):");
        currentX += labelWidth;
        if (GuiTextBox((Rectangle){currentX, currentY, currentScreenWidth - currentX - GRAPH_PADDING - (BUTTON_WIDTH + spacing), inputFieldHeight}, formulaInputText, MAX_FORMULA_CHARS, formulaEditMode))
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
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "xmin:");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, xminText, MAX_INPUT_CHARS, xminEditMode);
        currentX += inputFieldWidth + spacing;

        // XMAX input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "xmax:");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, xmaxText, MAX_INPUT_CHARS, xmaxEditMode);
        currentX += inputFieldWidth + spacing;

        // YMIN input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "ymin:");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, yminText, MAX_INPUT_CHARS, yminEditMode);
        currentX += inputFieldWidth + spacing;

        // YMAX input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "ymax:");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, ymaxText, MAX_INPUT_CHARS, ymaxEditMode);
        currentX += inputFieldWidth + spacing;

        currentX = GRAPH_PADDING; // Reset X for next row
        currentY += inputFieldHeight + spacing; // Move to next row

        // STEP input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "step:");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, inputFieldWidth, inputFieldHeight}, stepText, MAX_INPUT_CHARS, stepEditMode);
        currentX += inputFieldWidth + spacing;

        // Formula input
        GuiLabel((Rectangle){currentX, currentY, labelWidth, inputFieldHeight}, "f(x):");
        currentX += labelWidth;
        GuiTextBox((Rectangle){currentX, currentY, currentScreenWidth - currentX - GRAPH_PADDING - (BUTTON_WIDTH + spacing), inputFieldHeight}, formulaInputText, MAX_FORMULA_CHARS, formulaEditMode);

        // Draw Graph button
        currentX = currentScreenWidth - GRAPH_PADDING - BUTTON_WIDTH;
        GuiButton((Rectangle){currentX, currentY, BUTTON_WIDTH, BUTTON_HEIGHT}, "Draw Graph"); // This GuiButton is for drawing, actual logic handled above.

        // Compile status message
        DrawText(compileStatusBuffer, GRAPH_PADDING, currentY + inputFieldHeight + spacing, 16, compileStatusBuffer[0] == 'E' ? RED : DARKGRAY);


        // --- Draw Graph Area ---
        Rectangle graphRect = {GRAPH_PADDING, GUI_AREA_HEIGHT,
                               (float)currentScreenWidth - 2 * GRAPH_PADDING,
                               (float)currentScreenHeight - GUI_AREA_HEIGHT - GRAPH_PADDING};

        DrawRectangleLinesEx(graphRect, 1, BLACK); // Bounding box for the graph area


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
