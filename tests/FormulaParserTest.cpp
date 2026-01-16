#include "CppUTest/TestHarness.h"

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
