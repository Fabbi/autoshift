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
import logging
import logging.config
import re
from collections import defaultdict
from enum import StrEnum, auto
from pathlib import Path
from typing import Annotated, Any, Literal, override

import rich.logging
from pydantic import BeforeValidator, Field, PrivateAttr, SecretStr
from pydantic_core import PydanticUndefined
from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict

ROOT_DIR = Path(__file__).parent.parent


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


def path(v: str):
    """BeforeValidator doesn't work with `Path` directly"""
    return v and Path(v)


def validate_list(value):
    if not value:
        return []
    if isinstance(value, str):
        return list(filter(len, re.split(r"\W+", value)))
    return value


class SettingsFields(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="SHIFT_",
        extra="allow",
        validate_default=True,
    )

    PLATFORMS: Annotated[
        list[Platform],
        Field(
            description="Platforms you want to redeem SHiFT keys for",
        ),
        BeforeValidator(validate_list),
        NoDecode,
    ] = []

    GAMES: Annotated[
        list[Game],
        Field(
            description="Games you want to redeem SHiFT keys for",
        ),
        BeforeValidator(validate_list),
        NoDecode,
    ] = []

    _GAMES_PLATFORM_MAP: dict[Game, set[Platform]] = PrivateAttr(
        init=False, default_factory=lambda: defaultdict(set)
    )

    USER: str | None = Field(
        default=None,
        description="E-Mail for your login\n  (only needed for the first login & will be prompted if missing)",
    )
    PASS: SecretStr | None = Field(
        default=None,
        description="Password for your login\n  (only needed for the first login & will be prompted if missing)",
    )

    DATA_DIR: Annotated[Path, BeforeValidator(path)] = Field(
        default=ROOT_DIR / "data", description="Path to the data directory"
    )

    COOKIE_FILE: Annotated[Path, BeforeValidator(path)] = Field(
        default_factory=lambda data: (data["DATA_DIR"] / ".cookies.save"),
        description="Path to the cookie file (will be created if it doesn't exist)",
    )
    DB_FILE: Annotated[Path, BeforeValidator(path)] = Field(
        default_factory=lambda data: (data["DATA_DIR"] / "keys.db"),
        description="Path to the database file (will be created if it doesn't exist)",
    )

    LOG_LEVEL: Literal["DEBUG", "INFO", "WARNING", "ERROR"] = "WARNING"
    HTTP_LOG_LEVEL: Literal["DEBUG", "INFO", "WARNING", "ERROR"] = "WARNING"

    SCHEDULE: int | None = Field(
        default=120, description="Check for new keys every N minutes"
    )

    LIMIT: int = Field(
        default=255,
        gt=1,
        description="Maximum number of keys to redeem at once (GearBox caps at 255)",
    )

    SHIFT_SOURCE: str | None = Field(
        default="https://raw.githubusercontent.com/ugoogalizer/autoshift-codes/main/shiftcodes.json",
        description="""Can be a URL or a local file path (absolute or relative to the root dir)
                      |  Set this to `None` to disable querying new keys.""",
    )

    @staticmethod
    def write_defaults_file():
        with (ROOT_DIR / "env.default").open("w") as f:
            f.write("# -*- mode: sh -*-\n\n")
            default_settings = SettingsFields().model_dump()

            default_settings["COOKIE_FILE"] = "${SHIFT_DATA_DIR}/.cookies.save"
            default_settings["DB_FILE"] = "${SHIFT_DATA_DIR}/keys.db"

            indent_re = re.compile(r"^\s+\|")

            f.write("# There's a new way to specify game/platform combinations!\n")
            f.write("# just add an entry with the game name and a list of platforms\n")
            f.write("# supported games:\n")
            f.write(f"#{' ' * 4}{', '.join(Game)}\n")
            f.write("# supported platforms: \n")
            f.write(f"#{' ' * 4}{', '.join(Platform)}\n\n")

            for game in Game:
                f.write(f"# SHIFT_{game.upper()}=steam,psn\n")

            f.write("\n\n")
            for key, field_info in SettingsFields.model_fields.items():
                desc = field_info.description or ""
                desc_lines = desc.split("\n")
                for line in desc_lines:
                    line = indent_re.sub("", line)
                    f.write(f"# {line}\n")

                f.write(f"SHIFT_{key}=")

                default = default_settings.get(key)
                if default and default is not PydanticUndefined:
                    if isinstance(default, list):
                        default = ",".join(str(x) for x in default)
                    f.write(f"{default}  # default: {default}")
                f.write("\n\n")


class Settings(SettingsFields):
    """Read Settings from different sources: (in order of precedence)
    - Command line args
    - ENV variables
    - .env file
    """

    model_config = SettingsConfigDict(
        env_file=(ROOT_DIR / ".env"),
        env_prefix="SHIFT_",
        env_file_encoding="utf-8",
        env_ignore_empty=True,
        env_parse_none_str="None",
        extra="allow",
    )

    @override
    def model_post_init(self, context: Any, /) -> None:
        super().model_post_init(context)
        self.COOKIE_FILE.parent.mkdir(parents=True, exist_ok=True)
        self.DB_FILE.parent.mkdir(parents=True, exist_ok=True)

        if self.SHIFT_SOURCE and not self.SHIFT_SOURCE.startswith("http"):
            path = Path(self.SHIFT_SOURCE)
            if not path.is_absolute():
                path = ROOT_DIR / path
            self.SHIFT_SOURCE = str(path.resolve())

        handler = rich.logging.RichHandler(show_path=False)
        logging.getLogger("autoshift").addHandler(handler)

        for game in self.GAMES:
            self._GAMES_PLATFORM_MAP[game] = set(self.PLATFORMS)

        if not self.model_extra:
            return

        for game in Game:
            if platforms := self.model_extra.get(f"shift_{game.name.lower()}"):
                platforms = validate_list(platforms)
                self._GAMES_PLATFORM_MAP[game] = set(
                    Platform(platform) for platform in platforms
                )

        return


settings = Settings()


logging.config.dictConfig(
    dict(
        version=1,
        disable_existing_loggers=False,
        level=settings.LOG_LEVEL,
        handlers=dict(
            console={
                "class": "rich.logging.RichHandler",
                "rich_tracebacks": True,
                "enable_link_path": False,
            },
        ),
        formatters=dict(
            console=dict(format=("[%(levelname)s] %(name)s | %(message)s")),
        ),
        loggers={
            "autoshift": {
                "handlers": ["console"],
                "propagate": False,
                "level": settings.LOG_LEVEL,
            },
            "httpx": {
                "handlers": ["console"],
                "propagate": False,
                "level": settings.HTTP_LOG_LEVEL,
            },
            "httpcore": {
                "handlers": ["console"],
                "propagate": False,
                "level": settings.HTTP_LOG_LEVEL,
            },
        },
    )
)

_L = logging.getLogger("autoshift")

if __name__ == "__main__":
    SettingsFields.write_defaults_file()
