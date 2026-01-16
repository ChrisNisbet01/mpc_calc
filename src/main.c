#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

int main(int argc, char** argv) {
  /* Create Some Parsers */
  mpc_parser_t* number   = mpc_new("number");
  mpc_parser_t* operator = mpc_new("operator");
  mpc_parser_t* expr     = mpc_new("expr");
  mpc_parser_t* lispy    = mpc_new("lispy");

  /* Define them with a string */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    number, operator, expr, lispy);

  puts("MPC Calculator");
  puts("Press Ctrl+c to Exit\n");

  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.1");

  if (argc > 1) {
      mpc_result_t r;
      if (mpc_parse("<stdin>", argv[1], lispy, &r)) {
          mpc_ast_print(r.output);
          mpc_ast_delete(r.output);
      } else {
          mpc_err_print(r.error);
          mpc_err_delete(r.error);
      }
  } else {
      puts("Usage: mpc_calc_app \"<expression>\"");
  }

  /* Undefine and Delete our Parsers */
  mpc_cleanup(4, number, operator, expr, lispy);

  return 0;
}
