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
#include <QtNetwork>

#include <parser_orcicorn.hpp>
#include <request.hpp>

#include <misc/fsettings.hpp>
#include <misc/macros.hpp>

#define ORC OrcicornParser<GAME>

template<Game GAME> 
void ORC::parseKeys(ShiftCollection& coll, Callback cb)
{
  if (last_parsed.isValid()) {
    QDateTime now = QDateTime::currentDateTime();
    // every 5 minutes
    if (last_parsed.secsTo(now) < 300) {
      Platform platform = tPlatform(FSETTINGS["platform"].toString().toStdString());
      coll << collections[platform];

      if (cb)
        cb(true);
      return;
    }
  }

  Request* req = new Request(url);

  req->send([this, req, cb, &coll]() mutable {
#define IFCB(v) if(cb) cb(v);
    if (req->timed_out) {
      IFCB(false);
      return;
    }

    QJsonDocument content = QJsonDocument::fromJson(req->data);
    QJsonArray all_codes = content.array().at(0).toObject()["codes"].toArray();
    DEBUG << all_codes.size() << endl;

    for (QJsonValueRef codeRef : all_codes) {
      QJsonObject codeObj = codeRef.toObject();
      QString _platform = codeObj["platform"].toString().toLower();

      if (_platform == "universal")
        for (uint8_t i = 0; i < Platform::SIZE; ++i) {
          ShiftCode _sCode(codeObj["reward"].toString(),
                           (Platform)i, GAME, codeObj["code"].toString(),
                           codeObj["expires"].toString(), "Orcicorn", "" );
          collections[i].push_back(_sCode);
          //DEBUG << "Pushing UNIVERSAL code " << _sCode << endl;

        } else {
          ShiftCode _sCode(codeObj["reward"].toString(),
                           platformMapping.value(_platform), GAME, codeObj["code"].toString(),
                           codeObj["expires"].toString(), "Orcicorn", "" );
          //DEBUG << "Pushing             code " << _sCode << endl;
          collections[_sCode.platform()].push_back(_sCode);
        }

    }

    last_parsed = QDateTime::currentDateTime();

    Platform platform = tPlatform(FSETTINGS["platform"].toString().toStdString());
    coll << collections[platform];

    IFCB(true);
    req->deleteLater();
#undef IFCB
    });
}

template OrcicornParser<Game::BL1>;
template OrcicornParser<Game::BLPS>;
template OrcicornParser<Game::BL2>;
template OrcicornParser<Game::BL3>;
#undef ORC
