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

from typing import TYPE_CHECKING, Any

from peewee import (
    AutoField,
    BooleanField,
    Model,
    SqliteDatabase,
    TimestampField,
)
from peewee import (
    CharField as PCharField,
)

from src.common import Game, Platform, settings

database = SqliteDatabase(settings.DB_FILE)


if TYPE_CHECKING:

    def CharField(max_length: int = ..., *args, **kwargs) -> Any: ...
else:
    CharField = PCharField


class BaseModel(Model):
    class Meta:
        database = database


class Key(BaseModel):
    """Model for SHiFT keys."""

    id = AutoField()
    reward: str = CharField(default="")
    code: str = CharField()
    platform: Platform = CharField(choices=list(Platform))
    game: Game = CharField(choices=list(Game))
    redeemed = BooleanField(default=False)
    expires = TimestampField(utc=True, null=True, default=None)

    class Meta:  # pyright: ignore[reportIncompatibleVariableOverride]
        table_name = "keys"
        indexes = (
            # Unique constraint on code, platform, game combination
            (("code", "platform", "game"), True),
        )

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

    def copy(self) -> "Key":
        """Create a copy of this key with a new instance."""
        return Key(
            reward=self.reward,
            code=self.code,
            platform=self.platform,
            game=self.game,
            redeemed=self.redeemed,
        )

    def set(self, **kwargs) -> "Key":
        """Update attributes and return self for chaining."""
        for key, value in kwargs.items():
            setattr(self, key, value)
        return self


def create_tables():
    """Create database tables if they don't exist."""
    database.create_tables([Key], safe=True)


"""Initialize the database connection and create tables."""
database.connect()
create_tables()
