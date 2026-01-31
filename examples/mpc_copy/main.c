#include <stdio.h>
#include "mpc.h"

typedef struct { char *m; } mpc_pdata_fail_t;
typedef struct { mpc_ctor_t lf; void *x; } mpc_pdata_lift_t;
typedef struct { mpc_parser_t *x; char *m; } mpc_pdata_expect_t;
typedef struct { int(*f)(char,char); } mpc_pdata_anchor_t;
typedef struct { char x; } mpc_pdata_single_t;
typedef struct { char x; char y; } mpc_pdata_range_t;
typedef struct { int(*f)(char); } mpc_pdata_satisfy_t;
typedef struct { char *x; } mpc_pdata_string_t;
typedef struct { mpc_parser_t *x; mpc_apply_t f; } mpc_pdata_apply_t;
typedef struct { mpc_parser_t *x; mpc_apply_to_t f; void *d; } mpc_pdata_apply_to_t;
typedef struct { mpc_parser_t *x; mpc_dtor_t dx; mpc_check_t f; char *e; } mpc_pdata_check_t;
typedef struct { mpc_parser_t *x; mpc_dtor_t dx; mpc_check_with_t f; void *d; char *e; } mpc_pdata_check_with_t;
typedef struct { mpc_parser_t *x; } mpc_pdata_predict_t;
typedef struct { mpc_parser_t *x; mpc_dtor_t dx; mpc_ctor_t lf; } mpc_pdata_not_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t *x; mpc_dtor_t dx; } mpc_pdata_repeat_t;
typedef struct { int n; mpc_parser_t **xs; } mpc_pdata_or_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t **xs; mpc_dtor_t *dxs;  } mpc_pdata_and_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t *x; mpc_parser_t *sep; } mpc_pdata_sepby1;

typedef union {
  mpc_pdata_fail_t fail;
  mpc_pdata_lift_t lift;
  mpc_pdata_expect_t expect;
  mpc_pdata_anchor_t anchor;
  mpc_pdata_single_t single;
  mpc_pdata_range_t range;
  mpc_pdata_satisfy_t satisfy;
  mpc_pdata_string_t string;
  mpc_pdata_apply_t apply;
  mpc_pdata_apply_to_t apply_to;
  mpc_pdata_check_t check;
  mpc_pdata_check_with_t check_with;
  mpc_pdata_predict_t predict;
  mpc_pdata_not_t not;
  mpc_pdata_repeat_t repeat;
  mpc_pdata_and_t and;
  mpc_pdata_or_t or;
  mpc_pdata_sepby1 sepby1;
} mpc_pdata_t;

struct mpc_parser_t {
  char *name;
  mpc_pdata_t data;
  char type;
  char retained;
};

int main(int argc, char **argv) {

    printf("--- MPC Copy Example with mpc_define ---\n\n");

    /* mpc_define is used to define a previously created (undefined) parser.
     * Crucially, it consumes/destroys the parser it is given.
     * If you want to use that parser again, you must copy it first.
     */

    /* Create some parsers to define others with */
    mpc_parser_t *Digit = mpc_re("[0-9]");
    mpc_parser_t *Lower = mpc_re("[a-z]");

    printf("Created 'Digit' and 'Lower' parsers.\n");

    /* Create two undefined parsers */
    mpc_parser_t *Parser1 = mpc_new("parser1");
    mpc_parser_t *Parser2 = mpc_new("parser2");

    printf("Defining 'Parser1' as 'Digit'...\n");
    /* We pass a COPY of Digit, so the original is not destroyed. */
    mpc_parser_t * Digit_copy = mpc_copy(Digit);
    mpc_define(Parser1, Digit_copy);

    printf("Defining 'Parser2' as 'Lower'...\n");
    /* We pass a COPY of Lower, so the original is not destroyed. */
    mpc_parser_t * Lower_copy = mpc_copy(Lower);
    mpc_define(Parser2, Lower_copy);

    printf("\n'Parser1' and 'Parser2' are now defined.\n");
    printf("The original 'Digit' and 'Lower' parsers still exist because we used mpc_copy().\n");

    /* We can now use all four parsers */
    mpc_result_t r;
    if (mpc_parse("<string>", "5", Parser1, &r)) {
        printf("\nSuccessfully parsed '5' with Parser1 (defined as Digit).\n");
        // mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }

    if (mpc_parse("<string>", "a", Parser2, &r)) {
        printf("Successfully parsed 'a' with Parser2 (defined as Lower).\n");
        // mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }

    if (mpc_parse("<string>", "9", Digit, &r)) {
        printf("Successfully parsed '9' with the original 'Digit' parser.\n");
        // mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }

    /* Clean up all the parsers */
    printf("\nCleaning up...\n");
    mpc_cleanup(4, Digit, Lower, Parser1, Parser2);
    printf("Cleanup complete.\n");

    return 0;
}