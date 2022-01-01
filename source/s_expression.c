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

// ----------------------------------------

typedef struct lval
{
    int type;
    double num;
    char* sym;
    char* err;
    int cell_count;

    struct lval **cell;
} lval;

enum LvalType { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

enum LvalError { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_num(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m)+1);
    strcpy(v->err, m);
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s)+1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->cell = NULL;
    v->cell_count = 0;
    return v;
}

void lval_del(lval* v) {
    switch(v->type) {
        case LVAL_NUM:
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SEXPR:
            for(int i=0; i<v->cell_count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval *v, lval* x) {
    v->cell_count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->cell_count);
    v->cell[v->cell_count-1] = x;
    return v;
}

lval* lval_read(mpc_ast_t* t) {
    if(strstr(t->tag, "number")) { return lval_read_num(t); }
    if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    lval* x = NULL;
    if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if(strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

    for(int i = 0; i < t->children_num; i++) {
        if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if(strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for(int i=0; i < v->cell_count; i++) {
        lval_print(v->cell[i]);

        if(i != v->cell_count - 1) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v) {

    switch (v->type)
    {
    case LVAL_NUM:
        printf("%f", v->num);
        break;
    case LVAL_ERR:
        printf("%s", v->err);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    default:
        break;
    }

}

void lval_println(lval *v) { 
    lval_print(v); 
    putchar('\n');
}

lval* pop(lval *v, int i) {
    lval *x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * v->cell_count-1-i);
    v->cell_count--;
    v->cell = realloc(v->cell, sizeof(lval*) * v->cell_count);

    return x;
}

lval* take(lval* v, int i) {
    lval *x = pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_eval(lval* v);
lval* builtin_op(lval* v, char* op);

lval* lval_eval_sexpr(lval* v) {

    for(int i=0; i<v->cell_count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    for(int i=0; i<v->cell_count; i++) {
        if(v->cell[i]->type == LVAL_ERR) {
            return take(v, i);
        }
    }

    if(v->cell_count == 0) {
        return v;
    }

    if(v->cell_count == 1) {
        return take(v,0);
    }

    lval* s = pop(v,0);
    if(s->type != LVAL_SYM) {
        lval_del(v);
        lval_del(s);
        return lval_err("S-expression does not start with a symbol");
    }
    
    lval* x = builtin_op(v, s->sym);
    lval_del(s);

    return x;
}

lval* lval_eval(lval* v) {
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);

    return v;
}

lval* builtin_op(lval *v, char *op) {

    for(int i=0; i<v->cell_count; i++) {
        if(v->cell[i]->type != LVAL_NUM) {
            lval_del(v);
            return lval_err("Error! cannot operate on non-number");
        }
    }

    lval* x = pop(v, 0);

    if((strcmp(op, "-") == 0) && v->cell_count == 1) {
        x->num = -x->num;
    }

    while (v->cell_count > 0)
    {
        lval* y = pop(v, 0);

        if(strcmp(op, "+") == 0) { x->num += y->num; }
        if(strcmp(op, "-") == 0) { x->num -= y->num; }
        if(strcmp(op, "*") == 0) { x->num *= y->num; }
        if(strcmp(op, "/") == 0) { 
            if(y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Error! dividing by 0");
                break;
            }
            x->num /= y->num;
         }
        
        lval_del(y);
    }

    lval_del(v);

    return x;
}

// lval eval_op(char *op, lval oper1, lval oper2) {
//     if(oper1.type == LVAL_ERR) { return oper1; }
//     if(oper2.type == LVAL_ERR) { return oper2; }

//     if(strcmp(op, "+") == 0) { return lval_num(oper1.num + oper2.num); }
//     if(strcmp(op, "*") == 0) { return val_num(oper1.num * oper2.num); }
//     if(strcmp(op, "-") == 0) { return lval_num(oper1.num - oper2.num); }
//     if(strcmp(op, "/") == 0) { 
//         return oper2.num == 0 ? 
//         lval_err(LERR_DIV_ZERO) : 
//         lval_num(oper1.num / oper2.num); 
//         }
//     return lval_err(LERR_BAD_OP);
// }

// lval eval(mpc_ast_t *t) {
//     if(strstr(t->tag, "number")) {
//         errno = 0;
//         long x = strtol(t->contents, NULL, 10);

//         return errno != ERANGE ? create_lval_num(x) : create_lval_err(LERR_BAD_NUM);
//     }

//     char *op = t->children[1]->contents;

//     lval x = eval(t->children[2]);

//     int i=3;
    
//     while(strstr(t->children[i]->tag, "expr")) {
//         x = eval_op(op, x, eval(t->children[i]));
//         i++;
//     }

//     return  x;
// }

int countLeaves(mpc_ast_t *t) {
    if(strstr( t->tag, "symbol") || strstr(t->tag, "number") || strstr(t->tag, "char")){
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
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    /* Define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
    "  \
      number: /-?[0-9]+/;                                \
      symbol:  '+' | '*' |  '/' | '-' ;                  \
      sexpr: '(' <expr>* ')' ;                           \
      expr: <number> | <symbol> | <sexpr> ;              \
      lispy: /^/ <expr>* /$/;                            \
    ", Number, Symbol, Sexpr, Expr, Lispy);

    fputs("Lispy Version 0.0.0.0.1\n", stdout);
    fputs("Press Ctrl+c to Exit\n", stdout);

    while(1){

        char *input = readline("Lispy> ");

        add_history(input);

        // printf("No you're a %s\n", input);
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)){
            mpc_ast_print(r.output);
            lval *result = lval_eval( lval_read(r.output));
            lval_println(result);
            // printf("Leaves: %i\n", countLeaves(r.output));
            lval_del(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    
    /* Undefine and delete parser */
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
    return 0;
}