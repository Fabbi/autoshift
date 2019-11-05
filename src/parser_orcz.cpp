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
#include <parser_orcz.hpp>
#include <request.hpp>

#include <misc/fsettings.hpp>
#include <misc/macros.hpp>



const QRegularExpression rTable("(<table.*?>.*?</table>)",
                               QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rRow("<tr.*?>(.*?)</tr>",
                                QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rCol("<td.*?>(.*?)</td>",
                                QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rCode("((?:\\w{5}-){4}\\w{5})",
                               QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rTag("(<.*?>)",
                              QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rExp("<span[^>]*red",
                              QRegularExpression::DotMatchesEverythingOption|
                              QRegularExpression::CaseInsensitiveOption);
const QRegularExpression rThru("\\((thru.*?)\\)",
                               QRegularExpression::DotMatchesEverythingOption|
                               QRegularExpression::CaseInsensitiveOption);
#define BL2PS BLnBLPSParser<GAME>

// this parser can handle all platforms
template<Game GAME>
void BL2PS::parseKeys(ShiftCollection& coll, Callback cb)
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

    // extract table
    auto match = rTable.match(req->data);
    if (!match.hasMatch()) {
      IFCB(false);
      return;
    }

    const QString& table = match.captured(1);

    // find all rows
    auto rIt = rRow.globalMatch(table);

    // skip header row
    rIt.next();

    while (rIt.hasNext()) {
      QString row = rIt.next().captured(1);

      auto cIt = rCol.globalMatch(row);
      QString cols[7];
      uint8_t i = 0;
      while (cIt.hasNext() && i < 7) {
        cols[i] = cIt.next().captured(1);
        ++i;
      }

      // remove expired keys
      bool keep = false;
      for (i = 4; i < 7; ++i) {
        if (cols[i].isEmpty()) continue;

        // is this key expired?
        if (rExp.match(cols[i]).hasMatch()) {
          cols[i] = "";
        } else {
          // at least one isn't => keep the row
          keep = true;
          auto kMatch = rCode.match(cols[i]);
          if (!kMatch.hasMatch()) continue;

          cols[i] = kMatch.captured(1).trimmed();
        }
      }

      if (keep) {
        QTextDocument text;
        text.setHtml(cols[0]);
        QString src = QString("Orcz(%1)").arg(text.toPlainText().trimmed());

        text.setHtml(cols[1]);
        QString desc = text.toPlainText().trimmed();

        // extract "expires" information
        QString exp = cols[2].trimmed();
        auto match = rThru.match(exp);
        if (match.hasMatch())
          exp = match.captured(1);
        else
          exp = "";

        // add keys for every platform
        for (i = 4; i < 7; ++i) {
          if (cols[i].isEmpty()) continue;
          collections[i-3].push_back({desc, toPlatform(i-3), GAME, cols[i], exp, src, ""});
        }
      }
    }

    last_parsed = QDateTime::currentDateTime();

    Platform platform = tPlatform(FSETTINGS["platform"].toString().toStdString());
    coll << collections[platform];

    IFCB(true);
    req->deleteLater();
  });
#undef IFCB
}

template BLnBLPSParser<Game::BL1>;
template BLnBLPSParser<Game::BLPS>;
template BLnBLPSParser<Game::BL2>;
template BLnBLPSParser<Game::BL3>;
#undef BL2PS
