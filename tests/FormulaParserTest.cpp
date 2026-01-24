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

TEST(FormulaParser, SanityCheck)
{
    double result = 0;
    int ret = parse_and_evaluate("1", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0, result, 1e-9);
}

TEST(FormulaParser, SimpleAddition)
{
    double result = 0;
    int ret = parse_and_evaluate("1 + 2", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(3.0, result, 1e-9);
}

TEST(FormulaParser, Precedence)
{
    double result = 0;
    int ret = parse_and_evaluate("1 + 2 * 3", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(7.0, result, 1e-9);
}

TEST(FormulaParser, Parentheses)
{
    double result = 0;
    int ret = parse_and_evaluate("(1 + 2) * 3", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(9.0, result, 1e-9);
}

TEST(FormulaParser, FloatingPoint)
{
    double result = 0;
    int ret = parse_and_evaluate("1.5 * 2.5", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(3.75, result, 1e-9);
}

TEST(FormulaParser, VariableX)
{
    double result = 0;
    int ret = parse_and_evaluate("x", 2.5, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(2.5, result, 1e-9);
}

TEST(FormulaParser, NegativeNumbers)
{
    double result = 0;
    int ret = parse_and_evaluate("-5", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(-5.0, result, 1e-9);
}

TEST(FormulaParser, EvaluateSameFormulaMultipleTimes)
{
    Formula* formula = formula_compile("x*x");
    CHECK_TRUE(formula != NULL);

    EvalResult result = formula_evaluate(formula, 2.0);
    CHECK_EQUAL(EVAL_ERROR_NONE, result.error);
    DOUBLES_EQUAL(4.0, result.value, 1e-9);

    result = formula_evaluate(formula, 3.0);
    CHECK_EQUAL(EVAL_ERROR_NONE, result.error);
    DOUBLES_EQUAL(9.0, result.value, 1e-9);

    formula_cleanup(formula);
}

TEST(FormulaParser, DivisionByZero)
{
    double result = 0;
    int ret = parse_and_evaluate("1/0", 0, &result);
    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, UnknownConstant)
{
    double result = 0;
    int ret = parse_and_evaluate("y", 0, &result);
    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, CosineFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("cos(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0, result, 1e-9);
}

TEST(FormulaParser, SineFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, TangentFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("tan(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, PowerFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("pow(2, 3)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(8.0, result, 1e-9);
}

TEST(FormulaParser, LogFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("log(1)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, Log10Function)
{
    double result = 0;
    int ret = parse_and_evaluate("log10(10)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0, result, 1e-9);
}

TEST(FormulaParser, ASinFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("asin(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, ACosFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("acos(1)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, ATanFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("atan(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(0.0, result, 1e-9);
}

TEST(FormulaParser, PiConstant)
{
    double result = 0;
    int ret = parse_and_evaluate("pi", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_PI, result, 1e-9);
}

TEST(FormulaParser, EConstant)
{
    double result = 0;
    int ret = parse_and_evaluate("e", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(M_E, result, 1e-9);
}

TEST(FormulaParser, ComplexFormulaWithConstants)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(pi/2) + cos(0) * e", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0 + M_E, result, 1e-9);
}