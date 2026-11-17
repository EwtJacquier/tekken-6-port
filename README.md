# Tekken 6 — PC & browser port

If you enjoy this port and feel like it, you can [buy me a coffee](https://buymeacoffee.com/ewtjacquier). Thank you!

A fan-made port of **Tekken 6** (PSP, 2009) that runs natively on Linux and **in the browser**, at
high resolution and 60 frames a second, with HD textures, accounts for each player and **fights
with friends online** (the game's Ad Hoc mode). It is not an emulator: the game's own program was
translated, function by function, into C code that compiles for your PC or for WebAssembly.

> **Bring your own game.** This repository has no part of the game: no ISO, no game code, no game
> files. You build the port from your own copy, dumped from your own UMD. Not affiliated with
> Bandai Namco Entertainment or anyone who owns Tekken.

Made by fans, for fans: to keep a game we love alive, playable on today's machines and in a
browser tab, and to settle things with our friends again.

## Contents

1. [What you get](#what-you-get)
2. [How it works](#how-it-works)
3. [Technologies](#technologies)
4. [What you need](#what-you-need)
5. [Step by step](#step-by-step)
   1. [Get the code](#1-get-the-code)
   2. [Get the decrypted EBOOT](#2-get-the-decrypted-eboot)
   3. [Translate the game to C](#3-translate-the-game-to-c)
   4. [Get FFmpeg](#4-get-ffmpeg)
   5. [Build and play on Linux](#5-build-and-play-on-linux)
   6. [Build and play in the browser](#6-build-and-play-in-the-browser)
   7. [HD textures](#7-hd-textures)
6. [Online play: Ad Hoc only, no game server](#online-play-ad-hoc-only-no-game-server)
7. [Sharing it with friends (Cloudflare Tunnel)](#sharing-it-with-friends-cloudflare-tunnel)
8. [Accounts and saves: where everything is kept](#accounts-and-saves-where-everything-is-kept)
9. [Things to know](#things-to-know)
10. [Repository layout](#repository-layout)
11. [Credits](#credits)

## What you get

- **The whole game**, translated to native code: every mode, fighter, stage and costume,
  customization, music and voices.
- **4x internal resolution** (1920x1088) at **60 frames a second**, in fights too, natively and in
  the browser.
- **HD textures: the HD UI essentials pack**: the game's interface at 4x: the menus, the
  character select, the fight's HUD, the rank badges, the buttons and the command list's arrows,
  and the fonts, redrawn sharp from the game's own glyph tables (each letter where the game expects
  it); the title screen's movie plays in HD too. Most were upscaled with the
  [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) anime model, the ones where that fell short
  redrawn with OpenAI's image models, each one checked against its original. It comes as a separate
  download and never changes the game's files: turn it off and on in the settings menu.
- **A browser version**: the game in a tab, with a settings menu (Esc): fullscreen, volume,
  keyboard keys, gamepad buttons and which stick is the PSP's analog stick, internal resolution, HD
  textures, how the original textures are filtered.
- **Players**: each player logs in and keeps their own save; their name is their name in the game
  and online.
- **Online fights** through the game's **Ad Hoc** mode: one player hosts, the other joins. There
  is no game server: see [Online play](#online-play-ad-hoc-only-no-game-server).
- **Mods** (C code in `port/mods/`, each can be turned off):
  - straight to the title: no logos, no opening movie (the title screen in about two seconds);
  - the player's name is the account's name (no EDIT PLAYER NAME in the menus);
  - clearer menus: PROFILE starts with SELECT MAIN CHARACTER; OPTIONS without SAVE / LOAD (the
    save is handled by the port) and without DATA INSTALL (nothing to install).
- **Fixed along the way** (the port behaves as the PSP, not as a naive translation): the fighters'
  legs no longer shake between two poses (the VFPU's exact dot product), the game no longer drops
  to 30 frames a second for good, no stray polygons in the stages (the GE's bounding boxes and
  culling), the fog over the water and ground as on the PSP.

## How it works

The PSP runs MIPS machine code. Emulators interpret it (or compile it on the fly) while the game
runs. This port does it once, ahead of time: **static recompilation**.

```
your UMD ─▶ ISO ─▶ EBOOT.BIN (encrypted) ─▶ EBOOT_decrypted.elf
                                              │
           re/export/program.json ────────────┤  where each function and jump table is
           (made once with Ghidra)            │  (addresses only: no game code)
                                              ▼
                            tools/prx_load.py + tools/recomp.py
                                              │
                                              ▼
                  port/generated/*.c  ── about 23,500 functions as C code (stays on your PC)
                                              │
              port/runtime/*.c  ──────────────┤  the PSP's system, rewritten: threads, files,
              port/mods/*.c                   │  graphics, sound, movies, saves, network
                                              ▼
                        native binary (GCC)  or  WebAssembly (Emscripten)
```

- Each MIPS function becomes a C function; registers live in a struct, the PSP's memory is one
  block of memory, branches become `goto`s. The VFPU (the PSP's vector unit) is translated inline,
  with the hardware's exact rounding where the game depends on it.
- The PSP system functions the game calls are implemented by this project's runtime (nothing comes
  from an emulator): graphics go to OpenGL ES 3 / WebGL 2, sound to SDL or WebAudio, movies through
  FFmpeg, the game's files are read straight from your ISO.
- Mods replace game functions by address (`port/hooks.txt`) with hand-written C that can still
  call the original.

`docs/NOTES.md` has the technical notes: the disc and its encrypted data, the anti-tamper check,
the frame rate lock, the boot, the menu tree, the GE's culling, the fog, the fonts, the HD movies.

## Technologies

| What | Used for |
|---|---|
| **C** (GNU C11), **GCC**, **Make** | the generated game code, the runtime, the mods |
| **Python 3** | the recompiler, the build tools, the local web server (standard library only) |
| **SDL2** | window, input and sound in the native build |
| **OpenGL ES 3 / WebGL 2** | the PSP's graphics chip, rendered at 4x |
| **Emscripten** (WebAssembly, Asyncify, SIMD) | the browser build; the PSP's threads become fibers |
| **FFmpeg** (PPSSPP's builds) | ATRAC3+ music and voices, the movies |
| **LiveKit** (WebRTC) | online play: the game's Ad Hoc messages between players |
| **Ghidra** (optional) | the analysis that found every function (already done: `re/export/`) |
| **Real-ESRGAN**, **OpenAI image models** | how the HD textures were made (`realesrgan-x4plus-anime` at 4x; `gpt-image` redraws) |

## What you need

**The game**: *Tekken 6* (USA), **ULUS-10466**, as an ISO dumped from your own UMD (with a PSP
running custom firmware, for example).

**To build** (Linux; on Windows use WSL2 with Ubuntu):

```bash
sudo apt install build-essential pkg-config git python3 libsdl2-dev libgles-dev
```

- For the browser build, the **Emscripten SDK** (tested with 3.1.46):
  ```bash
  git clone https://github.com/emscripten-core/emsdk ~/emsdk
  cd ~/emsdk && ./emsdk install 3.1.46 && ./emsdk activate 3.1.46
  export EMSDK=~/emsdk          # the build scripts use it (or: source ~/emsdk/emsdk_env.sh)
  ```
- About **8 GB of RAM** and **2 GB of disk** for building (plus ~400 MB for the HD textures).

**To play**: Linux for the native build; for the browser build, any desktop browser with WebGL 2
(Chrome, Edge, Firefox) on any system. A gamepad is recommended (it is a fighting game).

## Step by step

### 1. Get the code

```bash
git clone https://github.com/<you>/tekken-6-port.git
```

Put your ISO **inside** the `tekken-6-port` folder (git ignores it), named
`Tekken 6 (USA) (En,Ja,Fr,De,Es,It,Ko,Ru).iso` (or pass its path to the commands below):

```
tekken-6-port/
├── Tekken 6 (USA) (En,Ja,Fr,De,Es,It,Ko,Ru).iso
├── EBOOT_decrypted.elf      ← step 2
├── port/
└── re/
```

### 2. Get the decrypted EBOOT

The game's program is `PSP_GAME/SYSDIR/EBOOT.BIN` inside the ISO, and it is encrypted. Ways to
decrypt it:

- **With [pspdecrypt](https://github.com/John-K/pspdecrypt)**: take `EBOOT.BIN` out of the ISO
  (any ISO tool, e.g. `7z x "Tekken 6 (USA) (En,Ja,Fr,De,Es,It,Ko,Ru).iso" PSP_GAME/SYSDIR/EBOOT.BIN`),
  then `pspdecrypt EBOOT.BIN` writes the decrypted file.
- **With [PPSSPP](https://www.ppsspp.org)**: in *Settings → Tools → Developer tools*, turn on the
  option that dumps the decrypted EBOOT when a game boots, then start the game once; the file
  lands in PPSSPP's memory stick folder, under `PSP/SYSTEM/DUMP/`.
- **With `re/tools/prxdec`**: PPSSPP's own decrypter built into a small command line tool
  (`PPSSPP=path/to/ppsspp-source re/tools/prxdec/build.sh`, then
  `re/tools/prxdec/prxdec EBOOT.BIN EBOOT_decrypted.elf`).

Save it as `EBOOT_decrypted.elf` in the `tekken-6-port` folder (git ignores it). Its SHA-1 must be
`e1a919244443045731525a34d7c6dca80655c37f` (step 3 checks it).

### 3. Translate the game to C

```bash
cd tekken-6-port/port
tools/generate.sh            # or: tools/generate.sh path/to/EBOOT_decrypted.elf
```

This is the "decompiling" part, done for you: it loads the program the way the PSP's kernel
would (`tools/prx_load.py`) and translates every function to C (`tools/recomp.py`), using the map
of the program in `re/export/program.json`. It takes a couple of minutes and writes
`port/generated/` (about 70 MB of C). **That folder is the game's code: it is yours to build,
never commit or share it.**

Redoing the analysis yourself is optional: it needs Ghidra, see `re/README.md`.

### 4. Get FFmpeg

```bash
tools/get_ffmpeg.sh
```

It downloads [PPSSPP's FFmpeg builds](https://github.com/hrydgard/ppsspp-ffmpeg) (only the
decoding libraries), uses the prebuilt ones for Linux and builds the WebAssembly ones with
Emscripten (a few minutes, once).

### 5. Build and play on Linux

```bash
make -j8
./build/tekken6              # or: ./build/tekken6 path/to/the.iso
```

The first build takes a while (about 23,500 functions; `-j8` is fine with 16 GB of RAM, use fewer
jobs with less). Keys: **arrows** = the analog stick, **I J K L** = the d-pad, **A** = □ (left
punch), **S** = △ (right punch), **Z** = ✕ (left kick), **X** = ◯ (right kick), **Q**/**W** = L/R,
**Enter** = Start, **Backspace** = Select; USB gamepads work too. Saves go to
`build/ms0/PSP/SAVEDATA`.

### 6. Build and play in the browser

```bash
tools/build_web.sh                 # writes build/web/
python3 tools/serve_web.py         # http://127.0.0.1:8644/
```

Open **http://127.0.0.1:8644/**. The server gives the page the ISO (only the parts the game
reads), keeps the saves in the project folder and handles the players' accounts.

- **Log in** or create a player (a new one starts from a copy of the main save), or "Play without
  an account" (the main save). Each browser *window* is one player: two windows, two players. The
  sign-in screen shows the game's art, read from your ISO.
- **Esc** (or the gear in the corner, or Home on a gamepad) opens the settings: fullscreen (also
  F11 or a double click), the volume, internal resolution (1x to 4x), how the original textures
  are filtered, HD textures, keys, gamepad buttons and which stick is the PSP's analog stick.
- Opened from another computer (a friend, through a tunnel), the page first copies the game into
  that browser: the ISO and the HD textures, about 1.2 GB once, with a progress bar, so the game
  never waits on the network after that (`?preload=0` in the address skips it).
- Without the server (the page opened some other way) the page asks for the ISO file and keeps
  the save in the browser.

### 7. HD textures

The first texture pack is **HD UI essentials**: the interface, where the PSP's low resolution
shows the most (the menus, the character select, the fight's HUD, the rank badges, the buttons, the
command list's arrows, the fonts) and the title screen's movie; 668 textures. The fighters and the
stages keep the game's own textures for now. Download it (the `tekken6-hd-textures*.zip` file on
this repository's Releases page) and install it, from `port/`:

```bash
python3 tools/textures.py install ~/Downloads/tekken6-hd-textures*.zip
python3 tools/textures.py status
```

Its textures are upscaled 4x and compared with their originals: most with Real-ESRGAN's anime model
(`realesrgan-x4plus-anime`), the ones where the plain upscale fell short redrawn with OpenAI's image
models (`gpt-image`), fitted back onto the original's place and outline and reviewed one by one.
The font pages are redone with clean edges instead (an AI upscale blurs thin letters), each letter
on the game's own baseline and spacing. The title screen's movie comes in HD too
(`movies/title.h264`, drawn in place of each of its frames). The pack only **adds** files in
`build/textures`: the game looks each texture up while it runs and draws the HD version in its
place, so your ISO and the game's files never change.

**Going back to the originals** (the rollback):

- in the browser, Esc → **HD textures: Off** (instant, and back On whenever you like);
- or remove the pack: `python3 tools/textures.py uninstall`.

No copy of the original textures is needed for that: they always come from your ISO.

## Online play: Ad Hoc only, no game server

**What works**: the game's **Ad Hoc** mode, the one PSPs played side by side: one player creates a
room in their own game, the other finds it and joins, and they fight; the ghost exchange works the
same way.

**What doesn't**: there is **no game server** anywhere. Nobody runs a server for this port, and it
doesn't need one: the players' own games talk to each other, exactly as on the PSP.

**How the players reach each other**: Ad Hoc is Wi-Fi between consoles in the same room. The port
emulates that network (`port/runtime/net.c`) and, in the browser, carries its messages through a
**LiveKit room**: LiveKit is a service for real-time apps (WebRTC); here it is only the "Wi-Fi",
passing the game's small messages along, so its **free plan** is plenty.

1. Create a free account at **[cloud.livekit.io](https://cloud.livekit.io)** and a project.
2. In the project's settings, create an **API key**. Note the three values: the project's
   **URL** (`wss://<your-project>.livekit.cloud`), the **API Key** and the **API Secret**.
3. Write them in `port/.env` (the file is ignored by git: never commit it):
   ```ini
   LIVEKIT_URL=wss://your-project.livekit.cloud
   LIVEKIT_API_KEY=<your API key>
   LIVEKIT_API_SECRET=<your API secret>
   ```
4. Restart `serve_web.py`. The login screen shows **Network play: ready**.
5. In the game, both players open the Ad Hoc menu: one creates the room, the other joins it.

How it fits together: the page asks *your* server for a pass (`/livekit-token`), and your server
signs it with the secret, which never leaves your PC. Everyone who opens the game from the same
server meets in the same room (`?T6_ROOM=name` in the address makes a separate one). To try it
alone, open two browser windows and log in as two players.

## Sharing it with friends (Cloudflare Tunnel)

To play with friends who are not on your network, one person (**the host of the page**) runs the
server and shares it through a **[Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/)**:
a secure HTTPS address for your local server, with no router setup.

What happens then: your friends open the address; their browser loads the game from your server
and runs it **on their own computer**; their saves and accounts live on yours; online fights go
through LiveKit with passes your server signs (they need no LiveKit account).

**Quick test** (a temporary random address):

```bash
# install cloudflared: https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/
cloudflared tunnel --url http://127.0.0.1:8644
```

It prints an address like `https://something-random.trycloudflare.com`: send it to your friends.
It lasts while the command runs.

**Your own address, open only to your friends** (free, needs a domain on Cloudflare):

```bash
cloudflared tunnel login                                         # pick your domain
cloudflared tunnel create tekken6                                # prints the tunnel's id
cloudflared tunnel route dns tekken6 tekken6.your-domain.com
```

`~/.cloudflared/config.yml`:

```yaml
tunnel: tekken6
credentials-file: /home/<you>/.cloudflared/<tunnel-id>.json
ingress:
  - hostname: tekken6.your-domain.com
    service: http://127.0.0.1:8644
  - service: http_status:404
```

```bash
cloudflared tunnel run tekken6
```

Then, in Cloudflare's dashboard, **Zero Trust → Access → Applications → Add a self-hosted
application** for `tekken6.your-domain.com`, with a policy that allows your friends' e-mail
addresses: each one logs in with a code sent to their e-mail before the game opens.

**Please read this before sharing:**

- Whoever opens the address downloads the game's data from **your** ISO and the translated game
  code. Share it **only with friends who own the game**, never publicly: use the Cloudflare Access
  policy above for anything longer than a quick test.
- Anyone who reaches the page can create a player on your server: another reason for Access.
- The game streams from your PC: your upload speed is everyone's first loading time (about 1.2 GB
  per friend, once; after that their browser keeps it).

## Accounts and saves: where everything is kept

There is no database server: everything is plain files in your `port/build/` folder, written by
`serve_web.py` (and by the native build, for the main save).

| What | Where |
|---|---|
| Main save (native build, and "Play without an account") | `build/ms0/PSP/SAVEDATA/` |
| A player's account (name, password hash) | `build/profiles/<name>/account.json` |
| A player's save | `build/profiles/<name>/ms0/PSP/SAVEDATA/` |
| Logged-in sessions | `build/profiles/sessions.json` |
| HD textures and movies | `build/textures/` |
| Settings of the page (keys, gamepad, switches) | the browser's `localStorage` |
| The game copied into a friend's browser | that browser's Cache Storage (site data) |
| Saves when the page runs without the server | the browser's IndexedDB |

- Passwords are never stored: only a salted **PBKDF2-SHA256** hash (200,000 rounds).
- Every time a save is written, the previous one is kept as `.bak` next to it.
- A new player starts from a copy of the main save; the game shows their account's name.
- **Back up**: copy `build/ms0` and `build/profiles`.
- **Forgotten password**: the host deletes `build/profiles/<name>/account.json`; the player then
  creates the same name again with a new password, and keeps their save.

## Things to know

- **Legal**: this repository holds only the port's own code, the analysis data (addresses and
  names) and icons drawn for the project. The game, its code and its art belong to their owners.
  Don't publish builds, generated code, ISOs or EBOOTs. The HD texture pack is derived from the
  game's art and meant for people who own the game.
- **Native builds**: Linux only for now (the browser build runs everywhere). Native online play
  only works between copies on the same machine (for testing); online play is a browser feature.
- **Online play is the newest part** of this port: if a fight or a lobby misbehaves, tell us.
- **Smooth motion**: on the PSP the graphics chip's drawing time spaced the frames; here it takes
  no time, so the port paces them itself (one frame per screen refresh). The game also measures
  its own load and would drop to 30 frames a second for good: the port reports the timings a PSP
  would, so it stays at 60.
- **Performance**: full speed at 4x on a laptop with integrated graphics (where it was made). The
  HD textures need more video memory; lower the internal resolution if a weak machine stutters.
- **Phones and tablets** get a note asking for a computer instead of the game (nothing of the
  game is downloaded there): it needs a keyboard or a gamepad, and a computer's memory.
- **Sound** starts after the first click or key press (browsers' rule).
- **Saves** are the game's own files: the native build and the browser build share the main one.
- **Switches** (environment variables natively, `?NAME=value` in the browser's address):
  `T6_MOD_SKIP_INTRO=0`, `T6_MOD_MENUS=0` and `T6_MOD_NAME=0` turn the mods off,
  `T6_SCALE=1..4` sets the render scale, `T6_FPS=1` logs the frame rate.
- Something wrong? In the browser, **F9** writes a diagnostic of the current frame into
  `build/webdump/` on the host (only from the host's own browser). The browser's console (F12)
  shows the game's log.

## Repository layout

```
port/
  runtime/        the PSP's system, rewritten (CPU helpers, VFPU, threads, files, graphics,
                  sound, movies, saves, network, texture packs)
  mods/           the mods (C), hooked by address in hooks.txt
  web/            the browser page (shell.html), audio and LiveKit glue
  tools/          generate.sh, recomp.py, prx_load.py, serve_web.py, textures.py, build scripts,
                  disc_textures.c (every texture on the disc, decoded: for making texture packs)
  third_party/    FFmpeg's headers (the libraries come from tools/get_ffmpeg.sh), stb_truetype
docs/
  NOTES.md        technical notes: the disc, the anti-tamper, the frame rate, the menus, the GE
re/
  export/         the analysis: program.json (functions, jump tables), imports
  ghidra_scripts/ how program.json was made (Ghidra)
  tools/          the EBOOT decrypter, Ghidra helpers
```

## Credits

- **Bandai Namco** (Namco Bandai Games), for Tekken 6.
- [PPSSPP](https://www.ppsspp.org): the reference for how the PSP behaves, its FFmpeg builds, its
  tables of PSP system function names and its EBOOT decrypter.
- [Ghidra](https://ghidra-sre.org), [Emscripten](https://emscripten.org),
  [SDL](https://www.libsdl.org), [FFmpeg](https://ffmpeg.org), [LiveKit](https://livekit.io)
  (client library, Apache-2.0), [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN) (BSD-3-Clause),
  [x264](https://www.videolan.org/developers/x264.html) (the HD movie),
  [stb_truetype](https://github.com/nothings/stb) (public domain / MIT).
