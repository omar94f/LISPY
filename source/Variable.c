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

#define LASSERT(args, cond, fmt, ...) \
    if(!(cond)) { \
     lval* err = lval_err(fmt, ##__VA_ARGS__); \
     lval_del(args); \
     return err; \
    }


struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval
{
    int type;
    double num;
    char* sym;
    char* err;
    lbuiltin fun;
    int cell_count;

    struct lval **cell;
} ;

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

enum LvalType { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

char* ltype_name(int t) {
    switch (t)
    {
    case LVAL_NUM: return "Number";
    case LVAL_SYM: return "Symbol";
    case LVAL_FUN: return "Function";
    case LVAL_ERR: return "Error";
    case LVAL_QEXPR: return "Q-Expression";
    case LVAL_SEXPR: return "S-Expression";
    default: return "Unknown";
    }
}

lval* lval_copy(lval* v);
void lval_del(lval* v);
lval* lval_err(char* fmt, ...);

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for(int i=0; i<e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    for(int i=0; i<e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("Unbound symbol: %s", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
    for(int i=0; i<e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0 ) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

lval* lval_num(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);

    vsnprintf(v->err, 511, fmt, va);

    v->err= realloc(v->err, strlen(v->err)+1);
    va_end(va);

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

lval* lval_fun(lbuiltin fun) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = fun;
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
        case LVAL_FUN:
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

lval* lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type)
    {
    case LVAL_NUM:
        x->num = v->num;
        break;
    case LVAL_FUN:
        x->fun = v->fun;
        break;
    case LVAL_ERR:
        x->err = malloc(strlen(v->err)+1);
        strcpy(x->err, v->err);
        break;
    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym)+1);
        strcpy(x->sym, v->sym);
        break;
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        x->cell_count = v->cell_count;
        x->cell = malloc(sizeof(lval*) * x->cell_count);
        for(int i=0; i<x->cell_count; i++) {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
    }
    return x;
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
    case LVAL_FUN:
        printf("<function>");
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

lval* lval_eval(lenv* e, lval* v);
lval* builtin_op(lenv* e, lval* v, char* op);
lval* builtin(lval* a, char* func);

lval* lval_eval_sexpr(lenv* e, lval* v) {

    for(int i=0; i<v->cell_count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
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

    lval* f = pop(v,0);
    if(f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("first element is not a function");
    }
    
    lval* x = f->fun(e, v);
    lval_del(f);

    return x;
}

lval* lval_eval(lenv* e, lval* v) {
    if(v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(e, v);

    return v;
}

lval* builtin_head(lenv* e, lval *a) {
    
    LASSERT(a, a->cell_count == 1, "Too many arguments for 'head'. Got %i expected %i", a->cell_count, 1);
    
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type for argument 0"
     " Got %s expected %s", ltype_name(a->type), ltype_name(LVAL_QEXPR));
    
    LASSERT(a, a->cell[0]->cell_count != 0, "Function 'head' passed {}!");

    lval* v = take(a, 0);
    while(v->cell_count > 1) {
        lval_del(pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
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

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, a->cell_count == 1, 
    "Function 'eval' has too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'eval' passed incorrect type");

    lval* x = take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lval *x, lval *y) {
    while(y->cell_count > 0) {
        x = lval_add(x, pop(y, 0));
    }
    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'join' passed incorrect type.");

    lval* x = pop(a, 0);
    while(a->cell_count > 0) {
        x = lval_join(x, pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval* builtin_op(lenv* e, lval *v, char *op) {

    for(int i=0; i<v->cell_count; i++) {
        if(v->cell[i]->type != LVAL_NUM) {
            lval_del(v);
            return lval_err("Error! cannot operate on type %s", ltype_name(v->cell[i]->type));
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

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_def(lenv* e, lval* a) {
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, 
    "Function 'def' passed incorrect type");

    lval* syms = a->cell[0];

    for(int i=0; i<syms->cell_count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
        "Function 'def' cannot define non-symbol");
    }

    LASSERT(a, syms->cell_count == a->cell_count-1,
    "Function 'def' cannot define incorrect number of values to symbols");

    for(int i=0; i<syms->cell_count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {

    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    lenv_add_builtin(e, "def", builtin_def);
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
      symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
      sexpr: '(' <expr>* ')' ;                                      \
      qexpr: '{' <expr>* '}' ;                                      \
      expr: <number> | <symbol> | <sexpr> | <qexpr> ;               \
      lispy: /^/ <expr>* /$/;                                       \
    ", Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    fputs("Lispy Version 0.0.0.0.1\n", stdout);
    fputs("Press Ctrl+c to Exit\n", stdout);

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1){

        char *input = readline("Lispy> ");

        add_history(input);

        // printf("No you're a %s\n", input);
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)){
            mpc_ast_print(r.output);
            lval *result = lval_eval(e, lval_read(r.output));
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

    lenv_del(e);
    
    /* Undefine and delete parser */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}