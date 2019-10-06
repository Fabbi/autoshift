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

#include <QString>
#include <QSqlQuery>
#include <QList>
#include <QRegularExpression>

#include <misc/macros.hpp>
#include <misc/logger.hpp>

FENUM(Platform,
      PC,
      PS,
      XBOX);

FENUM(Game,
      BL1,
      BL2,
      BLPS,
      BL3);

const QRegularExpression rGold("([0-9]+)[^0-9]*?gold", QRegularExpression::CaseInsensitiveOption);

const QString CREATE_TABLE("CREATE TABLE IF NOT EXISTS keys "
                           "(id INTEGER primary key, description TEXT,"
                           "key TEXT, platform TEXT, game TEXT, "
                           "redeemed INTEGER, expires Text, source TEXT, "
                           "note TEXT)");

/** @class ShiftCode
 * @brief SHiFT code representation
 *
 * This class has two main tasks:
 * 1. Holding information of available SHiFT codes to redeem
 * 2. Sending and retrieving these codes to/from the database.
 */
class ShiftCode // : Q??
{

public:
  /**
   * Construct a SHiFT code.
   *
   * @sa ShiftCode(const QString&, Platform, Game, const QString&, bool)
   * @sa ShiftCode(const QSqlQuery&)
   *
   * @param id The ID
   * @param description The code description (N golden keys / XY skin ...)
   * @param platform The platform
   * @param game The game
   * @param code SHiFT code (AAAAA-BBBBB-CCCCC-EEEEE-FFFFF)
   * @param expires String containing expiration information
   * @param redeemed is this code already redeemed?
   */
  ShiftCode(uint32_t, const QString&, Platform, Game, const QString&, const QString&, 
            const QString&, const QString&, bool=false);

  /**
   * Construct a SHiFT code. The ID will be set on pushing to database.
   *
   * @sa ShiftCode(uint32_t, const QString&, Platform, Game, const QString&, bool)
   * @sa ShiftCode(const QSqlQuery&)
   *
   * @param description The code description (N golden keys / XY skin ...)
   * @param platform The platform
   * @param game The game
   * @param code SHiFT code (AAAAA-BBBBB-CCCCC-EEEEE-FFFFF)
   * @param expires String containing expiration information
   * @param redeemed is this code already redeemed?
   */
  ShiftCode(const QString&, Platform, Game, const QString&, const QString&,
            const QString&, const QString&, bool=false);

  /**
   * Construct a SHiFT code
   *
   * @sa ShiftCode(uint32_t, const QString&, Platform, Game, const QString&, bool)
   * @sa ShiftCode(const QString&, Platform, Game, const QString&, bool)
   *
   * @param query SQL Query object with data from database
   */
  ShiftCode(const QSqlQuery&);

  /**
   * Construct an empty SHiFT code for database querying
   *
   * @sa ShiftCode(uint32_t, const QString&, Platform, Game, const QString&, bool)
   * @sa ShiftCode(const QString&, Platform, Game, const QString&, bool)
   */
  ShiftCode();

  // /**
  //  * @brief Query for a key in database.
  //  *
  //  * This method overwrites any field values that might have been
  //  * manually set.
  //  *
  //  * @param id The ID to search for
  //  *
  //  * @return Whether the query was successful or not
  //  */
  // bool query(uint32_t);

  /**
   * @brief Push this code to the database
   *
   * Push this code to database. Update already present keys.
   * This will also set the id if it is a new key.
   *
   * @return Whether the action was successful or not
   */
  bool commit();


  inline void setDesc(const QString& _d)
  {
    _desc = _d;
    dirty = true;

    updateGolden();
  }
  inline void setCode(const QString& _c)
  {
    _code = _c;
    dirty = true;
  }
  inline void setRedeemed(bool v=true)
  {
    _redeemed = v;
    dirty = true;
  }

  inline uint32_t id() const
  { return _id; }

  inline const QString& desc() const
  { return _desc; }
  inline const QString& code() const
  { return _code; }

  inline Platform platform() const
  { return _platform; }

  inline Game game() const
  { return _game; }

  inline bool redeemed() const
  { return _redeemed; }

  inline const QString& expires() const
  { return _expires; }

  inline const QString& note() const
  { return _note; } 

  inline const QString& source() const
  { return _source; }

  inline uint8_t golden() const
  { return _golden; }

private:
  void updateGolden()
  {
    _golden = 0;
    auto match = rGold.match(desc());

    if (match.hasMatch()) {
      _golden = match.captured(1).toInt();
    }
  }

private:
  QString _desc;  ///< Reward description
  QString _code;  ///< Code to redeem
  bool _redeemed; ///< Is this key already redeemed?
  QString _expires;   ///< (When) does this code expire?
  QString _note;  ///< additional info
  QString _source; ///< source information

  Platform _platform;
  Game _game;
  uint8_t _golden; ///< number of golden Keys as reward (if any)

  uint32_t _id; ///< ID of this entry

  bool dirty;

  // date expires at?
};

inline ashift::Logger& operator<<(ashift::Logger& l, const ShiftCode& c)
{
  l << "Key{" << JOIN(", ");
  l << c.id() << c.desc() << c.code() << c.redeemed() << c.expires();
  l << c.note() << c.source();
  l << JOINE << "}";
  return l;
}


/**
 * Predicate function to filter keys.
 *
 * Must return `bool` and receives a `ShiftCode` reference.
 */
typedef std::function<bool(const ShiftCode&)> CodePredicate;

/** @class ShiftCollection
 * @brief A collection of ShiftCode objects
 *
 * A Collection holds many ShiftCodes and can be used to query or write to
 * a database.
 */

class ShiftCollection: public QList<ShiftCode>
{
  static const uint8_t database_version = 2;

public:

  ShiftCollection()
  {}
  /**
   * @brief Construct a ShiftCollection
   *
   * Will query the database for codes matching given platform
   *
   * @param platform The platform to query for
   * @param game The game
   * @param used Whether to query for already redeemed keys, too
   */
  ShiftCollection(Platform, Game, bool=false);

  /**
   * @brief Query codes from Database.
   *
   * This method fills the collection, appending to already presend keys
   *
   * @sa ShiftCollection(Platform, bool)
   *
   * @param platform The platform to query for
   * @param game The game
   * @param used Whether to query for already redeemed keys, too
   */
  void query(Platform, Game, bool=false);

  /**
   * Commit all codes to database
   *
   * @sa ShiftCode::commit()
   */
  void commit();

  /**
   * Filter codes by given predicate
   *
   * @param pred A lambda function to filter keys.
   *
   * @return A new ShiftCollection
   */
  ShiftCollection filter(CodePredicate);

  inline void clear()
  {
    QList<ShiftCode>::clear();
    codeMap.clear();
  }
  void push_back(const ShiftCode& _c)
  {append(_c);}
  void append(const ShiftCode&);

  void update_database();
  QList<ShiftCode>& operator+=(const QList<ShiftCode> &other)
  { for (auto& el: other) {append(el);}; return *this; }
  QList<ShiftCode>& operator<<(const QList<ShiftCode> &other)
  { return operator+=(other); }

private:
  QMap<QString, ShiftCode*> codeMap;
};
