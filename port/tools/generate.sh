#!/usr/bin/env bash
# From your decrypted EBOOT to C (generated/): loads the program the way the PSP does, then
# translates every function with the analysis in ../re/export (program.json: where the functions
# and jump tables are; no game code). Run it again after changing hooks.txt.
#
#   tools/generate.sh [path/to/EBOOT_decrypted.elf]      (default: ../EBOOT_decrypted.elf)
set -euo pipefail
cd "$(dirname "$0")/.."
ELF=${1:-../EBOOT_decrypted.elf}
WANT=e1a919244443045731525a34d7c6dca80655c37f  # Tekken 6 (USA), ULUS-10466
[ -f "$ELF" ] || { echo "no $ELF: see README.md (Getting the decrypted EBOOT)" >&2; exit 1; }
HAVE=$(sha1sum "$ELF" | cut -d' ' -f1)
[ "$HAVE" = "$WANT" ] || echo "warning: $ELF is not the EBOOT this port was made from (SHA-1 $HAVE, expected $WANT)" >&2
python3 -I tools/prx_load.py "$ELF" ../re/export/imports.json generated
python3 -I tools/recomp.py . ../re/export/program.json
tail -3 generated/recomp_report.txt 2>/dev/null || true
