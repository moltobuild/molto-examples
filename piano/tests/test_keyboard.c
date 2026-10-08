#include <moltest.h>

#include <piano/keyboard.h>

#include <math.h>

static piano_keyboard laid_out(void) {
    piano_keyboard kb;
    piano_keyboard_layout(&kb, 1500, 300); /* 15 white keys, 100 wide each */
    return kb;
}

DESCRIBE(the_keyboard_runs_from_c4_to_c6_with_fifteen_white_keys) {
    piano_keyboard kb = laid_out();
    EXPECT_EQ(60, kb.keys[0].note);
    EXPECT_EQ(84, kb.keys[PIANO_KEY_COUNT - 1].note);
    int whites = 0;
    for (int i = 0; i < PIANO_KEY_COUNT; i++)
        whites += !kb.keys[i].black;
    EXPECT_EQ(15, whites);
    EXPECT_FALSE(kb.keys[0].black);  /* C4 */
    EXPECT_TRUE(kb.keys[1].black);   /* C#4 */
    EXPECT_FALSE(kb.keys[4].black);  /* E4 */
    EXPECT_FALSE(kb.keys[5].black);  /* F4: no black key between E and F */
}

DESCRIBE(a_click_low_on_a_white_key_plays_it) {
    piano_keyboard kb = laid_out();
    EXPECT_EQ(0, piano_keyboard_hit(&kb, 50, 280));   /* C4 */
    EXPECT_EQ(2, piano_keyboard_hit(&kb, 150, 280));  /* D4 */
    EXPECT_EQ(24, piano_keyboard_hit(&kb, 1450, 280)); /* C6 */
}

DESCRIBE(a_black_key_wins_where_it_covers_two_white_keys) {
    piano_keyboard kb = laid_out();
    /* C#4 straddles the C4 and D4 boundary at x = 100. */
    EXPECT_EQ(1, piano_keyboard_hit(&kb, 95, 50));
    EXPECT_EQ(1, piano_keyboard_hit(&kb, 105, 50));
    /* Below the black key the white ones show again. */
    EXPECT_EQ(0, piano_keyboard_hit(&kb, 95, 250));
    EXPECT_EQ(2, piano_keyboard_hit(&kb, 105, 250));
    /* Between E4 and F4 there is no black key. */
    EXPECT_EQ(4, piano_keyboard_hit(&kb, 295, 50));
    EXPECT_EQ(5, piano_keyboard_hit(&kb, 305, 50));
}

DESCRIBE(a_point_off_the_keyboard_plays_nothing) {
    piano_keyboard kb = laid_out();
    EXPECT_EQ(-1, piano_keyboard_hit(&kb, -1, 100));
    EXPECT_EQ(-1, piano_keyboard_hit(&kb, 1500, 100));
    EXPECT_EQ(-1, piano_keyboard_hit(&kb, 100, 300));
}

DESCRIBE(notes_follow_equal_temperament_from_a440) {
    EXPECT_TRUE(fabs(piano_note_frequency(69) - 440.0) < 1e-9);
    EXPECT_TRUE(fabs(piano_note_frequency(81) - 880.0) < 1e-9);
    EXPECT_TRUE(fabs(piano_note_frequency(60) - 261.6256) < 1e-3);
}

DESCRIBE(notes_are_named_with_their_octave) {
    char name[8];
    piano_note_name(60, name, sizeof name);
    EXPECT_STREQ("C4", name);
    piano_note_name(78, name, sizeof name);
    EXPECT_STREQ("F#5", name);
}

DESCRIBE(the_home_row_plays_white_keys_and_the_row_above_black_ones) {
    EXPECT_EQ(0, piano_key_for_char('a'));  /* C4 */
    EXPECT_EQ(1, piano_key_for_char('w'));  /* C#4 */
    EXPECT_EQ(2, piano_key_for_char('S'));  /* D4, either case */
    EXPECT_EQ(12, piano_key_for_char('k')); /* C5 */
    EXPECT_EQ(16, piano_key_for_char(';')); /* E5 */
    EXPECT_EQ(-1, piano_key_for_char('q'));
    EXPECT_EQ(-1, piano_key_for_char('1'));
}
