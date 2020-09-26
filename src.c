#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"
#include "hash_table/hash_table.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
/* #include <editline/history.h> */
#endif

/* Forward environment and variable declarations */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Forward parser declarations */

mpc_parser_t* Number;
mpc_parser_t* Decimal;
mpc_parser_t* Boolean;
mpc_parser_t* Symbol;
mpc_parser_t* String;
mpc_parser_t* Comment;
mpc_parser_t* Sexpr;
mpc_parser_t* Qexpr;
mpc_parser_t* Expr;
mpc_parser_t* Lispy;


/* type enumerations */

enum { LVAL_ERR, LVAL_NUM,  LVAL_DEC, LVAL_SYM, LVAL_BOOL, LVAL_OK,
       LVAL_STR, LVAL_USTR, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

enum { LVAL_FALSE, LVAL_TRUE };

typedef lval*(*lbuiltin)(lenv*, lval*);

/* lval variable structure */
struct lval {
  int type;

  /* Basic */
  long num;
  double dec;
  int boo;
  char* err;
  char* sym;
  char* str;

  /* Function */
  lbuiltin builtin;
  char* fun_name;
  lenv* env;
  lval* formals;
  lval* body;

  /* Expression */
  int count;
  lval** cell;
};

/* lenv environment structure */
struct lenv {
  lenv* par;
  int quit;
  int count;
  char** syms;
  lval** vals;
};

/* We now define functions to manipulate types, some of these also manipulate the environment so we forward declare these operations here */
lenv* lenv_new(void);
void lenv_del(lenv*);
lenv* lenv_copy(lenv*);
void lenv_put(lenv*, lval*, lval*);
lval* lenv_get(lenv*, lval*);

/* Creation ops */
lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_dec(double x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_DEC;
	v->dec = x;
	return v;
}

lval* lval_bool(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_BOOL;
	v->boo = x;
	return v;
}

lval* lval_ok() {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_OK;
	return v;
}

lval* lval_err(char* fmt, ...){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;

	/* Create a va list and initialize it */
	va_list va;
	va_start(va, fmt);

	/* printf the error string with a maximum of 511 characters */
	v->err = malloc(512);
	vsnprintf(v->err, 511, fmt, va);

	/* Reallocate to number of bytes actually used */
	v->err = realloc(v->err, strlen(v->err) + 1);

	va_end(va);
	return v;
}

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_str(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_STR;
	v->str = malloc(strlen(s) + 1);
	strcpy(v->str, s);
	return v;
}

lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_fun(lbuiltin func, char* name) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	v->fun_name = malloc(strlen(name) + 1);
	strcpy(v->fun_name, name);
	return v;
}

lval* lval_lambda(lval* formals, lval* body) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;

	/* Set Builtin to Null */
	v->builtin = NULL;

	/* Build new environment */
	v->env = lenv_new();

	/* Set Formals and Body */
	v->formals = formals;
	v->body = body;
	return v;
}

char* ltype_name(int t) {
	switch(t) {
		case LVAL_FUN: return "Function";
		case LVAL_NUM: return "Number";
		case LVAL_DEC: return "Decimal";
		case LVAL_BOOL: return "Boolean";
		case LVAL_ERR: return "Error";
		case LVAL_SYM: return "Symbol";
		case LVAL_STR: return "String";
		case LVAL_SEXPR: return "S-Expression";
		case LVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
	}
}

void lval_del(lval* v) {

	switch (v->type) {
		/* Do nothing special for number or dec type */
		case LVAL_NUM: break;
		case LVAL_DEC: break;
		case LVAL_BOOL: break;

		/* Free up name for function */
		case LVAL_FUN:
			if (v->builtin) { free(v->fun_name); }
			else {
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
			break;


		/* For Err or Sym free the string data */
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		case LVAL_STR: free(v->str); break;

		/* If Qexpr or Sexpr then delete all elements inside */
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			for (int i = 0; i < v->count; i++) {
				lval_del(v->cell[i]);
			}
			/* also free the memory allocated to contain the pointers */
			free(v->cell);
		break;
		}
	/* Free the memory allocated for the "lval" struct itself */
	free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		lval_num(x) : lval_err("invalid number");
}

lval* lval_read_dec(mpc_ast_t* t) {
	errno = 0;
	double x = strtof(t->contents, NULL);
	return errno != ERANGE ?
		lval_dec(x): lval_err("invalid decimal");
}

lval* lval_read_bool(mpc_ast_t* t) {
	if (strcmp(t->contents, "true") == 0){
		return lval_bool(LVAL_TRUE);
	}
	if (strcmp(t->contents, "false") == 0) {
		return lval_bool(LVAL_FALSE);
	}
	return lval_err("Invalid Boolean");
}

lval* lval_read_str(mpc_ast_t* t) {
	/* Cut off the final quote character */
	t->contents[strlen(t->contents)-1] = '\0';
	/* Copy the string missing out the first quote character */
	char* unescaped = malloc(strlen(t->contents+1)+1);
	strcpy(unescaped, t->contents+1);
	/* Store the string in an unescaped format */
	unescaped = mpcf_unescape(unescaped);
	lval* str = lval_str(unescaped);

	free(unescaped);
	return str;
}


lval* lval_add(lval* v, lval* x) {
	/* Append one lval to a list type lval */
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}


lval* lval_read(mpc_ast_t* t) {
	/* If non-list lval, convert using custom conversion function */
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "decimal")) { return lval_read_dec(t); }
	if (strstr(t->tag, "boolean")) { return lval_read_bool(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	if (strstr(t->tag, "string")) { return lval_read_str(t); }

	/* If root (>) or list type then create empty list */
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
	if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

	/* Fill this list with any valid expression contained within */
	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
		if (strstr(t->children[i]->tag, "comment")) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

void lval_print_str(lval* v) {
	/* Allocate new space for escaped string and print between quotes */
	char* escaped = malloc(strlen(v->str)+1);
	strcpy(escaped, v->str);
	escaped = mpcf_escape(escaped);

	printf("\"%s\"", escaped);
	free(escaped);
}

void lval_print_ustr(lval* v) {
	/* Used for builtin show, does not escape string */
	printf("\"%s\"", v->str);
}

void lval_print (lval* v);

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
		lval_print(v->cell[i]);

		/* Don't print trailing space if last element */
		if (i != (v->count-1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

void lval_print(lval* v){
	switch (v->type){
		case LVAL_NUM: printf("%li", v->num); break;
		case LVAL_DEC: printf("%lf", v->dec); break;
		case LVAL_BOOL:
			if (v->boo == LVAL_TRUE) { printf("true"); }
			else { printf("false"); }; break;
		case LVAL_OK: break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_STR: lval_print_str(v); break;
		case LVAL_USTR: lval_print_ustr(v); break;
		case LVAL_FUN:
			if (v->builtin) {
				printf("<builtin>: %s", v->fun_name); break;
			} else {
				printf("(\\ "); lval_print(v->formals);
				putchar(' '); lval_print(v->body); putchar(')');
			}
			break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_copy(lval* v) {

  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {

    /* Copy Functions and Numbers Directly */
    case LVAL_DEC: x->dec = v->dec; break;
    case LVAL_NUM: x->num = v->num; break;
    case LVAL_BOOL: x->boo = v->boo; break;

    case LVAL_FUN:
      if (v->builtin) {
	      x->builtin = v->builtin;
      	      x->fun_name = malloc(strlen(v->fun_name + 1));
	      strcpy(x->fun_name, v->fun_name);
      } else {
	      x->builtin = NULL;
	      x->env = lenv_copy(v->env);
	      x->formals = lval_copy(v->formals);
	      x->body = lval_copy(v->body);
      }
      break;

    /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    case LVAL_STR:
      x->str = malloc(strlen(v->str) + 1);
      strcpy(x->str, v->str); break;


    /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
    break;
  }

  return x;
}

lval* lval_pop(lval* v, int i) {
	/* Find item and shift memory over the top */
	lval* x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1],
		sizeof(lval*) * (v->count-i-1));
	/* Decrease count and reallocate */
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);

	return x;
}

lval* lval_take(lval* v, int i) {
	/* Grab item from lval list and delete the rest of the structure */
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}


lval* lval_join(lval* x, lval* y) {
	/* Join two lval lists */
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}

	lval_del(y);
	return x;
}

lval* lval_str_join(lval* x, lval* y) {
	x->str = realloc(x->str, strlen(x->str) + strlen(y->str) + 1);
	strcat(x->str, y->str);

	lval_del(y);
	return x;
}

int lval_eq(lval* x, lval* y) {
	/* Check whether two lvals are equal */
	if (x->type != y->type) {
		/* If the type isn't equal, check the types are not numerical */
		if (!(x->type == LVAL_NUM || x->type == LVAL_DEC || x->type == LVAL_BOOL)) {
			return 0;
		}
		if (!(y->type == LVAL_NUM || y->type == LVAL_DEC || y->type == LVAL_BOOL)) {
			return 0;
		}
	}

	switch (x->type) {
		/* Compare Number/Decimal/Boolean Value */
		case LVAL_NUM:
			switch (y->type) {
				case LVAL_NUM:
					return (x->num == y->num);
				case LVAL_DEC:
					return (x->num == y->dec);
				case LVAL_BOOL:
					return (x->num == y->boo);
			}
		case LVAL_DEC:
			switch (y->type) {
				case LVAL_NUM:
					return (x->dec == y->num);
				case LVAL_DEC:
					return (x->dec == y->dec);
				case LVAL_BOOL:
					return (x->dec == y->boo);
			}
		case LVAL_BOOL:
			switch (y->type) {
				case LVAL_NUM:
					return (x->boo == y->num);
				case LVAL_DEC:
					return (x->boo == y->dec);
				case LVAL_BOOL:
					return (x->boo == y->boo);
			}
		/* Compare String values */
		case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
		case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);
		case LVAL_STR: return (strcmp(x->str, y->str) == 0);

		/* If builtin compare, otherwise compare formals and body */
		case LVAL_FUN:
			if (x->builtin || y->builtin) {
				return x->builtin == y->builtin;
			} else {
				return lval_eq(x->formals, y-> formals)
					&& lval_eq(x->body, y->body);
			}
		/* If list, compare every individual element */
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			if (x->count != y->count) { return 0; }
			for (int i = 0; i < x->count; i++) {
				if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
			}
			return 1;
		break;
	}
	return 0;
}

lval* lval_eval(lenv* e, lval* a);
lval* lval_call(lenv* e, lval* f, lval* a);
lval* lval_eval_sexpr(lenv* e, lval* v) {

	/* Evalutate Children */
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
	}

	/* Error Checking */
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) {return lval_take(v, i);}
	}

	/* Empty Expression */
	if (v->count == 0) { return v; }

	/* Single Expression */
	if (v->count == 1) { return lval_take(v, 0); }

	/* Ensure First Element is Symbol */
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval* err = lval_err(
				"S-Expression starts with incorrect type. "
				"Got %s, Expected %s. ",
				ltype_name(f->type), ltype_name(LVAL_FUN));
		lval_del(f); lval_del(v);
		return err;
	}

	/* Call builtin with operator */
	lval* result = lval_call(e, f, v);
	lval_del(f);
	return result;
}

lval* lval_eval(lenv* e, lval* v) {
	if (v->type == LVAL_SYM) {
	    	lval* x = lenv_get(e, v);
	    	lval_del(v);
		return x;
 	}
	/* Evalutate Sexpressions */
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
	/* All other lval types remain the same */
	return v;
}

lval* builtin_eval(lenv* e, lval* a);
lval* lval_call(lenv* e, lval* f, lval* a) {
	/* Apply function f to variable a */
	/* If Builtin then simply apply that */
	if (f->builtin) { return f->builtin(e, a); }

	/* Loop through all arguments in a */
	int given = a->count;
	int total = f->formals->count;
	while (a->count) {
		/* We bind each argument to a formal, if we run out, pass an error */
		if (f->formals->count == 0) {
			lval_del(a); return lval_err(
					"Function passed too many arguments. "
					"Got %i, Expected %i.", given, total);
		}

		/* Pop the first symbol from the formals */
		lval* sym = lval_pop(f->formals, 0);

		/* '&' denotes a variable number of arguments */
		if (strcmp(sym->sym, "&") == 0) {
			if (f->formals->count != 1) {
				lval_del(a);
				return lval_err("Function format invalid. "
						"symbol '&' not followed by single symbol.");
			}

			/* Next formal should be bound to remaining arguments */
			lval* nsym = lval_pop(f->formals, 0);
			a->type = LVAL_QEXPR;
			lenv_put(f->env, nsym, a);
			lval_del(sym); lval_del(nsym);
			break;
		}

		/* Bind a copy into the function's environment */
		lval* val = lval_pop(a, 0);
		lenv_put(f->env, sym, val);

		lval_del(sym); lval_del(val);
	}

	/* Argument list is now bound so can be cleaned up */
	lval_del(a);

	/* If '&' is remaining, bind to empty list */
	if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") ==0) {

		/* Check to ensure that & is not passed invalidly. */
		if (f->formals->count != 2) {
			return lval_err("Function format invalid. "
					"Symbol '&' not followed by single symbol. ");
		}
		/* Pop and delete '&' symbol */
		lval_del(lval_pop(f->formals, 0));

		/* Pop next symbol and create empty list */
		lval* sym = lval_pop(f->formals, 0);
		lval* val = lval_qexpr();

		/* Bind to environment and delete */
		lenv_put(f->env, sym, val);
		lval_del(sym); lval_del(val);
	}

	/* If all formals have been bound evaluate */
	if (f->formals->count == 0) {

		/* Set environment parent to evaluation environment */
		f->env->par = e;
		lval* v = lval_add(lval_sexpr(), lval_copy(f->body));
		v->cell[0]->type = LVAL_SEXPR;

		return lval_eval(f->env, v);
	} else {
		/* Otherwise return partially evaluated function */
		return lval_copy(f);
	}
}


/* We now define methods to manipulate the environment */
lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->par = NULL;
	e->quit = 0;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e) {
	for (int i = 0; i < e->count; i++) {
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lenv* lenv_copy(lenv* e) {
	lenv* n = malloc(sizeof(lenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*) * n->count);
	n->vals = malloc(sizeof(lval*) * n->count);
	for (int i = 0; i < e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

lval* lenv_get(lenv* e, lval* k) {

	/* Iterate over all items in environment */
	for (int i = 0; i < e->count; i++) {
		/* Check if the stored string matches the symbol string */
		/* If it does, return a copy of the value */
		if (strcmp(e->syms[i], k->sym) == 0) {
			return lval_copy(e->vals[i]);
		}
	}
	/* If no symbol check in parent otherwise error*/
	if (e->par) {
		return lenv_get(e->par, k);
	} else {
		return lval_err("Unbound Symbol '%s'", k->sym);
	}
}


void lenv_put(lenv* e, lval* k, lval* v) {

	/* Iterate over all items in environment */
	/* This is to see if variable already exists */
	for (int i = 0; i < e->count; i++) {
		/* If variable is found delete item at that position */
		/* And replace with variable supplied by user */
		if (strcmp(e->syms[i], k->sym) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}
	/* If no existing entry found allocate space for new entry */
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);
	/* Copy contents of lval and symbol string into new location */
	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}

void lenv_def(lenv* e, lval* k, lval* v) {
	/* ITerate till e has no parent */
	while (e->par) { e = e->par; }
	/* Put value in e */
	lenv_put(e, k, v);
}

lval* builtin_eval(lenv*, lval*);
lval* builtin_list(lenv*, lval*);



/* We now define builtin methods for the user, we use macros to check whether the builtin is being used correctly */
#define LASSERT(args, cond, fmt, ...) \
	if (!(cond)) { \
		lval* err = lval_err(fmt, ##__VA_ARGS__); \
		lval_del(args);  \
		return err; \
	}
#define TYPE_CHECK(args, num, lval_type, fun_name) \
	LASSERT(args, args->cell[num]->type == lval_type, \
			"Function %s passed incorrect type for argument %i. " \
			"Got %s, Expected %s", \
			fun_name, num, \
			ltype_name(args->cell[num]->type), ltype_name(lval_type))

#define NUM_CHECK(args, num, fun_name) \
	if (!(args->cell[num]->type == LVAL_DEC || args->cell[num]->type == LVAL_NUM)) { \
		lval* err = lval_err("Function %s expected either number or decimal type " \
				     "for argument %i. Got %s." \
				     fun_name, num, ltype_name(args->cell[num]->type)); \
		lval_del(args); \
		return err; \
	}

#define CHECK_ARG_NUM(args, num, fun_name) \
	if (!(args->count == num)) { \
		lval* err = lval_err("Function %s passed incorrect number of arguments. " \
				"Got %i, Expected %i", \
				fun_name, args->count, num); \
		lval_del(args);  \
		return err; \
	}

# define CHECK_EMPTY(args, fun_name) \
	if (!(args->cell[0]->count > 0)) { \
		lval* err = lval_err("Function %s was passed empty argument", \
				fun_name); \
		lval_del(args); \
		return err; \
       	}

lval* dec_op(lval* a, char* op){
	/* Numerical operation function for decimal types */
	lval* x = lval_pop(a, 0);

	/* If no arguments and sub then perform unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->dec = -x->dec;
	}
	/* If no arguments and not op, then reverse boolean */
	if (strcmp(op, "!") == 0) {
		x->type = LVAL_BOOL;
		if (x->dec == 0) {
			x->boo = 1;
		} else {
			x->boo = 0;
		}
	}

	/* Otherwise, process each argument individually */
	while (a->count > 0) {

		lval* y = lval_pop(a, 0);

		/* Arithmetic ops */
		if (strcmp(op, "+") == 0) { x->dec += y->dec; }
		if (strcmp(op, "-") == 0) { x->dec -= y->dec; }
		if (strcmp(op, "*") == 0) { x->dec *= y->dec; }
		if (strcmp(op, "/") == 0) {
			if (y->dec == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!"); break;
			}
			x->dec /= y->dec;
		}
		if (strcmp(op, "%") == 0) { return lval_err("Can't compute remainder on decimal types!");}

		/* Comparison ops */
		if (strcmp(op, ">") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec > y->dec);
		}
		if (strcmp(op, "<") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec < y->dec);
		}
		if (strcmp(op, ">=") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec >= y->dec);
		}
		if (strcmp(op, "<=") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec <= y->dec);
		}
		if (strcmp(op, "||") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec || y->dec);
		}
		if (strcmp(op, "&&") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->dec && y->dec);
		}
		lval_del(y);
	}
	lval_del(a); return x;

}

lval* num_op(lval* a, char* op) {
	/* Numerical operation function for decimal types */
	lval* x = lval_pop(a, 0);

	/* If no arguments and sub then perform unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}
	/* If no arguments and not op, then reverse boolean */
	if (strcmp(op, "!") == 0) {
		x->type = LVAL_BOOL;
		if (x->num == 0) {
			x->boo = LVAL_FALSE;
		} else {
			x->boo = LVAL_TRUE;
		}
	}

	/* Otherwise, process each argument individually */
	while (a->count > 0) {

		lval* y = lval_pop(a, 0);

		/* Arithmetic ops */
		if (strcmp(op, "+") == 0) { x->num += y->num; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!"); break;
			}
			x->num /= y->num;
		}
		if (strcmp(op, "%") == 0) {
			/* Check there are no more elements */
			if (a->count > 0) { return lval_err("Remainder operator takes only two arguments!"); }
			x->num %= y->num;
		}

		/* Comparison ops */
		if (strcmp(op, ">") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num > y->num);
		}
		if (strcmp(op, "<") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num < y->num);
		}
		if (strcmp(op, ">=") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num >= y->num);
		}
		if (strcmp(op, "<=") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num <= y->num);
		}
		if (strcmp(op, "||") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num || y->num);
		}
		if (strcmp(op, "&&") == 0) {
			x->type = LVAL_BOOL;
			x->boo = (x->num && y->num);
		}
		lval_del(y);
	}
	lval_del(a); return x;

}


lval* builtin_op(lenv* e, lval* a, char* op) {
	/* If decimal is present we convert everything to decimal type */
	int is_dec = 0;
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			/* If boolean, convert to num for the purpose of the operation */
			if (a->cell[i]->type == LVAL_BOOL) {
				a->cell[i]->type = LVAL_NUM;
				a->cell[i]->num = (long) a->cell[i]->boo;
				continue;
			}
			/* If a decimal, make a note of this and continue */
			if (a->cell[i]->type == LVAL_DEC) {
				is_dec = 1;
				continue;
			}
			return lval_err("Function %s passsed incorrect type for argument %i. "
					"Got %s, expected %s or %s",
					op, i, ltype_name(a->cell[i]->type),
					ltype_name(LVAL_NUM), ltype_name(LVAL_DEC));
			lval_del(a);
		}
	}

	/* If decimal exists convert everything and run dec_op */
	if (is_dec) {
		for (int i = 0; i < a->count; i++) {
			if (a->cell[i]->type == LVAL_NUM) {
				a->cell[i]->type = LVAL_DEC;
				a->cell[i]->dec = (double) a->cell[i]->num;
			}
		}
		return dec_op(a, op);
	}

	/* If no decimal then run num_op */
	return num_op(a, op);

}

/* Builtin arithmetic operations */
lval* builtin_add(lenv* e, lval* a) {
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval*a) {
	return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
	return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
	return builtin_op(e, a, "/");
}

/* Builtin comparison operations */
lval* builtin_gt(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, ">")
	return builtin_op(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, "<")
	return builtin_op(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, ">=")
	return builtin_op(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, "<=")
	return builtin_op(e, a, "<=");
}

lval* builtin_or(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, "||")
	return builtin_op(e, a, "||");
}

lval* builtin_and(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, "&&")
	return builtin_op(e, a, "&&");
}

lval* builtin_not(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "!")
	return builtin_op(e, a, "!");
}



lval* builtin_cmp(lenv* e, lval* a, char* op) {
	CHECK_ARG_NUM(a, 2, op);
	int r;
	if (strcmp(op, "==") == 0) {
		r = lval_eq(a->cell[0], a->cell[1]);
	}
	if (strcmp(op, "!=") == 0) {
		r = !lval_eq(a->cell[0], a->cell[1]);
	}
	lval_del(a);
	return lval_bool(r);
}

lval* builtin_eq(lenv* e, lval* a) {
	return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
	return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 3, "if")
	/* Convert first argument to boolean type if numerical */
	if (a->cell[0]->type == LVAL_NUM) {
		a->cell[0]->type = LVAL_BOOL;
		if (a->cell[0]->num) {
			a->cell[0]->boo = LVAL_TRUE;
		} else {
			a->cell[0]->boo = LVAL_FALSE;
		}
	} else if (a->cell[0]->type == LVAL_DEC) {
		a->cell[0]->type = LVAL_BOOL;
		if (a->cell[0]->dec) {
			a->cell[0]->boo = LVAL_TRUE;
		} else {
			a->cell[0]->boo = LVAL_FALSE;
		}
	} else if (a->cell[0]->type != LVAL_BOOL) {
		return lval_err("Function if pass incorrected type for argument 0"
				"Got %s, Expected Number, Decimal or Bool",
				ltype_name(a->cell[0]->type));
	}
	TYPE_CHECK(a, 1, LVAL_QEXPR, "if")
	TYPE_CHECK(a, 2, LVAL_QEXPR, "if")
	lval* v;
	a->cell[1]->type = LVAL_SEXPR;
	a->cell[2]->type = LVAL_SEXPR;

	/* To evaluate expressions, add to sexpr and run eval */
	if (a->cell[0]->boo) {
		v = lval_eval(e, lval_pop(a, 1));
	} else {
		v = lval_eval(e, lval_pop(a, 2));
	}
	lval_del(a);
	return v;
}

lval* builtin_qexpr_head(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_QEXPR, "head")
	CHECK_EMPTY(a, "head")

	lval* v = lval_take(a, 0);
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_str_head(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_STR, "head")

	lval* v = lval_take(a, 0);
	v->str = realloc(v->str, 2);
	v->str[1] = '\0';
	return v;
}

lval* builtin_head(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "head");
	if (a->cell[0]->type == LVAL_QEXPR) {
		return builtin_qexpr_head(e, a);
	} else {
		return builtin_str_head(e, a);
	}
}

lval* builtin_qexpr_tail(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_QEXPR, "tail");
	CHECK_EMPTY(a,	"tail");

	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_str_tail(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_STR, "tail");

	lval* v = lval_take(a, 0);
	char tail = v->str[strlen(v->str) - 1];
	v->str = realloc(v->str, 2);
	v->str[0] = tail;
	v->str[1] = '\0';
	return v;
}

lval* builtin_tail(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "tail");
	if (a->cell[0]->type == LVAL_QEXPR) {
		return builtin_qexpr_tail(e, a);
	} else {
		return builtin_str_tail(e, a);
	}
}

lval* builtin_cons(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 2, "cons")
	TYPE_CHECK(a, 1, LVAL_QEXPR, "cons");

	lval* v = lval_qexpr();
	v = lval_add(v, a->cell[0]);
	v = lval_join(v, a->cell[1]);
	return v;
}

lval* builtin_len(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "len")
	TYPE_CHECK(a, 0, LVAL_QEXPR, "len");

	return lval_num(a->cell[0]->count);
}

lval* builtin_init(lenv* e, lval* a) {
	/* returns all but the final element of a qexpr */
	CHECK_ARG_NUM(a, 1, "init")
	TYPE_CHECK(a, 0, LVAL_QEXPR, "init");

	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v, v->count-1));
	return v;
}

lval* builtin_list(lenv* e, lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_read(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_STR, "read")
	CHECK_ARG_NUM(a, 1, "read")

	mpc_result_t r;
	/* Parse input through mpc */
	if (mpc_parse("<stdin>", a->cell[0]->str, Lispy, &r)) {

		lval* expr = lval_read(r.output);
		expr->type = LVAL_QEXPR;

		/* Delete expressions and arguments */
		mpc_ast_delete(r.output);
		lval_del(a);

		return expr;
	} else {
		/* Create error message using parse error */
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);
		lval* err = lval_err("Could not read: %s", err_msg);

		free(err_msg);
		lval_del(a);
		return err;
	}
}

lval* builtin_show(lenv* e, lval* a) {
	/* Prints string in unescaped form */
	TYPE_CHECK(a, 0, LVAL_STR, "show")
	CHECK_ARG_NUM(a, 1, "show")

	lval* v = lval_take(a, 0);
	v->type = LVAL_USTR;

	lval_print(v);
	putchar('\n');

	lval_del(v);
	return lval_ok();
}

lval* builtin_lambda(lenv* e, lval* a) {
	/* Create lambda function */
	CHECK_ARG_NUM(a, 2, "\\")
	TYPE_CHECK(a, 0, LVAL_QEXPR, "\\")
	TYPE_CHECK(a, 1, LVAL_QEXPR, "\\")

	/* Check first Q-expression contains only Symbols */
	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
				"Cannot define non-symbol. Got %s, Expected %s.",
				ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
	}

	/* Pop first two arguments and pass them to lval_lambda */
	lval* formals = lval_pop(a, 0);
	lval* body = lval_pop(a, 0);
	lval_del(a);

	return lval_lambda(formals, body);
}

lval* builtin_var(lenv* e, lval* a, char* func) {
	/* Assign symbols to values */
	TYPE_CHECK(a, 0, LVAL_QEXPR, func);

	lval* syms = a->cell[0];

	/* Ensure all elements of first list are symbols */
	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, syms->cell[i]->type == LVAL_SYM,
				"Function '%s' cannot define non-symbol"
				"Got %s, Expected %s. ",
				func, ltype_name(syms->cell[i]->type),
			       	ltype_name(LVAL_SYM));
	}

	/* Check correct number of symbols and values */
	LASSERT(a, syms->count == a->count-1,
			"Function '%s' passed too many arguments for symbols "
			"Got %i symbols and %i values",
			func, syms->count, a->count-1);

	/* Assign copies of values to symbols */
	for (int i = 0; i < syms->count; i++) {
		/* If 'def' define in globally. If 'put' define in locally */
		if (strcmp(func, "def") == 0) {
			lenv_def(e, syms->cell[i], a->cell[i+1]);
		}

		if (strcmp(func, "=") == 0) {
			lenv_put(e, syms->cell[i], a->cell[i+1]);
		}
	}

	lval_del(a);
	return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
	return builtin_var(e, a, "def");
}

lval* builtin_fun(lenv* e, lval* a) {
	lval* fun_name = lval_add(lval_qexpr(), lval_pop(a->cell[0], 0));
	lval* fun = builtin_lambda(e, a);

	lval* sexpr = lval_add(lval_sexpr(), fun_name);
	sexpr = lval_add(sexpr, fun);
	return builtin_def(e, sexpr);
}

lval* builtin_put(lenv* e, lval* a) {
	return builtin_var(e, a, "=");
}

lval* builtin_eval(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "eval")
	TYPE_CHECK(a, 0, LVAL_QEXPR, "eval")

	lval* x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}


lval* builtin_qexpr_join(lenv* e, lval* a) {
	for (int i = 0; i < a->count; i++) {
		TYPE_CHECK(a, i, LVAL_QEXPR, "qexpr join");
	}
	lval* x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

lval* builtin_str_join(lenv* e, lval* a) {
	for (int i = 0; i< a->count; i++) {
		TYPE_CHECK(a, i, LVAL_STR, "string join")
	}
	lval* x = lval_pop(a, 0);

	while (a->count) {
		x = lval_str_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}


lval* builtin_join(lenv* e, lval* a) {
	if (a->cell[0]->type == LVAL_QEXPR){
		return builtin_qexpr_join(e, a);
	} else {
		return builtin_str_join(e, a);
	}
}

lval* builtin_list_env(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_SEXPR, "list_env")
	CHECK_ARG_NUM(a, 1, "list_env")
	LASSERT(a, a->cell[0]->count == 0, "list_env expects empty sexpr as argument, "
			"received sexpr with %i arguments", a->cell[0]->count)


	lval_del(a);

	lval* v = lval_qexpr();
	/* Iterate over all items in environment */
	for (int i = 0; i < e->count; i++) {
		v = lval_add(v, lval_sym(e->syms[i]));
   	}
	return v;
}

lval* builtin_exit(lenv* e, lval* a) {
	TYPE_CHECK(a, 0, LVAL_SEXPR, "exit")
	CHECK_ARG_NUM(a, 1, "exit")
	LASSERT(a, a->cell[0]->count == 0, "exit expects empty sexpr as argument, "
			"received sexpr with %i arguments", a->cell[0]->count)
	e->quit = 1;
	lval_del(a);
	return lval_sym("Exiting Prompt");
}

/* Defined all builtins, now load them into environment */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_fun(func, name);
	lenv_put(e, k, v);
	lval_del(k); lval_del(v);
}

lval* builtin_load(lenv* e, lval* a) {
	/* Run contents of file */
	CHECK_ARG_NUM(a, 1, "load")
	TYPE_CHECK(a, 0, LVAL_STR, "load")

	mpc_result_t r;
	if (mpc_parse_contents(a->cell[0]->str, Lispy, &r)) {

		/* Read contents */
		lval* expr = lval_read(r.output);
		mpc_ast_delete(r.output);

		/* Evalutate each expression */
		while (expr->count) {
			lval* x = lval_eval(e, lval_pop(expr, 0));
			/* If Evaluation leads to error print it */
			if (x->type == LVAL_ERR) { lval_println(x); }
			lval_del(x);
		}

		/* Delete expressions and arguments */
		lval_del(expr);
		lval_del(a);

		/* Return empty list */
		return lval_sexpr();
	} else {
		/* Get parse Error as String */
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		/* Create new error message using it */
		lval* err = lval_err("Could not load Library %s", err_msg);
		free(err_msg);
		lval_del(a);

		/* Cleanup and return error */
		return err;
	}
}

lval* builtin_print(lenv* e, lval* a) {
	for (int i = 0; i < a->count; i++) {
		lval_print(a->cell[i]); putchar(' ');
	}

	/* Print a newline and delete arguments */
	putchar('\n');
	lval_del(a);
	return lval_ok();
}

lval* builtin_error(lenv* e, lval* a) {
	CHECK_ARG_NUM(a, 1, "error")
	TYPE_CHECK(a, 0, LVAL_STR, "error")

	/* Construct Error from first argument */
	lval* err = lval_err(a->cell[0]->str);

	/* Delete arguments and return */
	lval_del(a);
	return err;
}

void lenv_add_builtins(lenv* e) {
	/* List Functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "cons", builtin_cons);
	lenv_add_builtin(e, "len", builtin_len);
	lenv_add_builtin(e, "init", builtin_init);

	/* Environment and parsing functions */
	lenv_add_builtin(e, "list_env", builtin_list_env);
	lenv_add_builtin(e, "exit", builtin_exit);
	lenv_add_builtin(e, "\\", builtin_lambda);
	lenv_add_builtin(e, "load", builtin_load);
	lenv_add_builtin(e, "error", builtin_error);
	lenv_add_builtin(e, "print", builtin_print);
	lenv_add_builtin(e, "read", builtin_read);
	lenv_add_builtin(e, "show", builtin_show);

	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "fun", builtin_fun);
	lenv_add_builtin(e, "=", builtin_put);

	/* Arithmetic and Comparison Funtions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, ">", builtin_gt);
	lenv_add_builtin(e, "<", builtin_lt);
	lenv_add_builtin(e, ">=", builtin_ge);
	lenv_add_builtin(e, "<=", builtin_le);
	lenv_add_builtin(e, "==", builtin_eq);
	lenv_add_builtin(e, "!=", builtin_ne);
	lenv_add_builtin(e, "if", builtin_if);
	lenv_add_builtin(e, "||", builtin_or);
	lenv_add_builtin(e, "&&", builtin_and);
	lenv_add_builtin(e, "!", builtin_not);

}

long count_child(mpc_ast_t* t){
	/* Function to compute number of child nodes */
	if (t->children_num == 0){
			return 1;
	}

	/* if not, then has multiple children, loop through them */
	int count = 0;
	int child_num = 0;
	while(child_num < t->children_num){
		count += count_child(t->children[child_num]);
		child_num += 1;
	}
	return count;
}

int main(int argc, char** argv){
	/* Define parsers */
	Number = mpc_new("number");
	Decimal = mpc_new("decimal");
	Boolean = mpc_new("boolean");
	Symbol = mpc_new("symbol");
	String = mpc_new("string");
	Comment = mpc_new("comment");
	Sexpr  = mpc_new("sexpr");
	Qexpr = mpc_new("qexpr");
	Expr = mpc_new("expr");
	Lispy = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
		"					\
		number		: /-?[0-9]+/ ; \
		decimal		: /-?[0-9]+\\.[0-9]*/ ;		\
		boolean		: \"true\" | \"false\" ;    \
		symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&|]+/ ;         \
		string		: /\"(\\\\.|[^\"])*\"/ ; 	\
		comment		: /;[^\\r\\n]*/ ;		\
		sexpr 		: '(' <expr>* ')' ; \
		qexpr 		: '{' <expr>* '}' ; \
		expr 		: <decimal> | <number> | <boolean> | <symbol> \
	       			| <string> | <comment> |  <sexpr> | <qexpr> ; \
		lispy		: /^/   <expr>*  /$/ ; \
		",
		Number, Decimal, Boolean, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
	/* Create environment, load builtins and standard library */
	lenv* e = lenv_new();
	lenv_add_builtins(e);
	lval* v = builtin_load(e, lval_add(lval_sexpr(), lval_str("stlib.jdl")));
	lval_del(v);


	/* Supplied with list of files */
	if (argc >= 2) {
		for (int i = 1; i < argc; i++) {
			/* Argument list with a single argument, the filename */
			lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
			lval* x = builtin_load(e, args);

			/* If the result is an error be sure to print it */
			if (x->type == LVAL_ERR) { lval_println(x); }
			lval_del(x);
		}
	} else {
		/* Print Version and Exit Information */

		puts("JDlisp Version 1.0");
		puts("Made by Jamied");
		puts("Press Ctrl+c to Exit\n");

		while (1){
			/* Output our prompt and get input */
			char* input = readline("jdlisp> ");

			/* Add input to history */
			add_history(input);

			/* Attempt to Parse the user Input */
			mpc_result_t r;
			if (mpc_parse("<stdin>", input, Lispy, &r)){
				lval* x = lval_eval(e, lval_read(r.output));
				lval_println(x);
				lval_del(x);
				mpc_ast_delete(r.output);
			} else {
				/* Otherwise Print the Error */
				mpc_err_print(r.error);
				mpc_err_delete(r.error);
			}

			/* Free retrieved input */
			free(input);

			/* Check if exit function raised */
			if (e->quit == 1) {
				break;
			}
		}
	}
	lenv_del(e);
	/* Undefine and Delete our Parsers */
	mpc_cleanup(10, Number, Decimal, Boolean, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

	return 0;
}

