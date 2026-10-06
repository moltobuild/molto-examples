#include <moltest.h>

#include <calculator/calc.h>

#include <string.h>

/* The calculator without a window: what each key does, and what `=` answers. */

/* Press every character of `keys`, then `=`; the result as it is shown. */
static const char *type_and_equal(calc_state *state, const char *keys) {
    calc_init(state);
    for(const char *at = keys; *at != '\0'; at++)
        (void)calc_press(state, *at);
    (void)calc_equals(state);
    return state->result;
}

static const char *evaluated(const char *expr) {
    static char text[CALC_TEXT_MAX];
    double value = 0.0;
    if(calc_evaluate(expr, &value) != calc_ok)
        return "error";
    calc_format(value, text, sizeof text);
    return text;
}

/* --- evaluating --- */

DESCRIBE(multiplication_binds_tighter_than_addition) {
    EXPECT_STREQ("14", evaluated("2+3*4"));
    EXPECT_STREQ("20", evaluated("(2+3)*4"));
    EXPECT_STREQ("2", evaluated("8/2/2"));
    EXPECT_STREQ("3", evaluated("10-4-3"));
}

DESCRIBE(a_minus_sign_negates_what_follows) {
    EXPECT_STREQ("-3", evaluated("-3"));
    EXPECT_STREQ("6", evaluated("-2*-3"));
    EXPECT_STREQ("-5", evaluated("-(2+3)"));
}

DESCRIBE(percent_divides_by_a_hundred) {
    EXPECT_STREQ("0.5", evaluated("50%"));
    EXPECT_STREQ("20", evaluated("200*10%"));
}

DESCRIBE(decimals_are_shown_without_float_noise) {
    EXPECT_STREQ("0.3", evaluated("0.1+0.2"));
    EXPECT_STREQ("0.333333333333", evaluated("1/3"));
    EXPECT_STREQ("0", evaluated("0.1-0.1"));
    EXPECT_STREQ("0", evaluated("-0"));
}

DESCRIBE(what_cannot_be_evaluated_says_why) {
    double value = 0.0;
    EXPECT_EQ(calc_error_division_by_zero, calc_evaluate("1/0", &value));
    EXPECT_EQ(calc_error_division_by_zero, calc_evaluate("5/(2-2)", &value));
    EXPECT_EQ(calc_error_syntax, calc_evaluate("3+", &value));
    EXPECT_EQ(calc_error_syntax, calc_evaluate("(2", &value));
    EXPECT_EQ(calc_error_syntax, calc_evaluate("*4", &value));
    EXPECT_EQ(calc_error_syntax, calc_evaluate("inf", &value));
    EXPECT_EQ(calc_error_overflow, calc_evaluate("1e308*10", &value));
}

/* --- typing --- */

DESCRIBE(equals_evaluates_what_was_typed) {
    calc_state state;
    EXPECT_STREQ("14", type_and_equal(&state, "2+3*4"));
    EXPECT_STREQ("2.5", type_and_equal(&state, "5/2"));
}

DESCRIBE(equals_closes_the_parentheses_left_open) {
    calc_state state;
    EXPECT_STREQ("14", type_and_equal(&state, "2*(3+4"));
    /* And remembers what it evaluated, closed, for the line above. */
    EXPECT_STREQ("2*(3+4)", state.evaluated_expr);
}

DESCRIBE(a_second_operator_replaces_the_first) {
    calc_state state;
    EXPECT_STREQ("6", type_and_equal(&state, "3+*2"));
}

DESCRIBE(a_number_has_one_decimal_point) {
    calc_state state;
    calc_init(&state);
    EXPECT_TRUE(calc_press(&state, '1'));
    EXPECT_TRUE(calc_press(&state, '.'));
    EXPECT_FALSE(calc_press(&state, '.'));
    EXPECT_TRUE(calc_press(&state, '5'));
    EXPECT_STREQ("1.5", state.expr);
}

DESCRIBE(a_point_with_no_digit_before_it_reads_as_zero_point) {
    calc_state state;
    calc_init(&state);
    EXPECT_TRUE(calc_press(&state, '.'));
    EXPECT_TRUE(calc_press(&state, '5'));
    EXPECT_STREQ("0.5", state.expr);
}

DESCRIBE(a_leading_zero_is_replaced_not_extended) {
    calc_state state;
    calc_init(&state);
    (void)calc_press(&state, '0');
    (void)calc_press(&state, '0');
    (void)calc_press(&state, '7');
    EXPECT_STREQ("7", state.expr);
    (void)calc_press(&state, '+');
    (void)calc_press(&state, '0');
    (void)calc_press(&state, '3');
    EXPECT_STREQ("7+3", state.expr);
    calc_init(&state);
    (void)calc_press(&state, '1');
    (void)calc_press(&state, '0');
    (void)calc_press(&state, '0');
    EXPECT_STREQ("100", state.expr);
}

DESCRIBE(only_a_minus_may_start_an_expression) {
    calc_state state;
    calc_init(&state);
    EXPECT_FALSE(calc_press(&state, '*'));
    EXPECT_FALSE(calc_press(&state, '+'));
    EXPECT_TRUE(calc_press(&state, '-'));
    EXPECT_FALSE(calc_press(&state, '*'));
    EXPECT_STREQ("-", state.expr);
}

DESCRIBE(a_closing_parenthesis_needs_one_open) {
    calc_state state;
    calc_init(&state);
    (void)calc_press(&state, '2');
    EXPECT_FALSE(calc_press(&state, ')'));
    EXPECT_FALSE(calc_press(&state, '('));
}

DESCRIBE(after_equals_a_digit_starts_over_and_an_operator_continues) {
    calc_state state;
    EXPECT_STREQ("5", type_and_equal(&state, "2+3"));
    (void)calc_press(&state, '*');
    (void)calc_press(&state, '2');
    (void)calc_equals(&state);
    EXPECT_STREQ("10", state.result);

    (void)calc_press(&state, '7');
    EXPECT_STREQ("7", state.expr);
}

DESCRIBE(a_failed_equals_keeps_the_expression_and_says_why) {
    calc_state state;
    calc_init(&state);
    (void)calc_press(&state, '1');
    (void)calc_press(&state, '/');
    (void)calc_press(&state, '0');
    EXPECT_FALSE(calc_equals(&state));
    EXPECT_EQ(calc_error_division_by_zero, state.error);
    EXPECT_STREQ("1/0", state.expr);
    /* The next key clears the error. */
    calc_backspace(&state);
    EXPECT_EQ(calc_ok, state.error);
    EXPECT_STREQ("1/", state.expr);
}

DESCRIBE(the_preview_follows_what_is_typed) {
    calc_state state;
    calc_init(&state);
    char preview[CALC_TEXT_MAX] = "";
    (void)calc_press(&state, '2');
    (void)calc_press(&state, '+');
    EXPECT_FALSE(calc_preview(&state, preview, sizeof preview));
    (void)calc_press(&state, '3');
    EXPECT_TRUE(calc_preview(&state, preview, sizeof preview));
    EXPECT_STREQ("5", preview);
}

DESCRIBE(clear_and_backspace) {
    calc_state state;
    calc_init(&state);
    (void)calc_press(&state, '1');
    (void)calc_press(&state, '2');
    calc_backspace(&state);
    EXPECT_STREQ("1", state.expr);
    calc_clear(&state);
    EXPECT_STREQ("", state.expr);
    EXPECT_STREQ("0", state.result);
}

DESCRIBE(the_display_prints_the_operators_as_the_buttons_do) {
    char shown[64];
    calc_display("6*2-8/4", shown, sizeof shown);
    EXPECT_STREQ("6×2−8÷4", shown);
}
