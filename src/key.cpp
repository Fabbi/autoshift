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

#include <key.hpp>
#include <misc/fsettings.hpp>
#include <misc/logger.hpp>

#define SC ShiftCode

SC::ShiftCode(uint32_t m_id, const QString& _d, Platform _p, Game _g,
              const QString& _c, const QString& _e, bool _r):
  _id(m_id), _desc(_d), _platform(_p), _game(_g), _code(_c),
  _redeemed(_r), _expires(_e), _golden(0), dirty(false)
{ updateGolden(); }
SC::ShiftCode(const QString& _d, Platform _p, Game _g, const QString& _c, const QString& _e, bool _r):
  ShiftCode(UINT32_MAX, _d, _p, _g, _c, _e, _r)
{ dirty = true; }
SC::ShiftCode(const QSqlQuery& qry):
  ShiftCode(qry.value("id").toInt(), qry.value("description").toString(),
            tPlatform(qry.value("platform").toString().toStdString()),
            tGame(qry.value("game").toString().toStdString()),
            qry.value("key").toString(), qry.value("expires").toString(),
            qry.value("redeemed").toBool())
{}
SC::ShiftCode():
  _id(UINT32_MAX), _redeemed(false), dirty(false)
{}

bool SC::commit()
{
  bool new_code = (id() == UINT32_MAX);

  // nothing to do here..
  if (!new_code && !dirty) return true;

  QSqlQuery qry;
  if (new_code) {
    // insert new key
    qry.prepare("INSERT INTO keys(description, key, platform, game, redeemed, expires) "
                "VAlUES (:d,:k,:p,:g,:r,:e)");
  } else {
    // update key
    qry.prepare("UPDATE keys SET description=(:d), key=(:k), redeemed=(:r), "
                "platform=(:p), game=(:g), expires=(:e)"
                "WHERE id=(:id)");
    qry.bindValue(":id", id());
  }

  qry.bindValue(":p", QString(sPlatform(platform()).c_str()));
  qry.bindValue(":g", QString(sGame(game()).c_str()));
  qry.bindValue(":d", desc());
  qry.bindValue(":k", code());
  qry.bindValue(":r", redeemed());
  qry.bindValue(":e", expires());

  bool ret = qry.exec();

  if (new_code) {
    QSqlQuery idQry("SELECT last_insert_rowid() as id;");
    idQry.next();
    _id = idQry.value("id").toInt();
  }

  dirty = false;
  return ret;
}
#undef SC

#define SCOL ShiftCollection
SCOL::ShiftCollection(Platform platform, Game game, bool used)
{
  query(platform, game, used);
}

void SCOL::query(Platform platform, Game game, bool used)
{
  QString qry_s("SELECT * FROM keys "
                "WHERE platform=(:p) "
                "AND game=(:g)");
  if (!used)
    qry_s += " AND redeemed=0";

  QSqlQuery qry;
  qry.prepare(qry_s);

  qry.bindValue(":p", QString(sPlatform(platform).c_str()));
  qry.bindValue(":g", QString(sGame(game).c_str()));

  qry.exec();

  while (qry.next()) {
    QString code = qry.value("key").toString();
    QString code_id = code + QString(sPlatform(platform).c_str()) + QString(sGame(game).c_str());
    if (codeMap.contains(code_id)) continue;

    push_back({qry});
    ShiftCode* last = &back();
    codeMap.insert(code_id, last);
  }
}

void SCOL::append(const ShiftCode& code)
{
  QString code_id = code.code() + QString(sPlatform(code.platform()).c_str())
    + QString(sGame(code.game()).c_str());
  // append only if not there yet
  if (codeMap.contains(code_id)) return;

  // DEBUG << "NEW  " << code << endl;
  QList<ShiftCode>::append(code);

  codeMap.insert(code_id, &back());
}

void SCOL::commit()
{
  for (ShiftCode& code: *this) {
    code.commit();
  }
}
ShiftCollection SCOL::filter(CodePredicate pred)
{
  ShiftCollection ret;
  for (auto& key: *this) {
    if (pred(key))
      ret.push_back(key);
  }
  return ret;
}
#undef SCOL
