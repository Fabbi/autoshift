/*****************************************************************************
 **
 ** Copyright (C) 2019 Fabian Schweinfurth
 ** Contact: autoshift <at> derfabbi.de
 **
 ** This file is part of autoshift
 **
 ** autoshift is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU Lesser General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** autoshift is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public License
 ** along with autoshift.  If not, see <http://www.gnu.org/licenses/>.
 **
 *****************************************************************************/
#pragma once

#include <key.hpp>
#include <QUrl>

#include <misc/fsettings.hpp>
#include <controlwindow.hpp>

#include "parser.hpp"

const QMap<Game, QString> gameMapping{
  {Game::BL1, "borderlands"},
  {Game::BLPS, "presequel"},
  {Game::BL2, "borderlands2"},
  {Game::BL3, "borderlands3"},
};
const QMap<QString, Platform> platformMapping{
  {"epic", Platform::EPIC},
  {"steam", Platform::STEAM},
  {"playstation", Platform::PS},
  {"xbox", Platform::XBOX}
};
const QString tUrl{ "https://shift.orcicorn.com/tags/%1/index.json" };


// https://shift.orcicorn.com/tags/borderlands3/index.xml
template<Game GAME>
class OrcicornParser: public CodeParser
{
public:
  using CodeParser::CodeParser;
  OrcicornParser(ControlWindow& cw):
    CodeParser(cw, { GAME },
                   { Platform::EPIC, Platform::STEAM, Platform::PS, Platform::XBOX }, {}),
    url{tUrl.arg(gameMapping.value(GAME))}
  {}
  void parseKeys(ShiftCollection&, CodeParser::Callback = 0);
protected:
  Game game = GAME;
  const QUrl url;
  ShiftCollection collections[Platform::SIZE];
  QDateTime last_parsed;
};

namespace orcicorn {
typedef OrcicornParser<Game::BL1> BL1Parser;
typedef OrcicornParser<Game::BLPS> BLPSParser;
typedef OrcicornParser<Game::BL2> BL2Parser;
typedef OrcicornParser<Game::BL3> BL3Parser;
}

