#include "CppUTest/TestHarness.h"

#include <math.h>

extern "C"
{
#include "formula_parser.h"
}

TEST_GROUP(FormulaParser)
{
    Formula * f = NULL;

    void setup()
    {
    }

    void teardown()
    {
        formula_cleanup(f);
        f = NULL;
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
    f = formula_compile("x*x");
    CHECK_TRUE(f != NULL);
    POINTERS_EQUAL(formula_get_last_error(f), NULL);

    EvalResult result = formula_evaluate(f, 2.0);
    CHECK_EQUAL(EVAL_ERROR_NONE, result.error);
    DOUBLES_EQUAL(4.0, result.value, 1e-9);

    result = formula_evaluate(f, 3.0);
    CHECK_EQUAL(EVAL_ERROR_NONE, result.error);
    DOUBLES_EQUAL(9.0, result.value, 1e-9);
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

TEST(FormulaParser, PowerFunctionWithTooManyArguments)
{
    double result = 0;
    int ret = parse_and_evaluate("pow(2, 3, 4)", 0, &result);
    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, PowerFunctionWithTooFewArguments)
{
    double result = 0;
    int ret = parse_and_evaluate("pow(2)", 0, &result);
    CHECK_EQUAL(-1, ret);
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

TEST(FormulaParser, FormulaWithConstant)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(pi/2) + e", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0 + M_E, result, 1e-9);
}

TEST(FormulaParser, ComplexFormula)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(pi/2) + cos(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(2.0, result, 1e-9);
}

TEST(FormulaParser, ComplexFormulaNoSpaceAfterPlus)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(pi/2) +cos(0)", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(2.0, result, 1e-9);
}

TEST(FormulaParser, ComplexFormulaWithConstants)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(pi/2) + cos(0) * e", 0, &result);
    CHECK_EQUAL(0, ret);
    DOUBLES_EQUAL(1.0 + M_E, result, 1e-9);
}

TEST(FormulaParser, FormulaWithTrailingWhitespace)
{
    f = formula_compile("1 ");

    CHECK(f != NULL);
    POINTERS_EQUAL(formula_get_last_error(f), NULL);
}

TEST(FormulaParser, FormulaWithTrailingGarbage)
{
    f = formula_compile("1 blah");

    CHECK(f != NULL);
    CHECK(formula_get_last_error(f) != NULL);
}

TEST(FormulaParser, FormulaWithTrailingWhitespaceAfterGarbage)
{
    f = formula_compile("1 blah ");

    CHECK(f != NULL);
    CHECK(formula_get_last_error(f) != NULL);
}

TEST(FormulaParser, CosZeroSeparateCompileandEval)
{
    f = formula_compile("cos(0)");
    CHECK(f != NULL);
    POINTERS_EQUAL(formula_get_last_error(f), NULL);

    double x = 0;
    EvalResult const eval_result = formula_evaluate(f, 0);
    CHECK(eval_result.error == EVAL_ERROR_NONE);
}

TEST(FormulaParser, NonExistentFunction)
{
    double result = 0;
    int ret = parse_and_evaluate("non_existent_func(1)", 0, &result);
    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, SingleArgFunctionTooManyArgs)
{
    double result = 0;
    int ret = parse_and_evaluate("sin(1, 2)", 0, &result);
    CHECK_EQUAL(-1, ret);
}

TEST(FormulaParser, MultiArgFunctionTooFewArgs)
{
    double result = 0;
    int ret = parse_and_evaluate("pow()", 0, &result);
    CHECK_EQUAL(-1, ret);
}