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

from enum import Enum, StrEnum, auto
from typing import TYPE_CHECKING, Any, ClassVar, override

from peewee import (
    AutoField,
    IntegerField,
    Metadata,
    Model,
    TimestampField,
)
from peewee import (
    BooleanField as PBooleanField,
)
from peewee import (
    CharField as PCharField,
)

from autoshift.storage import database

if TYPE_CHECKING:

    def CharField(max_length: int = ..., *args, **kwargs) -> Any: ...
    def BooleanField(*args, **kwargs) -> Any: ...

    EnumField = CharField
else:
    CharField = PCharField
    BooleanField = PBooleanField

    class EnumField(PCharField):
        def __set_name__(self, cls, value: str) -> None:
            self.enum_class = cls.__annotations__[value]

        def db_value(self, value: Enum | None) -> Any:
            if value is None:
                return None

            return value.value

        def python_value(self, value: Any) -> Enum | None:
            if value is None:
                return None
            return self.enum_class(value)


class AltEnum(StrEnum):
    def __init__(self, name: str, *json_names: str):
        super().__init__()
        self.long_name = json_names[0] if json_names else name

    def __new__(cls, value: str, *json_names: str):
        obj = str.__new__(cls, value)
        obj._value_ = value
        obj.long_name = json_names[0] if json_names else value
        for alias in json_names:
            cls._value2member_map_[alias.lower()] = obj
        return obj

    @classmethod
    def _missing_(cls, value):
        return cls._value2member_map_.get(str(value).lower())


class Game(AltEnum):
    bl1 = auto(), "Borderlands: Game of the Year Edition", "Borderlands 1"
    bl2 = auto(), "Borderlands 2"
    bl3 = auto(), "Borderlands 3"
    bl4 = auto(), "Borderlands 4"
    blps = auto(), "Borderlands The Pre-Sequel"
    ttw = auto(), "Tiny Tina's Wonderlands"
    gdfll = auto(), "Godfall"
    UNKNOWN = auto(), "Unknown"


class Platform(AltEnum):
    steam = auto()
    epic = auto()
    psn = auto(), "playstation"
    xboxlive = auto(), "xbox"


class BaseModel(Model):
    _meta: ClassVar[Metadata]

    class Meta:
        database = database


class Key(BaseModel):
    """Model for SHiFT keys."""

    id = AutoField()
    code: str = CharField()
    game: Game = EnumField(choices=list(Game))
    platform: Platform = EnumField(choices=list(Platform))
    reward: str = CharField(default="")
    num_golden = IntegerField(null=True, default=None)
    expires = TimestampField(utc=True, null=True, default=None)
    expired: bool = BooleanField(default=False)
    redeemed: bool = BooleanField(default=False)

    class Meta:  # pyright: ignore[reportIncompatibleVariableOverride]
        table_name = "keys"
        indexes = (
            # Unique constraint on code, platform, game combination
            (("code", "platform", "game"), True),
        )

    def copy(self) -> "Key":
        """Create a copy of this key with a new instance."""
        return Key(
            **{k: getattr(self, k) for k in self._meta.sorted_field_names if k != "id"},
        )

    def set(self, **kwargs) -> "Key":
        """Update attributes and return self for chaining."""
        for key, value in kwargs.items():
            setattr(self, key, value)
        return self

    @override
    def __setattr__(self, name: str, value: Any, /) -> None:
        if name == "game" and isinstance(value, str):
            value = Game(value)
        elif name == "platform" and isinstance(value, str):
            value = Platform(value)
        return super().__setattr__(name, value)

    def __repr__(self) -> str:
        return f"<Key game={self.game} platform={self.platform} code={self.code} redeemed={self.redeemed} reward={self.reward}>"
