#ifndef CALCULATOR_CALC_H
#define CALCULATOR_CALC_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The calculator, without a window.
 *
 * What a person types is kept as an expression — "12+3×(4−1)" — and evaluated
 * with the precedence they were taught, rather than one operator at a time the
 * way a pocket calculator does. Every rule about what may be typed next lives
 * here, so the interface only forwards keys and shows two strings.
 *
 * Internally the operators are ASCII (+ - * /); `calc_display` spells them the
 * way they are printed on the buttons.
 */

#define CALC_EXPR_MAX 128
#define CALC_TEXT_MAX 64

typedef enum {
    calc_ok,
    calc_error_syntax,           /* "3+", "(", "*4" */
    calc_error_division_by_zero, /* "1/0", "5/(2-2)" */
    calc_error_overflow,         /* a result too large to be a number */
} calc_error;

typedef struct {
    char expr[CALC_EXPR_MAX];
    size_t length;
    /* The last result, shown under the expression; "0" before there is one. */
    char result[CALC_TEXT_MAX];
    /* Set by `calc_equals`: the next digit starts over, the next operator
       continues from the result. */
    bool evaluated;
    /* What `=` evaluated, parentheses closed, for the line above the result
       while it is shown. Empty until there is one. */
    char evaluated_expr[CALC_EXPR_MAX + 16];
    calc_error error;
} calc_state;

void calc_init(calc_state *state);

/* One key: a digit, '.', one of "+-*%/", '(' or ')'. False when the key is not
   one of those, or when it cannot follow what was typed: a second '.' in a
   number, a ')' with nothing open, an operator at the start (other than '-').
   An operator typed after another replaces it. */
bool calc_press(calc_state *state, char key);

/* Remove the last character typed; after `=`, clear instead. */
void calc_backspace(calc_state *state);

/* Back to an empty expression and a result of "0". */
void calc_clear(calc_state *state);

/* Evaluate what was typed, closing any parenthesis left open. On success the
   result becomes the expression, so `=` then `+2` keeps going. False, with
   `state->error` set and the expression untouched, when it cannot be
   evaluated. */
bool calc_equals(calc_state *state);

/* What the expression evaluates to so far, for a preview under it. False, with
   nothing written, while it is incomplete or invalid. */
bool calc_preview(const calc_state *state, char *out, size_t size);

/* Evaluate `expr`. `%` divides the number before it by a hundred. */
calc_error calc_evaluate(const char *expr, double *out);

/* `value` as a calculator shows it: up to twelve significant digits, no
   trailing zeros, never "-0". */
void calc_format(double value, char *out, size_t size);

/* `expr` with the operators printed as the buttons print them: × ÷ −. */
void calc_display(const char *expr, char *out, size_t size);

/* A sentence for an error, for the line where the result would be. */
const char *calc_error_message(calc_error error);

#endif /* CALCULATOR_CALC_H */
