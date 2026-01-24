#include "CppUTest/TestHarness.h"
#include "mpc/mpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C"
{

/*
 * A dummy AST structure for demonstration purposes.
 * We don't need a complex AST, just something to represent a parsed result.
 */
typedef struct {
    char *value;
} DummyAST;

static mpc_val_t *dummy_ctor(mpc_val_t *val) {
    DummyAST *ast = (DummyAST *)malloc(sizeof(DummyAST));
    ast->value = strdup((char *)val);
    free(val);
    return ast;
}

static void dummy_dtor(mpc_val_t *val) {
    DummyAST *ast = (DummyAST *)val;
    if (ast) {
        free(ast->value);
        free(ast);
    }
}

static mpc_val_t *dummy_fold_str(int n, mpc_val_t **xs) {
    char *result = (char *)malloc(1);
    result[0] = '\0';
    for (int i = 0; i < n; i++) {
        result = (char *)realloc(result, strlen(result) + strlen((char *)xs[i]) + 1);
        strcat(result, (char *)xs[i]);
        free(xs[i]);
    }
    return result;
}

static mpc_val_t *dummy_fold_ast(int n, mpc_val_t **xs) {
    DummyAST *ast = (DummyAST *)malloc(sizeof(DummyAST));
    char *result_str = (char *)dummy_fold_str(n, xs);
    ast->value = result_str;
    return ast;
}

static void dummy_dtor_str(mpc_val_t *val) {
    free((char *)val);
}


} // extern "C"

TEST_GROUP(MpcCopy)
{
    void setup()
    {
    }

    void teardown()
    {
    }
};

/*
 * This test demonstrates a use-after-free scenario if mpc_copy() is not used.
 * We define a grammar where one rule (Expr) depends on another (Term).
 * If Term is redefined without copying its previous definition,
 * Expr might end up holding a pointer to freed memory when Term is modified.
 */
TEST(MpcCopy, UseAfterFreeDemonstration)
{
    mpc_parser_t *Term = mpc_new("term");
    mpc_parser_t *Expr = mpc_new("expr");

    // Initial definition for Term: matches a number
    mpc_define(Term, mpc_apply(mpc_re("[0-9]+"), dummy_ctor));

    // Expr depends on Term
    mpc_define(Expr, mpc_and(2, dummy_fold_str, Term, mpc_re("[+]"), Term, dummy_dtor_str, dummy_dtor_str));

    // Try parsing with the initial grammar
    mpc_result_t r;
    CHECK_TRUE(mpc_parse("<input>", "1+2", Expr, &r));
    DummyAST *initial_ast = (DummyAST *)r.output;
    STRCMP_EQUAL("1+2", initial_ast->value);
    dummy_dtor(initial_ast);

    // Now, redefine Term WITHOUT mpc_copy(). This will free the internal parser of Term.
    // If Expr is not updated (or if it held a direct pointer), it's now dangling.
    mpc_undefine(Term); // Explicitly undefine to simulate replacement of internal structure
    mpc_define(Term, mpc_apply(mpc_re("[a-zA-Z]+"), dummy_ctor)); // New definition for Term: matches a letter

    // Attempt to parse with Expr. If Expr still uses the old Term, it might crash or
    // exhibit undefined behavior (use-after-free).
    // Note: MPC's internal mechanisms might prevent an immediate crash in simple cases,
    // but this illustrates the principle. A more complex scenario could trigger it.
    // We expect this parse to fail because "1+2" doesn't match the new Term definition.
    // The key here is that if it *didn't* fail cleanly, it could be a UAF.
    CHECK_FALSE(mpc_parse("<input>", "1+2", Expr, &r));
    mpc_err_delete(r.error); // Clean up error for failed parse

    // A valid parse with the new Term definition and old Expr structure
    CHECK_TRUE(mpc_parse("<input>", "a+b", Expr, &r));
    DummyAST *new_ast = (DummyAST *)r.output;
    STRCMP_EQUAL("a+b", new_ast->value);
    dummy_dtor(new_ast);

    mpc_cleanup(2, Term, Expr);
}

/*
 * This test demonstrates how mpc_copy() prevents use-after-free.
 * Here, Expr copies a *safe* version of Term. When Term is redefined,
 * Expr's copy remains valid.
 */
TEST(MpcCopy, NoUseAfterFreeWithMpcCopy)
{
    mpc_parser_t *Term = mpc_new("term");
    mpc_parser_t *Expr = mpc_new("expr");

    // Initial definition for Term: matches a number
    mpc_define(Term, mpc_apply(mpc_re("[0-9]+"), dummy_ctor));

    // Expr depends on a COPY of Term
    mpc_define(Expr, mpc_and(2, dummy_fold_str, mpc_copy(Term), mpc_re("[+]"), mpc_copy(Term), dummy_dtor_str, dummy_dtor_str));

    // Try parsing with the initial grammar
    mpc_result_t r;
    CHECK_TRUE(mpc_parse("<input>", "1+2", Expr, &r));
    DummyAST *initial_ast = (DummyAST *)r.output;
    STRCMP_EQUAL("1+2", initial_ast->value);
    dummy_dtor(initial_ast);

    // Now, redefine Term. Expr's copy of Term remains valid.
    mpc_undefine(Term);
    mpc_define(Term, mpc_apply(mpc_re("[a-zA-Z]+"), dummy_ctor)); // New definition for Term

    // This parse should still work cleanly, using Expr's *copied* definition of Term (numbers).
    // It should fail to match "a+b" because the copy of Term in Expr is still numeric.
    CHECK_FALSE(mpc_parse("<input>", "a+b", Expr, &r));
    mpc_err_delete(r.error);

    // This parse should still work cleanly, using Expr's *copied* definition of Term (numbers).
    CHECK_TRUE(mpc_parse("<input>", "3+4", Expr, &r)); // Should still parse numbers
    DummyAST *valid_ast = (DummyAST *)r.output;
    STRCMP_EQUAL("3+4", valid_ast->value);
    dummy_dtor(valid_ast);

    mpc_cleanup(2, Term, Expr);
}

/*
 * This test demonstrates a memory leak if mpc_copy() is used improperly,
 * specifically if the copied parser is never freed.
 */
TEST(MpcCopy, MemoryLeakDemonstration)
{
    // CppUTest's MemoryLeakDetector helps detect leaks
    // MemoryLeakDetectorInstaller installer;
    // installer.install();

    // Create a parser
    mpc_parser_t *Original = mpc_new("original");
    mpc_define(Original, mpc_apply(mpc_re("test"), dummy_ctor));

    // Copy the parser, but then only clean up the original.
    // The copy is never passed to mpc_cleanup or mpc_delete.
    mpc_parser_t *Copy = mpc_copy(Original);

    // Use the copy
    mpc_result_t r;
    CHECK_TRUE(mpc_parse("<input>", "test", Copy, &r));
    dummy_dtor(r.output); // Clean up result of parse

    // Clean up only the original parser
    mpc_cleanup(1, Original);
    // The 'Copy' parser and its internal structures are now leaked.

    // CppUTest will report a memory leak here if enabled.
    // To explicitly demonstrate the leak without relying on the framework,
    // you would need to track malloc/free calls manually or use a tool like Valgrind.
    // For this test, we rely on CppUTest's detection.
}