#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wramif-smoke.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
export XDG_DATA_HOME="$TMP/data"
"$ROOT/wramif" --help >/dev/null
"$ROOT/wramif" --flush >/dev/null
DB="$XDG_DATA_HOME/wramif/ramif_database.db"
test -f "$DB"
python3 - "$DB" <<'PY'
import sqlite3, sys
conn = sqlite3.connect(sys.argv[1])
assert conn.execute("select count(*) from runs").fetchone()[0] == 0
assert conn.execute("select count(*) from files").fetchone()[0] == 0
print("WRAMIF smoke test passed.")
PY
