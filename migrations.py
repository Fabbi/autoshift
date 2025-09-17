import os
import re
import sqlite3
from common import DIRNAME, _L

DB_PATH = os.path.join(DIRNAME, "data", "keys.db")
CODE_PATTERN = re.compile(r"^[A-Za-z0-9]{5}(?:-[A-Za-z0-9]{5}){4}$")


def migrate_shift_codes():
    if not os.path.exists(DB_PATH):
        return
    conn = sqlite3.connect(DB_PATH)
    try:
        c = conn.cursor()
        # Remove codes not matching the pattern
        c.execute("SELECT id, code FROM keys")
        rows = c.fetchall()
        to_remove = [
            (row[0], row[1]) for row in rows if not CODE_PATTERN.match(str(row[1]))
        ]
        if to_remove:
            c.executemany("DELETE FROM keys WHERE id = ?", [(i,) for i, _ in to_remove])
            conn.commit()
            removed_codes = [code for _, code in to_remove]
            _L.info(
                f"Migration: Removed {len(removed_codes)} invalid shift codes from database."
            )
            for code in removed_codes:
                _L.info(f"Removed code: {code}")
    finally:
        conn.close()
