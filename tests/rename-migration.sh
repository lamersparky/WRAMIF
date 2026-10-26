#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wramif-rename-migration.XXXXXX")
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
export XDG_DATA_HOME="$TMP/data"
legacy="$XDG_DATA_HOME/recode_all_music_in_folder"
new="$XDG_DATA_HOME/wramif"
mkdir -p "$legacy"
python3 - "$legacy/ramif_database.db" <<'PY'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
c.execute('CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)')
c.execute("INSERT INTO settings(key,value) VALUES('migration_probe','preserved')")
c.commit(); c.close()
PY
# --resume initializes/migrates state and then fails cleanly because this fixture has no prior run.
set +e
"$ROOT/wramif" --resume --no-clipboard >"$TMP/run.log" 2>&1
rc=$?
set -e
[ "$rc" -ne 0 ]
[ -d "$new" ]
[ ! -e "$legacy" ]
[ -f "$new/ramif_database.db" ]
[ -f "$new/wramif.ini" ]
python3 - "$new/ramif_database.db" <<'PY'
import sqlite3, sys
c = sqlite3.connect(sys.argv[1])
assert c.execute("select value from settings where key='migration_probe'").fetchone() == ('preserved',)
c.close()
PY
echo 'WRAMIF legacy data-directory migration test passed.'
