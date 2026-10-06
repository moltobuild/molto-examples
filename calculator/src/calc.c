#include <calculator/calc.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- evaluating --- */

/* A recursive-descent parser over the expression, in the grammar a person
   expects:

     expr    = term (('+' | '-') term)*
     term    = unary (('*' | '/') unary)*
     unary   = '-' unary | postfix
     postfix = primary '%'*
     primary = number | '(' expr ')'
*/
typedef struct {
    const char *at;
    calc_error error;
} parser;

static double parse_expr(parser *p);

static bool failed(const parser *p) { return p->error != calc_ok; }

static double fail(parser *p, calc_error error) {
    if(p->error == calc_ok)
        p->error = error;
    return 0.0;
}

static double parse_number(parser *p) {
    char *end = NULL;
    const double value = strtod(p->at, &end);
    if(end == p->at)
        return fail(p, calc_error_syntax);
    p->at = end;
    return value;
}

static double parse_primary(parser *p) {
    if(*p->at == '(') {
        p->at++;
        const double value = parse_expr(p);
        if(failed(p))
            return 0.0;
        if(*p->at != ')')
            return fail(p, calc_error_syntax);
        p->at++;
        return value;
    }
    /* strtod would also take "inf", "nan" and a sign; none of them is a key. */
    if(!isdigit((unsigned char)*p->at) && *p->at != '.')
        return fail(p, calc_error_syntax);
    return parse_number(p);
}

static double parse_postfix(parser *p) {
    double value = parse_primary(p);
    while(!failed(p) && *p->at == '%') {
        p->at++;
        value /= 100.0;
    }
    return value;
}

static double parse_unary(parser *p) {
    if(*p->at == '-') {
        p->at++;
        return -parse_unary(p);
    }
    return parse_postfix(p);
}

static double parse_term(parser *p) {
    double value = parse_unary(p);
    while(!failed(p) && (*p->at == '*' || *p->at == '/')) {
        const char op = *p->at++;
        const double right = parse_unary(p);
        if(failed(p))
            return 0.0;
        if(op == '/' && right == 0.0)
            return fail(p, calc_error_division_by_zero);
        value = op == '*' ? value * right : value / right;
    }
    return value;
}

static double parse_expr(parser *p) {
    double value = parse_term(p);
    while(!failed(p) && (*p->at == '+' || *p->at == '-')) {
        const char op = *p->at++;
        const double right = parse_term(p);
        value = op == '+' ? value + right : value - right;
    }
    return value;
}

calc_error calc_evaluate(const char *expr, double *out) {
    parser p = {.at = expr, .error = calc_ok};
    const double value = parse_expr(&p);
    if(!failed(&p) && *p.at != '\0')
        (void)fail(&p, calc_error_syntax);
    if(!failed(&p) && !isfinite(value))
        (void)fail(&p, calc_error_overflow);
    if(!failed(&p))
        *out = value;
    return p.error;
}

void calc_format(double value, char *out, size_t size) {
    if(value == 0.0)
        value = 0.0; /* -0 is not a number anyone types */
    snprintf(out, size, "%.12g", value);
}

const char *calc_error_message(calc_error error) {
    switch(error) {
    case calc_ok:
        return "";
    case calc_error_syntax:
        return "Incomplete expression";
    case calc_error_division_by_zero:
        return "Cannot divide by zero";
    case calc_error_overflow:
        return "Result too large";
    }
    return "Error";
}

void calc_display(const char *expr, char *out, size_t size) {
    size_t used = 0;
    for(const char *at = expr; *at != '\0'; at++) {
        const char *piece = *at == '*' ? "×" : *at == '/' ? "÷" : *at == '-' ? "−" : NULL;
        char single[2] = {*at, '\0'};
        const char *text = piece != NULL ? piece : single;
        const size_t length = strlen(text);
        if(used + length + 1 > size)
            break;
        memcpy(out + used, text, length);
        used += length;
    }
    if(size > 0)
        out[used] = '\0';
}

/* --- typing --- */

static bool is_operator(char c) { return c == '+' || c == '-' || c == '*' || c == '/'; }

static char last(const calc_state *state) {
    return state->length == 0 ? '\0' : state->expr[state->length - 1];
}

/* Parentheses opened and not yet closed. */
static int open_count(const calc_state *state) {
    int open = 0;
    for(size_t i = 0; i < state->length; i++)
        open += state->expr[i] == '(' ? 1 : state->expr[i] == ')' ? -1 : 0;
    return open;
}

/* Whether the number being typed already has a decimal point. */
static bool number_has_point(const calc_state *state) {
    for(size_t i = state->length; i > 0; i--) {
        const char c = state->expr[i - 1];
        if(c == '.')
            return true;
        if(!isdigit((unsigned char)c))
            return false;
    }
    return false;
}

/* True after something a value can end with: a digit, '.', ')' or '%'. */
static bool ends_a_value(char c) {
    return isdigit((unsigned char)c) || c == '.' || c == ')' || c == '%';
}

/* True when the number being typed is exactly "0": a zero with no digit and
   no point before it. */
static bool ends_in_a_lone_zero(const calc_state *state) {
    if(last(state) != '0')
        return false;
    if(state->length == 1)
        return true;
    const char previous = state->expr[state->length - 2];
    return !isdigit((unsigned char)previous) && previous != '.';
}

static bool append(calc_state *state, char c) {
    if(state->length + 1 >= sizeof state->expr)
        return false;
    state->expr[state->length++] = c;
    state->expr[state->length] = '\0';
    return true;
}

void calc_init(calc_state *state) {
    memset(state, 0, sizeof *state);
    snprintf(state->result, sizeof state->result, "0");
}

void calc_clear(calc_state *state) { calc_init(state); }

/* After `=`, a digit, '.' or '(' starts a new expression; anything else
   continues from the result, which is already the expression. */
static void start_over_if_needed(calc_state *state, char key) {
    if(!state->evaluated)
        return;
    state->evaluated = false;
    if(isdigit((unsigned char)key) || key == '.' || key == '(') {
        state->expr[0] = '\0';
        state->length = 0;
    }
}

bool calc_press(calc_state *state, char key) {
    if(!isdigit((unsigned char)key) && key != '.' && key != '%' && key != '(' && key != ')' &&
       !is_operator(key))
        return false;
    start_over_if_needed(state, key);
    state->error = calc_ok;
    const char before = last(state);

    if(isdigit((unsigned char)key)) {
        /* A digit straight after ')' or '%' would be a second number with no
           operator between them. */
        if(before == ')' || before == '%')
            return false;
        /* "007" is 7: a lone leading zero is replaced, not extended. */
        if(ends_in_a_lone_zero(state)) {
            state->expr[state->length - 1] = key;
            return true;
        }
        return append(state, key);
    }

    if(key == '.') {
        if(number_has_point(state) || before == ')' || before == '%')
            return false;
        /* ".5" reads as "0.5" on the display. */
        if(!isdigit((unsigned char)before))
            if(!append(state, '0'))
                return false;
        return append(state, '.');
    }

    if(key == '(') {
        if(ends_a_value(before))
            return false;
        return append(state, '(');
    }

    if(key == ')') {
        if(open_count(state) <= 0 || !ends_a_value(before))
            return false;
        return append(state, ')');
    }

    if(key == '%') {
        if(!ends_a_value(before) || before == '.')
            return false;
        return append(state, '%');
    }

    /* An operator. A leading or bracketed '-' is a sign; anything else needs a
       value before it, and replaces an operator typed just before. */
    if(before == '\0' || before == '(')
        return key == '-' && append(state, key);
    if(is_operator(before)) {
        /* "(-" cannot become "(*": the sign is the only thing there. */
        if(state->length == 1 || state->expr[state->length - 2] == '(')
            return false;
        state->expr[state->length - 1] = key;
        return true;
    }
    if(before == '.')
        return false;
    return append(state, key);
}

void calc_backspace(calc_state *state) {
    if(state->evaluated) {
        calc_clear(state);
        return;
    }
    state->error = calc_ok;
    if(state->length > 0)
        state->expr[--state->length] = '\0';
}

/* The expression with every open parenthesis closed, which is what `=`
   evaluates: "2*(3+4" means "2*(3+4)". */
static void closed_copy(const calc_state *state, char *out, size_t size) {
    snprintf(out, size, "%s", state->expr);
    size_t used = strlen(out);
    for(int open = open_count(state); open > 0 && used + 1 < size; open--) {
        out[used++] = ')';
        out[used] = '\0';
    }
}

bool calc_equals(calc_state *state) {
    if(state->length == 0)
        return false;
    char closed[CALC_EXPR_MAX + 16];
    closed_copy(state, closed, sizeof closed);
    double value = 0.0;
    state->error = calc_evaluate(closed, &value);
    if(state->error != calc_ok)
        return false;

    calc_format(value, state->result, sizeof state->result);
    snprintf(state->evaluated_expr, sizeof state->evaluated_expr, "%s", closed);
    /* The result is the new expression, so the next operator continues it. */
    snprintf(state->expr, sizeof state->expr, "%s", state->result);
    state->length = strlen(state->expr);
    state->evaluated = true;
    return true;
}

bool calc_preview(const calc_state *state, char *out, size_t size) {
    if(state->length == 0)
        return false;
    char closed[CALC_EXPR_MAX + 16];
    closed_copy(state, closed, sizeof closed);
    double value = 0.0;
    if(calc_evaluate(closed, &value) != calc_ok)
        return false;
    calc_format(value, out, size);
    return true;
}
