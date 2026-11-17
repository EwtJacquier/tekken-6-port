# Tekken 6 (ULUS-10466) — reverse engineering

Everything the port needs from here is already in `export/`: you only need this folder to redo the
analysis, or to look into the game. Addresses are always in the PSP's runtime form (the program
loads at `0x08804018`): the same in Ghidra, in PPSSPP's debugger and in the port (`f_08869e00` is
the function at `0x08869e00`).

## Files

| Path | What it is |
|---|---|
| `../EBOOT_decrypted.elf` | the decrypted EBOOT.BIN (a plain MIPS/Allegrex executable), yours, never committed |
| `export/program.json` | where every function and jump table is: what the recompiler reads |
| `export/imports.json` | the PSP system functions the game imports (NID → name, from PPSSPP's tables) |
| `ghidra_scripts/` | the scripts that made and use the analysis (export, signatures, decompile, cross-references) |
| `ghidra_project/` | the Ghidra project `analyze.sh` makes (not committed) |
| `tools/decomp.sh` | prints Ghidra's pseudo-C for the functions at some addresses |
| `tools/gen_imports.py` | makes `imports.json` from the EBOOT and PPSSPP's source tree |
| `tools/prxdec/` | decrypts EBOOT.BIN with PPSSPP's decrypter (`PPSSPP=path/to/ppsspp build.sh`) |

## Redoing the analysis

It needs [Ghidra](https://ghidra-sre.org) 12.2 with the
[ghidra-allegrex](https://github.com/kotcrab/ghidra-allegrex) extension (the PSP's CPU and its
ELF loader) and a JDK 21 or newer. The scripts look for them in `~/tools/` (`ghidra_12.2_PUBLIC`,
`jdk-25.0.5+8`); point them elsewhere with `GHIDRA=...` and `JAVA_HOME=...`.

```bash
re/analyze.sh                # import + analyze EBOOT_decrypted.elf, export program.json (a few minutes)
port/tools/rebuild.sh        # export again, translate to C and build
re/tools/decomp.sh 08869e00  # Ghidra's pseudo-C of a function (read-only)
```

Ghidra's analysis of this executable is rough (branches into other functions' bodies, calls into
the middle of functions, missed switch tables): the recompiler (`port/tools/recomp.py`) makes up
for it by itself, see `../docs/NOTES.md`.

## Findings

`../docs/NOTES.md` has the notes: the disc and its encrypted data block, the textures' packages,
the anti-tamper check, the frame rate lock, the boot sequence, the profile, the menu tree, the
VFPU's exact dot product, the GE's culling, the fog, the fonts' glyph tables and the HD movies.
