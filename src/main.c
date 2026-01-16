#include "formula_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for sprintf

#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define MAX_TEXT_LENGTH 128

int
main(int argc, char ** argv)
{
    char const * formula_str = "x * x + 2 * x + 1";
    Formula * formula = NULL;
    double result1 = 0.0;
    double result2 = 0.0;
    int ret1 = 0;
    int ret2 = 0;

    char text_buffer1[MAX_TEXT_LENGTH] = {0};
    char text_buffer2[MAX_TEXT_LENGTH] = {0};
    char compile_status_buffer[MAX_TEXT_LENGTH] = {0};

    /* Compile the formula */
    printf("Compiling formula: \"%s\"\n", formula_str);
    formula = formula_compile(formula_str);

    if (NULL == formula)
    {
        fprintf(stderr, "Failed to compile formula.\n");
        snprintf(compile_status_buffer, MAX_TEXT_LENGTH, "Compile Failed!");
    }
    else
    {
        snprintf(compile_status_buffer, MAX_TEXT_LENGTH, "Compile Success: %s", formula_str);

        /* Evaluate with x = 2.0 */
        double const x1 = 2.0;
        ret1 = formula_evaluate(formula, x1, &result1);
        if (0 == ret1)
        {
            snprintf(text_buffer1, MAX_TEXT_LENGTH, "x = %.2f -> Result: %.2f", x1, result1);
        }
        else
        {
            snprintf(text_buffer1, MAX_TEXT_LENGTH, "x = %.2f -> Evaluation Failed", x1);
        }

        /* Evaluate with x = 3.0 */
        double const x2 = 3.0;
        ret2 = formula_evaluate(formula, x2, &result2);
        if (0 == ret2)
        {
            snprintf(text_buffer2, MAX_TEXT_LENGTH, "x = %.2f -> Result: %.2f", x2, result2);
        }
        else
        {
            snprintf(text_buffer2, MAX_TEXT_LENGTH, "x = %.2f -> Evaluation Failed", x2);
        }
    }

    /* Initialize RayLib window */
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MPC Calculator Results");
    SetTargetFPS(60);

    /* Main game loop */
    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText(compile_status_buffer, 50, 50, 20, DARKGRAY);
        DrawText(text_buffer1, 50, 100, 20, DARKBLUE);
        DrawText(text_buffer2, 50, 130, 20, DARKGREEN);
        DrawText("Press ESC to close", 50, SCREEN_HEIGHT - 50, 20, LIGHTGRAY);

        EndDrawing();
    }

    /* Clean up RayLib and formula resources */
    CloseWindow();
    if (NULL != formula)
    {
        formula_cleanup(formula);
    }

    /* Return success if both evaluations were successful, else failure */
    return (0 == ret1 && 0 == ret2) ? 0 : -1;
}
