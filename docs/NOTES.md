# Tekken 6 (ULUS-10466): technical notes

## The disc
- `PSP_GAME/SYSDIR/EBOOT.BIN`: encrypted (~PSP), decrypted with re/tools/prxdec (PPSSPP's
  PrxDecrypter). `BOOT.BIN` is all zeros. The result is a plain executable (ELF ET_EXEC), not a
  relocatable module: one LOAD segment at 0x08804018 (memsz 0x45f02c), entry 0x08804124, no
  section headers; the module info is at the segment's p_paddr (file offset 0x386740 =
  0x08b89740), gp 0x08c022b0. The import stubs (268 functions, all in PPSSPP's tables) run from
  0x08b88c68: the code is 0x08804018..0x08b88c68.
- The entry does not set gp: the kernel gives every thread the module's gp (runtime/kernel.c,
  META_GP from tools/prx_load.py).
- Data: `PSP_GAME/USRDIR/DATA401.IDX` (encrypted index) and ~384 MB after the file system's files,
  read by sector (`disc0:/sce_lbn0x38370_size0x16e42800`). Files are encrypted (key schedule in
  FUN_08aa305c/FUN_08aa2ca8, decrypted in place by the reader thread "ktRead"), checked with a
  CRC-32 (FUN_08aa2ae0, table at 0x08c2ba00; the expected CRCs are in the index), then mostly gzip
  (the game's own inflate, hand-written with bltzal subroutines: FUN_08b556cc / FUN_08b55bf0).
- Movies: `USRDIR/movie/*.pmf` through scePsmfPlayer (opening, title, the endings). Modules
  `USRDIR/modules/psmf.prx`, `libpsmfplayer.prx` (HLE, not loaded).
- Pictures for the page: `PSP_GAME/PIC1.PNG` (480x272 key art with the logo), `ICON0.PNG`.

## Recompiling it (port/tools/recomp.py), what this game needed
Ghidra's analysis of this executable is rough (1,649 branches into other functions' bodies, 171
calls into the middle of functions, functions started at delay slots, missed switch tables):
- Shared code: a branch into another function's body copies the code reachable from there into
  the branching function (functions may overlap); every way out of a function's code (fall
  through, branches, after a call) that is not another function's entry is followed; falling
  into an entry is a tail call; a branch's delay slot always belongs to the branch.
- Functions: the entry, every jal target, and every code address the program stores (vtables,
  callback tables, C++ member function pointers) or builds in a register (lui + addiu/ori,
  followed through the function, and a looser pass for switch cases) becomes a function.
- Switch tables found in the code (lw from table + index*4 before jr, the bound from sltiu).
- bltzal/bgezal: local subroutines of hand-written code: a goto setting ra, and jr ra jumps back
  to the local return points.
- The import stubs end the code range: a tail jump to an import (j sceKernelStartThread) is the
  system call, never the stub's bytes.

## Things the game checks
- Anti-tamper: it loads `ms0:/MEMSTICK.IND` as a module (name XOR 0xA5 at 0x08bef158) and
  compares the result (FUN_08aa1908 and copies). A retail PSP refuses modules on the memory stick
  (0x80020146); a success (a console with plugins) poisons a global added to every file's CRC
  (0x08bfe0bc = CRC table[1]): every check fails and loading never ends. sceKernelLoadModule
  answers as a retail PSP.
- Frame rate: FUN_08861a24 measures CPU and GPU load each frame and drops to 30 frames a second
  for good when the GPU took over 95% of a frame, or the display's line after its drawing is 264
  or later. GPU time = the GE FINISH callback's clock minus the clock noted after
  sceGeListEnQueue: the callback is delivered later (display.c, T6_GE_ASYNC=0: at once) and reads
  the time the GE finished; sceDisplayGetCurrentHcount counts lines from the last vblank.
- The PLL: it sets 222/222/111 MHz at boot and waits until scePowerGetPllClockFrequencyInt says so.

## State (2026-11-14)
- Native: boots, logos, opening movie, title, profile creation, menus, character select, Arcade
  fights at 60 frames a second, saves (savedata modes 8, 11, 13-18 added).
- Web: the same at 60 frames a second in fights too (was ~37). A match draws ~6,700 times a frame
  (~6,000 triangle strips of ~6.5 vertices, nearly all lit by 3-4 directional lights, specular
  powers 10 and 20), so the time per draw counted as much as the time per vertex:
  - xf_setup works out the matrices and lights again only when a register or matrix it reads
    changed (ge_command counts those changes in xf_gen / vp_gen); before, three 4x4 products in
    double and every color unpacked, every draw.
  - Directional lights: the ambient parts summed, the colors times the material's and the half
    vectors made once per draw; per vertex a dot product per light (specular powers multiplied
    out: powf is slow in the browser). Checked against the old code on ~100 million vertices:
    the same positions, 161 color components 1/255 apart.
  - Vertices go to the GPU once with 32-bit indices (32-byte vertices, fog in c1's alpha; batches
    queued in ring buffers), as in the Fat Princess port.
  - Textures: where each key was last found is looked at before the 512-entry table; CLUTs are
    hashed only for the part a format reads (~300 CLUT loads a frame); the target framebuffer's
    lookup remembers the last one.
- Boot (mods/boot.c): the mode switcher FUN_08869e00 (once a frame, a0 = the mode manager; asked
  mode at +0x80, 0x26 = none; current at +0x7c) goes 0 -> 18 (logos) -> 22 (opening movie, then
  the title) -> 13 (main menu: loads the system save or makes a profile); 2 is the demo fights
  (also where the opening goes when it ends by itself). The first ask for 18 becomes 22 and the
  opening plays black and silent (psmf.c rt_psmf_hide_next) while START is pressed for the player:
  the title in ~2 s. T6_MOD_SKIP_INTRO=0 keeps the original sequence, T6_BOOT_LOG=1 logs modes.
- The profile (mods/profile.c): 0x2168 bytes made by FUN_08a3bed4 (at boot, then filled from
  SYSDATA/MAINDATA.SAV, an encrypted save); the name at +0x61 (32 bytes, up to 12 characters,
  default "PLAYER_NAME" from 0x08bc8210). The mod keeps it equal to T6_NICK (the site's login).
- Menus (mods/menus.c): a tree in the executable at 0x08be5b80 (FUN_089e9958 sets it up beside
  the labels' table 0x08beb7c0, an exported spreadsheet: DebugName, string ids...): records of
  0x60 bytes per node: +4 parent, +8 children, +12 3 submenu / 2 item, +16 first child, +20 previous,
  +24 next, +28 label. PROFILE 12 (PLAYER_SETTING 13 -> EDIT_PLAYER_NAME 14, MAIN_CHARACTER 15;
  CUSTOMIZE 16, RECORDS 17, GHOST_LIST 18), OPTIONS 24 (GAME 25, SOUND 48, CONTROL 88, SAVE_LOAD
  92, DATA_INSTALL 101). Relinking them changes the menus; T6_MOD_MENUS=0 keeps them.
- Legs shaking: the foot IK flipped between two poses frame after frame; the VFPU's vmmul must
  form each element as the hardware does (exact products truncated, aligned, summed as integers:
  vfpu.c psp_dot4; PPSSPP's compat.ini turns the same on for this game, "MoreAccurateVMMUL").
  tools/recomp.py writes vmmul inline (VFAST), so it calls rt_vdot4 there too: with the runtime
  alone nothing changed. Measured on the bone/world matrices per vertex buffer over frames.
- Leaving ad hoc (battle lobby, ghost exchange): CState_DisconnectingIBSS waits to see the
  network object "disconnecting" (status FUN_089c6104 == 2) before the library is shut down; net.c
  keeps the state connected and tells DISCONNECT 100 ms after sceNetAdhocctlDisconnect.
- Classes are easy to name at runtime: an object's vtable[-1] is its typeinfo, whose +4 is the
  mangled name (COnlineSubModeBattleLobby_PSP, CState_DisconnectingIBSS...).
- DXT textures keep red in the high bits of their 565 colors (the select screen's portraits).
- Bloom: the game copies the screen at a quarter size into a 565 buffer, subtracts a threshold
  (reverse subtract) and adds it back; a framebuffer read as a 565 texture must read alpha 1.
- Fighters were see-through at high render scales: the stencil-to-alpha passes (needed by Fat
  Princess's glow) are off unless T6_STENCIL_ALPHA=1.
- Switches: T6_* (T6_INPUT, T6_DUMP, T6_MS0, T6_SCALE, T6_HEADLESS, T6_NOAUDIO, T6_FRAMES, T6_FPS,
  T6_TRACE, T6_STATUS, T6_STATUS_STACK, T6_PROFILE, T6_HITCH, T6_GE_ASYNC...).
- Headless test into a fight (new save: first create the profile with X presses):
  `T6_INPUT="900:start,1000:start,1300:start,1500:cross,1700-5300/60:cross"`.

## The GE's culling (2026-11-16)
- BOUNDINGBOX (0x07) + BJUMP (0x09): Tekken 6 tests ~440 boxes a frame and jumps past what is out of
  sight. ge.c ge_bounding_box does the GE's test as PPSSPP has it (the vertices, skinned and by the
  world matrix, against the planes of the offset/scissor/region area; near/far only with depth
  clamping; of more than 0x100 vertices one run of 0x100) and moves the vertex/index address on as
  a drawing does. Before, every box was "visible" and the address stayed: objects the game had
  stopped updating were drawn (polygons from nowhere), and ~28% more draws. T6_NO_BBOX=1: as before.
- Vertex range culling: a primitive with a vertex outside the GE's screen space (x, y 0..4096
  before the offset) is not drawn, nor, without depth clamping (CLIPENABLE), one with a vertex
  outside the projection's depth range (no near-plane clipping then); T6_NO_RANGE_CULL=1 off,
  T6_CULL_SHOW=1 paints them magenta instead, T6_CULL_LOG=1 counts them.
- Tests: T6_FAKE_TIME=<unix seconds> pins the date the game sees; Arcade draws its stages from it,
  so a stage comes back (1800000444: the jungle with water; 1800000000 village; 1800000222 snow...).
  T6_SKIPDRAW=a-b leaves out a frame's draws a..b (what draws what).
- How Arcade draws: newlib's srand(time()) at boot (0x0889d5c4 -> 0x08b7b5c8; the state is 64 bits
  at _REENT+168, _REENT = *(0x08bf0bd0) here), three rand() (0x08b7b600) on each of boot, title and
  main menu, then the ladder (stages, opponents: rand from 0x0899aefc on) the moment ARCADE BATTLE is
  chosen. So with T6_FAKE_TIME=T the ladder starts from LCG^9(T) (next = next * 6364136223846793005
  + 1). The sceRtcGetCurrentTick at 0x0889d048 right after is the ghost record's date, not the draw.

## Fog (2026-11-16)
- The fog depth ((view z + FOG1) * FOG2) goes to the GPU as it is (GVert.fog, a float) and is
  clamped to 0..1 per pixel, as the GE and PPSSPP (its software rasterizer too) do. It used to be
  clamped per vertex: a polygon with a vertex far past the fog's ends (the jungle's water and
  ground, from under the camera to the back) got fog all across its middle: washed-out, cyan
  water. Checked against PPSSPP on the same stage and opponent (below).

## Vertex ring and the main menu's black flashes (2026-11-16)
- Every ~5 s (~330 frames) the main menu showed one frame with its interface over black: batch_flush
  put the draws' vertices (GVerts) and then the sprite corners (SVerts) into one 16 MB ring buffer,
  and when the corners did not fit the ring was made anew (glBufferData) - the vertices just put in
  the old one were gone, and the draws read zeros. The menu's background is all vertices, its
  interface all sprites. Now room for both is made before either goes up. Caught by a frame-by-frame
  check (a dark logo area) with the draw logs of the frames before it: same draws, same textures,
  same CLUTs, same target; probing the framebuffer mid-frame (flushes) made it go away.


## Comparing with PPSSPP (the flatpak, isolated)
- The flatpak's PPSSPP with its own config in its private /tmp (XDG_CONFIG_HOME set inside
  `flatpak run --command=sh`, else it uses ~/.var/app's), no network (`--unshare=network`: its
  debugger's web server reports the LAN address to report.ppsspp.org and blocks on it), on a
  nested X display (`Xwayland :57 -geometry 960x544`; SDL's offscreen driver stalls the emulation).
  Its debugger (RemoteDebuggerOnStartup, ws://127.0.0.1:PORT/debugger) is reached from the host
  with `nsenter -t PID -U -n --preserve-credentials`; keep one connection open (this build aborts
  on some closes). input.buttons.press drives it; pictures are taken from the X display
  (PIL ImageGrab xdisplay) since gpu.buffer.screenshot needs stepping. No save: create the profile
  (X through the dialogs, autosave off).
- Same stage as the port: a breakpoint at 0x0899aef4 (the ladder's first rand) and _REENT+168 set
  to LCG^9(T) there, T the port's T6_FAKE_TIME: same stage and opponent.

## Every texture from the disc (tools/disc_textures.c)
- The data block after the file system (lbn 0x38370, 0x16e42800 bytes) is encrypted as ONE stream by
  position. Its first sector holds the key: words k[i] = sector[sector[T788[i]] ^ T780[i]] (tables at
  0x08bce780/0x08bce788; T788 = 0xf1, 0x1ab, the two words FUN_08aa305c's compare skips). Each 1 KB n:
  x = n + k0, x += bitreverse(x), x = rotr(x, (3n mod 29) + 2) ^ k1, then each word ^= x and
  x = rotl(x, 3) * 5 + 1 (FUN_08aa2ca8). Offsets are the block's own (sector 0 = the header).
- Decrypted: "MOTA" archives, "GZIP" blocks (u32 1, 1, unpacked size, packed size, then zlib; 7049, all
  unpack) and "NTPP" texture packages (u16 1, u16 count; per texture a header: u32 total, palette
  size, texel size, u16 header size (0x50), u16 colors, byte mip count at +0x11, palette format +0x12
  (GE CLUT format; 15 none), texel format +0x13 (GE format | 0x80 swizzled), u16 w, h at +0x14; "eXt"
  and "GIDX" chunks; texels at +header size, palette after them). Entries with w = h = 0 are palettes
  alone (26,674: costume colors, in packages of their own; which texture each goes with is not known).
- disc_textures decodes each texture as ge.c does; the key covers the picture (w x h): 3,723 of the
  4,200 textures collected in play have the same key. 11,323 different textures on the disc, 7,600 the
  game was not seen drawing. Not decoded: textures whose palette is
  elsewhere (~4,000 headers), and fighters drawn with the costume palettes (those come from play).

## HD movies (runtime/psmf.c, ge.c)
- A movie with an HD version in the texture pack, `movies/<name>.h264` (H.264 Annex B, a whole multiple
  of 480x272, the same frames at 29.97 fps), has each frame's HD picture decoded beside the PSP one
  (frame n with frame n) and drawn in its place: ge.c gives the texture at the frame's buffer a texture
  of the picture's size, the coordinates scaled (uvsx = texture width / 480). The game writes its frames
  into three buffers in turn and draws an earlier one: one HD picture per buffer. On the web the stream
  is fetched from textures/movies/ when the movie is set (texpack_file); until it is there, the PSP frames.
- title.pmf (119 frames): its frames out with a PMF extractor (libavcodec), Real-ESRGAN anime 4x (tile
  128: bigger tiles come back black on this GPU), encoded with ffmpeg/libx264 (crf 16, no B-frames,
  `-f h264`): 2.7 MB. T6_MOVIE_LOG=1 lists the textures read from movie frames.
