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
   * @param redeemed is this code already redeemed?
   */
  ShiftCode(uint32_t, const QString&, Platform, Game, const QString&, bool=false);

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
   * @param redeemed is this code already redeemed?
   */
  ShiftCode(const QString&, Platform, Game, const QString&, bool=false);

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

  friend ashift::Logger& operator<<(ashift::Logger& l, const ShiftCode& c)
  {
    l << "Key{" << JOIN(", ");
    l << c.id << c.desc << c.code << c.redeemed;
    l << JOINE << "}";
    return l;
  }
private:

public:
  QString desc;  ///< Reward description
  QString code;  ///< Code to redeem
  bool redeemed; ///< Is this key already redeemed?

  Platform platform;
  Game game;

private:
  uint32_t id; ///< ID of this entry


  // date expires at?
};



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

public:

  ShiftCollection();
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

private:
};
