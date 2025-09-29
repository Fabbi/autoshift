#############################################################################
#
# Copyright (C) 2018 Fabian Schweinfurth
# Contact: autoshift <at> derfabbi.de
#
# This file is part of autoshift
#
# autoshift is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# autoshift is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with autoshift.  If not, see <http://www.gnu.org/licenses/>.
#
#############################################################################
from collections.abc import Callable
from functools import wraps
from typing import Any

import peewee as pw
from peewee import Context, SqliteDatabase
from playhouse.migrate import SqliteMigrator, migrate, operation

from autoshift.common import _L


class ShiftMigrator(SqliteMigrator):
    @operation
    def execute(self, query):
        ctx: Context = self.make_context()
        return ctx.sql(query)


MigrationFunc = Callable[[ShiftMigrator], Any]
migrationFunctions: list[MigrationFunc] = []


def revision(func: MigrationFunc):
    version = len(migrationFunctions) + 1

    @wraps(func)
    def wrapper(migrator: ShiftMigrator):
        try:
            migrate(*func(migrator))
            migrator.database.user_version = version
        except Exception as e:
            _L.error(
                f"There was an error while migrating database to version {version}.\n"
                "Please contact the developer @ github.com/fabbi"
            )
            raise e

    migrationFunctions.append(wrapper)
    return wrapper


def run_migrations(db: SqliteDatabase):
    current_version = db.user_version
    if current_version == 0:
        from autoshift.models import Key

        # skip the whole migration if the db is new
        # and just create the tables
        db.create_tables([Key], safe=True)
        db.user_version = len(migrationFunctions)
        return

    with db.atomic():
        migrator = ShiftMigrator(db)
        for upgrade in migrationFunctions[current_version:]:
            _L.info(f"Migrating database to version {current_version + 1}")
            upgrade(migrator)
            current_version += 1


############


@revision
def update_1(ops: ShiftMigrator):
    Keys = pw.Table("keys")

    yield ops.rename_column("keys", "description", "reward")
    yield ops.rename_column("keys", "key", "code")
    yield ops.execute(Keys.update(platform="psn").where(Keys.c.platform == "ps"))
    yield ops.execute(Keys.update(platform="xboxlive").where(Keys.c.platform == "xbox"))
    yield ops.execute(Keys.update(game="bl1").where(Keys.c.game == "bl"))
    yield ops.execute(
        Keys.insert(
            Keys.select(
                Keys.c.reward, Keys.c.code, pw.Value("epic"), Keys.c.game, Keys.c.redeemed
            ).where(Keys.c.platform == "pc"),
            [Keys.c.reward, Keys.c.code, Keys.c.platform, Keys.c.game, Keys.c.redeemed],
        )
    )
    yield ops.execute(Keys.update(platform="steam").where(Keys.c.platform == "pc"))


@revision
def update_2(ops: ShiftMigrator):
    ## new columns
    expires = pw.TimestampField(utc=True, null=True, default=None)
    yield ops.add_column("keys", "expires", expires)
    expired = pw.BooleanField(default=False)
    yield ops.add_column("keys", "expired", expired)
    num_golden = pw.IntegerField(null=True, default=None)
    yield ops.add_column("keys", "num_golden", num_golden)

    ## indexes
    yield ops.add_index("keys", ["code"])
    # codes are unique
    yield ops.add_unique("keys", "code", "game", "platform")
