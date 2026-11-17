// Mod (menus): the player's name comes from the site's account, so the menus lose what would
// change it or work around the site's saves:
//   PROFILE: SELECT MAIN CHARACTER first, in place of PLAYER SETTINGS (whose other item was
//            EDIT PLAYER NAME); CUSTOMIZE, RECORDS and GHOST LIST as they were.
//   OPTIONS: GAME OPTIONS, SOUND OPTIONS, CONTROL SETUP; no SAVE / LOAD (the site keeps the
//            saves) and no INSTALL (nothing to install).
//
// The menus are a tree in the executable's data (0x08be5b80, set up by FUN_089e9958 beside the
// labels' table 0x08beb7c0): records of 0x60 bytes, indexed by node: +0 the node, +4 its parent,
// +8 how many children, +12 3 for a submenu, 2 for an item, +16 the first child, +20 the previous
// sibling, +24 the next one, +28 its label (a word offset into the labels). The mod relinks them.
// T6_MOD_MENUS=0 keeps the original menus.
#include <stdlib.h>

#include "funcs.h"
#include "rt.h"

#define TREE 0x08be5b80u
#define NODE(i) (TREE + (uint32_t)(i) * 0x60u)
enum { N_PARENT = 4, N_COUNT = 8, N_FIRST = 16, N_PREV = 20, N_NEXT = 24 };
enum { PROFILE = 12, PLAYER_SETTING = 13, MAIN_CHARACTER = 15, CUSTOMIZE = 16, OPTIONS = 24, CONTROL_SETUP = 88,
       SAVE_LOAD = 92 };

static void relink(void) {
	const char *e = getenv("T6_MOD_MENUS");
	if (e && atoi(e) == 0) return;
	if (M32(NODE(CONTROL_SETUP) + N_NEXT) == SAVE_LOAD) {  // OPTIONS ends at CONTROL SETUP
		M32(NODE(CONTROL_SETUP) + N_NEXT) = 0;
		M32(NODE(OPTIONS) + N_COUNT) -= 2;
	}
	if (M32(NODE(PROFILE) + N_FIRST) == PLAYER_SETTING) {  // SELECT MAIN CHARACTER moves up to PROFILE
		M32(NODE(PROFILE) + N_FIRST) = MAIN_CHARACTER;
		M32(NODE(MAIN_CHARACTER) + N_PARENT) = PROFILE;
		M32(NODE(MAIN_CHARACTER) + N_PREV) = 0;
		M32(NODE(MAIN_CHARACTER) + N_NEXT) = CUSTOMIZE;
		M32(NODE(CUSTOMIZE) + N_PREV) = MAIN_CHARACTER;
		rt_log("mod: menus without EDIT PLAYER NAME, SAVE / LOAD and INSTALL");
	}
}

void f_089e9958(CPU *c) {
	relink();
	f_089e9958_orig(c);
}
