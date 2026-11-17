// Mod (boot): no company logos and no opening movie: the game starts at its title screen (the
// TEKKEN 6 logo, PRESS START BUTTON).
//
// The game runs one mode at a time; FUN_08869e00 (once a frame, a0 = the mode manager) starts the
// mode asked for in manager+0x80 (0x26: none), the current one being manager+0x7c. At boot it goes
// 0 (init) -> 18 (logos, tcb_logomode_t) -> 22 (the opening movie, then the title,
// tcb_moviemode_t) -> 13 (the main menu: it loads the system save first, or makes a new profile).
// The first ask for 18 becomes 22; the movie that mode starts first (opening.pmf) plays black and
// silent, and START is pressed for the player while it does (a movie that ends by itself leads to
// the demo fights, one skipped with START to the title). T6_MOD_SKIP_INTRO=0 keeps the original
// sequence; T6_BOOT_LOG=1 logs every mode change.
#include <stdlib.h>

#include "funcs.h"
#include "rt.h"

#define MODE_NONE 0x26
#define MODE_LOGOS 18
#define MODE_MOVIE_TITLE 22
#define BUTTON_START 0x0008

void profile_name_frame(void);

static bool enabled(void) {
	const char *e = getenv("T6_MOD_SKIP_INTRO");
	return !e || atoi(e) != 0;
}

void f_08869e00(CPU *c) {
	static bool done, skipping;
	static int log = -1, wait;
	if (log < 0) log = getenv("T6_BOOT_LOG") && atoi(getenv("T6_BOOT_LOG")) != 0;
	uint32_t asked = A0 + 0x80;
	if (!done && M32(asked) != MODE_NONE && M32(asked) != 0) {
		done = true;
		if (M32(asked) == MODE_LOGOS && enabled()) {
			M32(asked) = MODE_MOVIE_TITLE;
			rt_psmf_hide_next();
			skipping = true;
			rt_log("mod: logos and opening movie skipped");
		}
	}
	if (skipping && rt_psmf_hidden_playing() && wait-- <= 0) {  // START, held 6 vblanks, every 10
		rt_tap_buttons(BUTTON_START);
		wait = 10;
	}
	if (skipping && M32(asked) != MODE_NONE && M32(asked) != MODE_MOVIE_TITLE) skipping = false;
	if (log && M32(asked) != MODE_NONE) rt_log("mode %u -> %u", M32(A0 + 0x7c), M32(asked));
	rt_scene = M32(A0 + 0x7c);  // the game's mode (the texture catalog says where it saw a texture)
	profile_name_frame();  // (profile.c)
	f_08869e00_orig(c);
}
