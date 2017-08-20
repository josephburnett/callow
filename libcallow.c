#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

typedef enum
    { NIL, SYMBOL, NUMBER, ERROR, LIST, LAMBDA, RECUR, MACRO, FUNC,
ENV } type;

typedef int64_t chunk_t;

typedef struct {
    type type;
    chunk_t value;
} value_t;

typedef struct cell_t {
    value_t car;
    struct cell_t *cdr;
} cell_t;

typedef struct {
    value_t names;
    value_t form;
    value_t env;
} lambda_t;

typedef struct {
    value_t names;
    value_t form;
    value_t env;
} macro_t;

typedef value_t(*func_fn) (value_t, value_t);

typedef struct {
    func_fn func;
} func_t;

typedef struct {
    value_t value;
    value_t env;
} env_t;

typedef struct {
    value_t params;
    value_t env;
} recur_t;

value_t read(FILE * fp);

value_t read_string(char form[])
{
    FILE *stream;
    stream = fmemopen(form, strlen(form), "r");
    value_t v = read(stream);
    fclose(stream);
    return v;
}

value_t read_file(char *filename)
{
    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL) {
	return (value_t) {
	ERROR, (chunk_t) "Error opening file"};
    }
    value_t form = read(fp);
    fclose(fp);
    return form;
}

value_t list(FILE * fp, int length)
{
    value_t car = read(fp);
    if (car.type == ERROR) {
	return car;
    }
    if (car.type == LIST && car.value == 0) {
	if (length == 0) {
	    // Special case nil
	    return (value_t) {
	    NIL, 0};
	} else {
	    // End of non-zero length list
	    return car;
	}
    }
    value_t cdr = list(fp, length + 1);
    if (cdr.type == ERROR) {
	return cdr;
    }
    cell_t *cl = malloc(sizeof(cell_t));
    cl->car = car;
    cl->cdr = (cell_t *) cdr.value;
    return (value_t) {
    LIST, (chunk_t) cl};
}

value_t symbol(FILE * fp)
{
    value_t v = (value_t) { SYMBOL, 0 };
    int ch;
    int index = 0;
    while ((ch = getc(fp)) != EOF) {
	if (index == 8) {
	    return (value_t) {
	    ERROR, (chunk_t) "Symbol too long."};
	}
	switch (ch) {
	case 'a' ... 'z':
	case '0' ... '9':
	case '-':
	    ((char *) &v.value)[index++] = ch;
	    continue;
	default:
	    ungetc(ch, fp);
	    return v;
	}
    }
    return v;
}

value_t number(FILE * fp)
{
    int ch;
    value_t v = { NUMBER, 0 };
    chunk_t sign = 0;
    while ((ch = getc(fp)) != EOF) {
	switch (ch) {
	case '-':
	    if (sign != 0) {
		return (value_t) {
		ERROR, (chunk_t) "Invalid character '-' in number."};
	    }
	    sign = -1;
	    continue;
	case '0' ... '9':
	    if (sign == 0) {
		sign = 1;
	    }
	    v.value = v.value * 10 + (ch - '0');
	    continue;
	case 'a' ... 'z':
	    return (value_t) {
	    ERROR, (chunk_t) "Invalid letter in number."};
	default:
	    ungetc(ch, fp);
	    v.value = v.value * sign;
	    return v;
	}
    }
    v.value = v.value * sign;
    return v;
}

value_t string(FILE * fp)
{
    int ch = getc(fp);
    if (ch == EOF) {
	return (value_t) {
	ERROR, (chunk_t) "Unexpected EOF while reading string."};
    }
    if (ch == '"') {
	return (value_t) {
	NIL, 0};
    }
    value_t car = { SYMBOL, 0 };
    ((char *) &car.value)[0] = ch;
    value_t cdr = string(fp);
    if (cdr.type == ERROR) {
	return cdr;
    }
    cell_t *cl = malloc(sizeof(cell_t));
    cl->car = car;
    cl->cdr = (cell_t *) cdr.value;
    return (value_t) {
    LIST, (chunk_t) cl};
}

value_t read(FILE * fp)
{
    int ch;
    while ((ch = getc(fp)) != EOF) {
	switch (ch) {
	case ' ':
	case '\n':
	    continue;
	case '(':
	    return list(fp, 0);
	case ')':
	    return (value_t) {
	    LIST, 0};
	case '-':
	case '0' ... '9':
	    ungetc(ch, fp);
	    return number(fp);
	case 'a' ... 'z':
	    ungetc(ch, fp);
	    return symbol(fp);
	case '"':
	    return string(fp);
	default:
	    return (value_t) {
	    ERROR, (chunk_t) "Invalid character."};
	}
    }
    return (value_t) {
    ERROR, (chunk_t) "Unexpected EOF."};
}

void print_index(FILE * fp, value_t v, int index)
{
    if (index > 0) {
	fprintf(fp, " ");
    }
    switch (v.type) {
    case LIST:
	if (index <= 0) {
	    fprintf(fp, "(");
	}
	print_index(fp, ((cell_t *) v.value)->car, 0);
	cell_t *cdr = ((cell_t *) v.value)->cdr;
	if (cdr != 0) {
	    print_index(fp, (value_t) {
			LIST, (chunk_t) cdr}
			, index + 1);
	}
	if (index <= 0) {
	    fprintf(fp, ")");
	}
	break;
    case SYMBOL:
	{
	    char *sym = (char *) &(v.value);
	    int i;
	    for (i = 0; i < 8; i++) {
		if (sym[i] != 0) {
		    fprintf(fp, "%c", sym[i]);
		}
	    }
	    break;
	}
    case NUMBER:
	fprintf(fp, "%" PRId64, v.value);
	break;
    case FUNC:
	fprintf(fp, "<func>");
	break;
    case LAMBDA:
	fprintf(fp, "<lambda>");
	break;
    case RECUR:
	fprintf(fp, "<recur>");
	break;
    case MACRO:
	fprintf(fp, "<macro>");
	break;
    case ERROR:
	fprintf(fp, "<error: %s>", (char *) v.value);
	break;
    case NIL:
	fprintf(fp, "()");
	break;
    case ENV:
	print_index(fp, ((env_t *) v.value)->value, index);
	break;
    }
}

void print(FILE * fp, value_t v)
{
    print_index(fp, v, 0);
}

int len(value_t v, int l)
{
    if (v.type != LIST) {
	return l;
    }
    cell_t *c = (cell_t *) v.value;
    if (c == 0) {
	return l;
    }
    return len((value_t) {
	       LIST, (chunk_t) c->cdr}
	       , l + 1);
}

value_t eval(value_t v, value_t env);
value_t eval_args(value_t list, value_t env, int limit);

value_t atom(value_t args, value_t env)
{
    if (len(args, 0) != 1) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for atom."};
    }
    args = eval_args(args, env, -1);
    args = ((cell_t *) args.value)->car;
    if (args.type == LIST) {
	return (value_t) {
	NIL, 0};
    }
    return (value_t) {
    SYMBOL, 't'};
}

value_t car(value_t args, value_t env)
{
    if (len(args, 0) != 1) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for car."};
    }
    args = eval_args(args, env, -1);
    args = ((cell_t *) args.value)->car;
    if (args.type != LIST) {
	return (value_t) {
	ERROR, (chunk_t) "Non-list argument to car."};
    }
    return ((cell_t *) args.value)->car;
}

value_t cdr(value_t args, value_t env)
{
    if (len(args, 0) != 1) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for crd."};
    }
    args = eval_args(args, env, -1);
    args = ((cell_t *) args.value)->car;
    if (args.type != LIST) {
	return (value_t) {
	ERROR, (chunk_t) "Non-list argument to crd."};
    }
    cell_t *c = ((cell_t *) args.value)->cdr;
    if (c == 0) {
	return (value_t) {
	NIL, 0};
    }
    return (value_t) {
    LIST, (chunk_t) c};
}

value_t cond(value_t args, value_t env)
{
    if (len(args, 0) < 2) {
	return (value_t) {
	ERROR, (chunk_t) "Uneven number of args to cond."};
    }
    args = eval_args(args, env, 2);
    value_t pred = ((cell_t *) args.value)->car;
    value_t val = ((cell_t *) args.value)->cdr->car;
    if (pred.value != 0) {
	return val;
    }
    cell_t *c = ((cell_t *) args.value)->cdr->cdr;
    if (c == 0) {
	return (value_t) {
	ERROR, (chunk_t) "No matching condition."};
    }
    return cond((value_t) {
		LIST, (chunk_t) c}
		, env);
}

value_t cons(value_t args, value_t env)
{
    if (len(args, 0) != 2) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for cons."};
    }
    args = eval_args(args, env, -1);
    value_t car = ((cell_t *) args.value)->car;
    value_t cdr = ((cell_t *) args.value)->cdr->car;
    if (cdr.type != LIST && cdr.type != NIL) {
	return (value_t) {
	ERROR, (chunk_t) "Non list or nil arg to cons."};
    }
    cell_t *c = malloc(sizeof(cell_t));
    c->car = car;
    c->cdr = 0;
    if (cdr.type == LIST) {
	c->cdr = (cell_t *) cdr.value;
    }
    return (value_t) {
    LIST, (chunk_t) c};
}

value_t eq_internal(value_t a, value_t b)
{
    if (a.type != b.type) {
	return (value_t) {
	NIL, 0};
    }
    if (a.type == LIST) {
	value_t first_eq = eq_internal(((cell_t *) a.value)->car,
				       ((cell_t *) b.value)->car);
	if (first_eq.type == SYMBOL) {
	    value_t rest_a =
		(value_t) { LIST, (chunk_t) ((cell_t *) a.value)->cdr };
	    value_t rest_b =
		(value_t) { LIST, (chunk_t) ((cell_t *) b.value)->cdr };
	    if (rest_a.value == 0) {
		rest_a.type = NIL;
	    }
	    if (rest_b.value == 0) {
		rest_b.type = NIL;
	    }
	    return eq_internal(rest_a, rest_b);
	} else {
	    return first_eq;
	}
    } else if (a.value != b.value) {
	return (value_t) {
	NIL, 0};
    }
    return (value_t) {
    SYMBOL, 't'};
}

value_t eq(value_t args, value_t env)
{
    if (len(args, 0) != 2) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity to eq."};
    }
    args = eval_args(args, env, -1);
    value_t a = ((cell_t *) args.value)->car;
    value_t b = ((cell_t *) args.value)->cdr->car;
    return eq_internal(a, b);
}

value_t quote(value_t args, value_t env)
{
    if (len(args, 0) != 1) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity to quote."};
    }
    return ((cell_t *) args.value)->car;
}

value_t lookup(value_t s, value_t env)
{
    if (env.type == NIL) {
	return (value_t) {
	ERROR, (chunk_t) "Nil environment."};
    }
    value_t binding = ((cell_t *) env.value)->car;
    value_t first = ((cell_t *) binding.value)->car;
    if (eq_internal(first, s).type != NIL) {
	return ((cell_t *) binding.value)->cdr->car;
    }
    cell_t *cdr = ((cell_t *) env.value)->cdr;
    if (cdr == 0) {
	return (value_t) {
	ERROR, (chunk_t) "Lookup of symbol failed."};
    }
    return lookup(s, (value_t) {
		  LIST, (chunk_t) cdr}
    );
}

value_t bind(value_t name, value_t value, value_t env)
{
    if (name.type != SYMBOL) {
	return (value_t) {
	ERROR, (chunk_t) "Attempt to bind non-symbol."};
    }
    cell_t *first = malloc(sizeof(cell_t));
    cell_t *second = malloc(sizeof(cell_t));
    first->car = name;
    first->cdr = second;
    second->car = value;
    second->cdr = 0;
    cell_t *new_env = malloc(sizeof(cell_t));
    new_env->car = (value_t) {
    LIST, (chunk_t) first};
    if (env.type == LIST) {
	new_env->cdr = (cell_t *) env.value;
    } else {
	new_env->cdr = 0;
    }
    return (value_t) {
    LIST, (chunk_t) new_env};
}

value_t eval_args(value_t list, value_t env, int limit)
{
    if (list.type == NIL) {
	return list;
    }
    cell_t *cell = malloc(sizeof(cell_t));
    value_t car = eval(((cell_t *) list.value)->car, env);
    cell->car = car;
    cell_t *cdr = ((cell_t *) list.value)->cdr;
    cell->cdr = cdr;
    if (cdr != 0 && limit != 0) {
	value_t l = eval_args((value_t) { LIST, (chunk_t) cdr }
			      , env, limit - 1);
	cell->cdr = (cell_t *) l.value;
    }
    return (value_t) {
    LIST, (chunk_t) cell};
}

value_t expand(value_t macro, value_t params, value_t form)
{
    if (len(params, 0) == 0) {
	return form;
    }
    if (form.type != LIST && form.type != SYMBOL) {
	return form;
    }
    macro_t *mac = (macro_t *) macro.value;
    int names_len = len(mac->names, 0);
    int params_len = len(params, 0);
    if (names_len > params_len + 1) {
	return (value_t) {
	ERROR, (chunk_t) "Not enough arguments provided to macro."};
    }
    // Symbols are replaced
    if (form.type == SYMBOL) {
	cell_t *name = (cell_t *) mac->names.value;
	cell_t *param = (cell_t *) params.value;
	while (name != 0) {
	    if (name->car.type == SYMBOL && name->car.value == form.value) {
		form = param->car;
		break;
	    }
	    name = name->cdr;
	    param = param->cdr;
	}
    }
    // Lists are recursively expanded
    if (form.type == LIST) {
	cell_t *form_list = (cell_t *) form.value;
	cell_t *head = malloc(sizeof(cell_t));
	cell_t *tail = head;
	tail->car = expand(macro, params, form_list->car);
	form_list = form_list->cdr;
	while (form_list != 0) {
	    tail->cdr = malloc(sizeof(cell_t));
	    tail = tail->cdr;
	    tail->car = expand(macro, params, form_list->car);
	    tail->cdr = 0;
	    form_list = form_list->cdr;
	}
	form = (value_t) {
	LIST, (chunk_t) head};
    }
    return form;
}

value_t eval(value_t v, value_t env)
{
    switch (v.type) {
    case NIL:
    case NUMBER:
    case ERROR:
    case FUNC:
    case LAMBDA:
    case MACRO:
	return v;
    case ENV:
	{
	    env_t *e = (env_t *) v.value;
	    return eval(e->value, e->env);
	}
    case SYMBOL:
	return lookup(v, env);
    case LIST:
	{
	    value_t first = ((cell_t *) v.value)->car;
	    if (first.type == NIL || first.type == NUMBER
		|| first.type == ERROR) {
		return eval_args(v, env, -1);
	    }
	    value_t params = (value_t) { NIL, 0 };
	    cell_t *cdr = ((cell_t *) v.value)->cdr;
	    if (cdr != 0) {
		params = (value_t) {
		LIST, (chunk_t) cdr};
	    }
	    if (first.type != FUNC) {
		first = eval(first, env);
	    }
	    switch (first.type) {
	    case ERROR:
		return first;
	    case FUNC:
		{
		    func_t *func = (func_t *) first.value;
		    return (*(func->func)) (params, env);
		}
	    case MACRO:
		{
		    macro_t *mac = (macro_t *) first.value;
		    // Wrap parameters with their environment
		    // And group extra parameters into the last named symbol
		    cell_t *param_list = (cell_t *) params.value;
		    int len_names = len(mac->names, 0);
		    int len_params = len(params, 0);
		    if (len_names > len_params + 1) {
			return (value_t) {
			    ERROR, (chunk_t)
			"Not enough arguments provided to macro."};
		    }
		    // new list
		    cell_t *head = malloc(sizeof(cell_t));
		    cell_t *tail = head;
		    env_t *e = malloc(sizeof(env_t));
		    // wrap the first element
		    if (len_names == 1) {
			e->value = params;
		    } else {
			e->value = param_list->car;
		    }
		    e->env = env;
		    tail->car = (value_t) {
		    ENV, (chunk_t) e};
		    tail->cdr = 0;
		    param_list = param_list->cdr;
		    // wrap all the rest of the names bindings
		    // except the last one
		    for (int i = 1; i < len_names - 1; i++) {
			tail->cdr = malloc(sizeof(cell_t));
			tail = tail->cdr;
			env_t *e = malloc(sizeof(env_t));
			e->value = param_list->car;
			e->env = env;
			tail->car = (value_t) {
			ENV, (chunk_t) e};
			tail->cdr = 0;
			param_list = param_list->cdr;
		    }
		    // wrap the remaining params as the last name
		    tail->cdr = malloc(sizeof(cell_t));
		    tail = tail->cdr;
		    e = malloc(sizeof(env_t));

		    e->value = (value_t) {
		    LIST, (chunk_t) param_list};
		    e->env = env;
		    tail->car = (value_t) {
		    ENV, (chunk_t) e};
		    tail->cdr = 0;
		    // Expand the macro
		    value_t result =
			expand(first, (value_t) { LIST, (chunk_t) head }
			       , mac->form);
		    // Evaluate with the environment of the macro
		    return eval(result, mac->env);
		}
	    case LAMBDA:
		{
		    lambda_t *lamb = (lambda_t *) first.value;
		    while (1) {
			if (len(params, 0) != len(lamb->names, 0)) {
			    return (value_t) {
			    ERROR, 0};
			}
			params = eval_args(params, env, -1);
			value_t lambda_env = lamb->env;
			cell_t *name = (cell_t *) lamb->names.value;
			cell_t *param = (cell_t *) params.value;
			while (name != 0) {
			    if (param->car.type == ERROR) {
				return param->car;
			    }
			    lambda_env =
				bind(name->car, param->car, lambda_env);
			    name = name->cdr;
			    param = param->cdr;
			}
			value_t result = eval(lamb->form, lambda_env);
			if (result.type != RECUR) {
			    return result;
			}
			params = ((recur_t *) result.value)->params;
			env = ((recur_t *) result.value)->env;
		    }
		}
	    default:
		return (value_t) {
		ERROR, (chunk_t) "Attempt to call non-function."};
	    }
	}
    }
}

value_t wrap_fn(func_fn fn)
{
    func_t *func = malloc(sizeof(func_t));
    func->func = fn;
    return (value_t) {
    FUNC, (chunk_t) func};
}

value_t label(value_t args, value_t env)
{
    if (len(args, 0) != 3) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for label."};
    }
    value_t name = ((cell_t *) args.value)->car;
    value_t value = ((cell_t *) args.value)->cdr->car;
    value = eval(value, env);
    env = bind(name, value, env);
    value_t form = ((cell_t *) args.value)->cdr->cdr->car;
    return eval(form, env);
}

value_t lambda(value_t args, value_t env)
{
    if (len(args, 0) != 2) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for lambda."};
    }
    value_t names = ((cell_t *) args.value)->car;
    if (names.type != LIST && names.type != NIL) {
	return (value_t) {
	ERROR, (chunk_t) "First argument to lambda is non-list."};
    }
    value_t form = ((cell_t *) args.value)->cdr->car;
    lambda_t *lamb = malloc(sizeof(lambda_t));
    lamb->names = names;
    lamb->form = form;
    lamb->env = env;
    return (value_t) {
    LAMBDA, (chunk_t) lamb};
}

value_t recur(value_t args, value_t env)
{
    args = eval_args(args, env, -1);
    if (args.type == ERROR) {
	return args;
    }
    recur_t *r = malloc(sizeof(recur_t));
    r->params = args;
    r->env = env;
    return (value_t) {
    RECUR, (chunk_t) r};
}

value_t macro(value_t args, value_t env)
{
    if (len(args, 0) != 2) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for macro."};
    }
    value_t names = ((cell_t *) args.value)->car;
    if (names.type != LIST && names.type != NIL) {
	return (value_t) {
	ERROR, (chunk_t) "First argument to macro is non-list."};
    }
    value_t form = ((cell_t *) args.value)->cdr->car;
    macro_t *mac = malloc(sizeof(macro_t));
    mac->names = names;
    mac->form = form;
    mac->env = env;
    return (value_t) {
    MACRO, (chunk_t) mac};
}

value_t to_string(value_t v, char *str)
{
    if (v.type != LIST) {
	return (value_t) {
	ERROR, (chunk_t) "to_string requires a list of symbols."};
    }
    int size = len(v, 0) + 1;
    cell_t *head = (cell_t *) v.value;
    for (int i = 0; i < size - 1; i++) {
	if (head->car.type != SYMBOL) {
	    return (value_t) {
	    ERROR, (chunk_t) "Non SYMBOL in to_string list"};
	}
	str[i] = (char) head->car.value;
	head = head->cdr;
    }
    str[size - 1] = 0;
    return (value_t) {
    NIL, 0};
}

value_t load(value_t filename)
{
    char *str = malloc(len(filename, 0) + 1);
    value_t err = to_string(filename, str);
    if (err.type != NIL) {
	return err;
    }
    FILE *fp;
    if ((fp = fopen(str, "r")) == NULL) {
	return (value_t) {
	ERROR, (chunk_t) "Error opening file"};
    }
    value_t form = read(fp);
    fclose(fp);
    free(str);
    return form;
}

value_t import(value_t args, value_t env)
{
    if (len(args, 0) != 2) {
	return (value_t) {
	ERROR, (chunk_t) "Wrong arity for import."};
    }
    value_t first = ((cell_t *) args.value)->car;
    if (first.type != LIST && first.type != NIL) {
	return (value_t) {
	ERROR, (chunk_t) "First argument to import is non-list."};
    }
    cell_t *import_list = (cell_t *) first.value;
    while (import_list != 0) {
	value_t import_env = load(import_list->car);
	import_env = eval(import_env, env);
	if (import_env.type == ERROR) {
	    return import_env;
	}
	if (import_env.type != LIST) {
	    return (value_t) {
	    ERROR, (chunk_t) "Invalid import. Non-List."};
	}
	cell_t *binding = (cell_t *) import_env.value;
	while (binding != 0) {
	    if (len(binding->car, 0) != 2) {
		return (value_t) {
		ERROR, (chunk_t) "Invalid import. Non pair binding."};
	    }
	    value_t sym = ((cell_t *) binding->car.value)->car;
	    value_t val = ((cell_t *) binding->car.value)->cdr->car;
	    if (sym.type != SYMBOL) {
		return (value_t) {
		    ERROR, (chunk_t)
		"Invalid import. First of pair not symbol."};
	    }
	    env = bind(sym, val, env);
	    binding = binding->cdr;
	}
	import_list = import_list->cdr;
    }
    return eval(((cell_t *) args.value)->cdr->car, env);
}

value_t callow_core()
{
    value_t env = (value_t) { NIL, 0 };
    // The magnificent seven.
    env = bind(read_string("atom"), wrap_fn(&atom), env);
    env = bind(read_string("car"), wrap_fn(&car), env);
    env = bind(read_string("cdr"), wrap_fn(&cdr), env);
    env = bind(read_string("cond"), wrap_fn(&cond), env);
    env = bind(read_string("cons"), wrap_fn(&cons), env);
    env = bind(read_string("eq"), wrap_fn(&eq), env);
    env = bind(read_string("quote"), wrap_fn(&quote), env);
    // Two special forms.
    env = bind(read_string("label"), wrap_fn(&label), env);
    env = bind(read_string("lambda"), wrap_fn(&lambda), env);
    // And some stuff I think is essential.
    env = bind(read_string("recur"), wrap_fn(&recur), env);
    env = bind(read_string("macro"), wrap_fn(&macro), env);
    env = bind(read_string("import"), wrap_fn(&import), env);
    return env;
}
