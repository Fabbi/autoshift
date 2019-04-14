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

/** @class CodeParser
 * @brief A parser for SHiFT codes
 *
 * This class queries and parses various sources for SHiFT codes
 */
// class ControlWindow;

class CodeParser
{
public:
  typedef std::function<void(bool)> Callback;

  CodeParser()
  {}
  CodeParser(ControlWindow& cw, Game _g, Platform _p, const QIcon& _i = QIcon()):
    CodeParser(cw, {_g, Game::NONE}, {_p, Platform::NONE}, {_i})
  {}

  // CodeParser(ControlWindow& cw, Game _gs[], Platform _ps[], const QIcon& _i = QIcon()):
  CodeParser(ControlWindow& cw, std::initializer_list<Game> _gs,
             std::initializer_list<Platform> _ps, std::initializer_list<QIcon> _i):
    CodeParser()
  {
    // register this parser
    auto icon_it = _i.begin();

    for (Game _g: _gs) {
      if (_g == Game::NONE) continue;
      QIcon icn;
      if (icon_it != _i.end()) {
        icn = *icon_it;
        ++icon_it;
      }
      for (Platform _p: _ps) {
        if (_p == Platform::NONE) continue;
        cw.registerParser(_g, _p, this, icn);
      }
    }
  }

  /**
   * Parse keys and add them to the passed ShiftCollection.
   *
   * @param coll Output variable to contain the shift codes after parsing
   * @param cb Callback to trigger after parsing is done (receives bool success-value)
   */
  virtual void parseKeys(ShiftCollection& /* out */, Callback=0) = 0;

protected:
};

/** @class BLnBLPSParser
 * BL2 and BLPS code Parser
 */
class BLnBLPSParser: public CodeParser
{
public:
  BLnBLPSParser(ControlWindow&, Game);
  void parseKeys(ShiftCollection&, CodeParser::Callback=0);
private:
  Game game;
  const QUrl& url;
protected:
  ShiftCollection collections[3];
  QDateTime last_parsed;
};

class BL1Parser: public BLnBLPSParser
{
public:
  BL1Parser(ControlWindow& cw):
    BLnBLPSParser(cw, Game::BL1)
  {}

protected:
  ShiftCollection collections[3];
  QDateTime last_parsed;
};

class BL2Parser: public BLnBLPSParser
{
public:
  BL2Parser(ControlWindow& cw):
    BLnBLPSParser(cw, Game::BL2)
  {}

protected:
  ShiftCollection collections[3];
  QDateTime last_parsed;
};

class BLPSParser: public BLnBLPSParser
{
public:
  BLPSParser(ControlWindow& cw):
    BLnBLPSParser(cw, Game::BLPS)
  {}
protected:
  ShiftCollection collections[3];
  QDateTime last_parsed;
};
