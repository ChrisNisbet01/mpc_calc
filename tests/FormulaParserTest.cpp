#include "CppUTest/TestHarness.h"

#include <math.h>

extern "C"
{
#include "formula_parser.h"
}

TEST_GROUP(FormulaParser)
{
    void setup()
    {
    }
    void teardown()
    {
    }
};

TEST(FormulaParser, SimpleAddition)
{
    char const * formula = "2 + 3";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(5.0, result, 0.001);
}

TEST(FormulaParser, Precedence)
{
    char const * formula = "2 + 3 * 4";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(14.0, result, 0.001);
}

TEST(FormulaParser, Parentheses)
{
    char const * formula = "(2 + 3) * 4";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(20.0, result, 0.001);
}

TEST(FormulaParser, FloatingPoint)
{
    char const * formula = "10.5 / 2.0";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(5.25, result, 0.001);
}

TEST(FormulaParser, VariableX)
{
    char const * formula = "x * (x + 1)";
    double x = 3.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(12.0, result, 0.001);
}

TEST(FormulaParser, NegativeNumbers)
{
    char const * formula = "-5 * -2";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(10.0, result, 0.001);
}

TEST(FormulaParser, InvalidSyntax)
{
    char const * formula = "2 + * 3";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, EvaluateSameFormulaMultipleTimes)
{
    char const * formula_str = "x * (x + 2)";
    Formula * formula = formula_compile(formula_str);
    CHECK_TRUE(formula != NULL);

    double result = 0.0;
    int ret = 0;

    /* First evaluation */
    ret = formula_evaluate(formula, 3.0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(15.0, result, 0.001);

    /* Second evaluation */
    ret = formula_evaluate(formula, 4.0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(24.0, result, 0.001);

    formula_cleanup(formula);
}

TEST(FormulaParser, CosineFunction)
{
    char const * formula = "cos(0)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0, result, 0.001);
}

TEST(FormulaParser, SineFunction)
{
    char const * formula = "sin(0)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 0.001);
}

TEST(FormulaParser, TangentFunction)
{
    char const * formula = "tan(0)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 0.001);
}

TEST(FormulaParser, PowerFunction)
{
    char const * formula = "pow(2, 3)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(8.0, result, 0.001);
}

TEST(FormulaParser, LogFunction)
{
    char const * formula = "log(1)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 0.001);
}

TEST(FormulaParser, Log10Function)
{
    char const * formula = "log10(10)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0, result, 0.001);
}

TEST(FormulaParser, ASinFunction)
{
    char const * formula = "asin(0.5)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_PI / 6.0, result, 0.001);
}

TEST(FormulaParser, ACosFunction)
{
    char const * formula = "acos(0.0)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_PI / 2.0, result, 0.001);
}

TEST(FormulaParser, ATanFunction)
{
    char const * formula = "atan(1.0)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_PI / 4.0, result, 0.001);
}

TEST(FormulaParser, PiConstant)
{
    char const * formula = "pi";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_PI, result, 0.001);
}

TEST(FormulaParser, EConstant)
{
    char const * formula = "e";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_E, result, 0.001);
}

TEST(FormulaParser, ComplexFormulaWithConstants)
{
    char const * formula = "sin(pi/2) + log(e)";
    double x = 0.0;
    double result = 0.0;
    int const ret = parse_and_evaluate(formula, x, &result);

    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(2.0, result, 0.001);
}

