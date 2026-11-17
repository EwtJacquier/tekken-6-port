// Mod (profile name): the profile carries the player's name from the site (T6_NICK: the account
// a browser signed in with), whatever the save had; without one the game's own name stays.
//
// The profile (0x2168 bytes) is made by FUN_08a3bed4 (a0: where), which writes the default
// "PLAYER_NAME" at +0x61 (a 32-byte field; the game takes up to 12 characters). It is made at boot
// and then filled from the save, so the name is put back every frame when it differs (boot.c calls
// profile_name_frame), and is saved with the profile. T6_MOD_NAME=0 leaves the name alone.
#include <stdlib.h>
#include <string.h>

#include "funcs.h"
#include "rt.h"

#define NAME_AT 0x61
#define NAME_FIELD 32
#define NAME_MAX 12

static uint32_t profile;

static const char *login_name(void) {
	static int on = -1;
	if (on < 0) on = !getenv("T6_MOD_NAME") || atoi(getenv("T6_MOD_NAME")) != 0;
	const char *n = getenv("T6_NICK");
	return on && n && *n ? n : NULL;
}

void profile_name_frame(void) {
	const char *n = login_name();
	if (!n || !profile) return;
	char want[NAME_FIELD] = {0};
	strncpy(want, n, NAME_MAX);
	if (strncmp((const char *)MP(profile + NAME_AT), want, NAME_FIELD) == 0) return;
	memcpy(MP(profile + NAME_AT), want, NAME_FIELD);
	static bool said;
	if (!said) said = true, rt_log("mod: the profile's name is %s", want);
}

void f_08a3bed4(CPU *c) {
	profile = A0;
	f_08a3bed4_orig(c);
	profile_name_frame();
}
