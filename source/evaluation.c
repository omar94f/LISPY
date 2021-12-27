#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

// __APPLE__ for mac
#ifdef _WIN32

#include <string.h>

static char buffer[2048];

char* readline(char *prompt){
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *cpy = malloc(strlen(buffer)+1);
    strcpy(cyp, buffer);
    coy[strlen(cpy)-1] = '\0';

    return cpy;
}

void add_history(char *unused) {}

#else

#include <editline/readline.h>

#endif

long eval_op(char *op, long oper1, long oper2) {
    if(strcmp(op, "+") == 0) { return oper1 + oper2; }
    if(strcmp(op, "*") == 0) { return oper1 * oper2; }
    if(strcmp(op, "-") == 0) { return oper1 - oper2; }
    if(strcmp(op, "/") == 0) { return oper1 / oper2; }
    return 0;
}

long eval(mpc_ast_t *t) {
    if(strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    char *op = t->children[1]->contents;

    long x = eval(t->children[2]);

    int i=3;
    
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(op, x, eval(t->children[i]));
        i++;
    }

    return  x;
}

int countLeaves(mpc_ast_t *t) {
    if(strstr( t->tag, "operator") || strstr(t->tag, "number") || strstr(t->tag, "char")){
        return 1;
    }
    
    int x = 0;
    int i = 0;
    while(i < t->children_num){
        x = x + countLeaves(t->children[i]);
        i++;
    }
    return x; 
}

int main(int arc, char** argv) {
    /* create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    /* Define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
    "  \
      number: /-?[0-9]+/;                                   \
      operator:  '+' | '*' |  '/' | '-' ;                   \
      expr: <number> | '(' <operator> <expr>+ ')' ;         \
      lispy: /^/ <operator> <expr>+ /$/;  \
    ", Number, Operator, Expr, Lispy);

    fputs("Lispy Version 0.0.0.0.1\n", stdout);
    fputs("Press Ctrl+c to Exit\n", stdout);

    while(1){

        char *input = readline("Lispy> ");

        add_history(input);

        // printf("No you're a %s\n", input);
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)){
            mpc_ast_print(r.output);
            printf("Evaluation: %li\n", eval(r.output));
            printf("Leaves: %i\n", countLeaves(r.output));
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    
    /* Undefine and delete parser */
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}