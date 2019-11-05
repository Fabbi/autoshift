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
#include <QNetworkAccessManager>

#include <misc/fsettings.hpp>
#include <controlwindow.hpp>

#include "parser.hpp"

/** @class CodeParser
 * @brief A parser for SHiFT codes
 *
 * This class queries and parses various sources for SHiFT codes
 */
// class ControlWindow;
const QUrl urls[]{
  /*[Game::BL1]  =*/ {"http://orcz.com/Borderlands:_Golden_Key"},
  /*[Game::BL2]  =*/ {"http://orcz.com/Borderlands_2:_Golden_Key"},
  /*[Game::BLPS] =*/ {"http://orcz.com/Borderlands_Pre-Sequel:_Shift_Codes"},
  /*[Game::BL3]  =*/ {"http://orcz.com/Borderlands_3:_Golden_Key"} };





/** @class BLnBLPSParser
 * BL2 and BLPS code Parser
 */
template<Game GAME>
class BLnBLPSParser: public CodeParser
{
public:
  using CodeParser::CodeParser;
  BLnBLPSParser(ControlWindow &cw): 
    CodeParser(cw, {GAME},
                   {Platform::STEAM, Platform::EPIC, Platform::PS, Platform::XBOX}, {}),
    url(urls[GAME])
  {}
  void parseKeys(ShiftCollection&, CodeParser::Callback=0);
protected:
  const QUrl& url;
  ShiftCollection collections[4];
  QDateTime last_parsed;
};

namespace orcz {
  typedef BLnBLPSParser<Game::BL1> BL1Parser;
  typedef BLnBLPSParser<Game::BLPS> BLPSParser;
  typedef BLnBLPSParser<Game::BL2> BL2Parser;
  typedef BLnBLPSParser<Game::BL3> BL3Parser;
}