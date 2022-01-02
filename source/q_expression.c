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

#define LASSERT(args, cond, err) \
    if(!(cond)) {lval_del(args); return lval_err(err); }

typedef struct lval
{
    int type;
    double num;
    char* sym;
    char* err;
    int cell_count;

    struct lval **cell;
} lval;

enum LvalType { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

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

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
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
        
        case LVAL_QEXPR:
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
    if(strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    for(int i = 0; i < t->children_num; i++) {
        if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if(strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if(strcmp(t->children[i]->contents, "}") == 0) { continue; }
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
    case LVAL_QEXPR:
        lval_expr_print(v, '{', '}');
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
lval* builtin(lval* a, char* func);

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
    
    lval* x = builtin(v, s->sym);
    lval_del(s);

    return x;
}

lval* lval_eval(lval* v) {
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);

    return v;
}

lval* builtin_head(lval *a) {
    
    LASSERT(a, a->cell_count == 1, "Too many arguments for 'head'");
    
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type");
    
    LASSERT(a, a->cell[0]->cell_count != 0, "Function 'head' passed {}!");

    lval* v = take(a, 0);
    while(v->cell_count > 1) {
        lval_del(pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lval* a) {
    LASSERT(a, a->cell_count == 1, 
    "Too many arguments for 'head'");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'head' passed incorrect type");
    LASSERT(a, a->cell[0]->cell_count != 0, 
    "Function 'head' passed {}!");

    lval* v = take(a, 0);
    lval_del(pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT(a, a->cell_count == 1, 
    "Function 'eval' has too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'eval' passed incorrect type");

    lval* x = take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* lval_join(lval *x, lval *y) {
    while(y->cell_count > 0) {
        x = lval_add(x, pop(y, 0));
    }
    lval_del(y);
    return x;
}

lval* builtin_join(lval* a) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'join' passed incorrect type.");

    lval* x = pop(a, 0);
    while(a->cell_count > 0) {
        x = lval_join(x, pop(a, 0));
    }
    lval_del(a);
    return x;
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

lval* builtin(lval *a, char *func) {
    if(strcmp("list", func) == 0) { return builtin_list(a); }
    if(strcmp("head", func) == 0) { return builtin_head(a); }
    if(strcmp("tail", func) == 0) { return builtin_tail(a); }
    if(strcmp("join", func) == 0) { return builtin_join(a); }
    if(strcmp("eval", func) == 0) { return builtin_eval(a); }
    if(strstr("+*-/", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknow Function");
}


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
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    /* Define them with the following language */
    mpca_lang(MPCA_LANG_DEFAULT,
    "  \
      number: /-?[0-9]+/;                                           \
      symbol: \"list\" | \"head\" | \"tail\" |                      \
              \"join\" | \"eval\" | '+' | '*' |  '/' | '-' ;        \
      sexpr: '(' <expr>* ')' ;                                      \
      qexpr: '{' <expr>* '}' ;                                      \
      expr: <number> | <symbol> | <sexpr> | <qexpr> ;               \
      lispy: /^/ <expr>* /$/;                                       \
    ", Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

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
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}