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

#include <macros.hpp>
#include <QObject>

#include <QNetworkAccessManager>
#include <QUrl>

class Request;

class QSslError;
class QAuthenticator;
class QNetworkReply;
class QDataStream;

FENUM(Status,
      REDIRECT,
      TRYLATER,
      EXPIRED,
      REDEEMED,
      SUCCESS,
      INVALID,
      UNKNOWN);

/**
 * Status Object with message and optional data
 */
struct StatusC
{
  Status code;
  QString message;
  QVariant data;

  StatusC()
    :code(Status::NONE), message(""), data()
  {}
  StatusC(Status c, const QString& msg, const QVariant& _data)
    :code(c), message(msg), data(_data)
  {}
  StatusC(Status c, const QString& msg)
    :StatusC(c, msg, QVariant())
  {}
  ~StatusC()
  {}
  void reset() { code = Status::NONE; message = ""; data=0; }
};


class ShiftClient : public QObject
{
  Q_OBJECT

  typedef std::function<bool(Request&)> ReqCallback;
public:
  explicit ShiftClient(QObject* parent = 0);
  ~ShiftClient();

  /**
   * Redeem a code
   *
   * @param code The SHiFT code to redeem
   *
   * @return Status of redemption
   */
  Status redeem(const QString&);

private:
  /**
   * Get CSRF Token from given URL
   *
   * @param url The URL to query
   */
  StatusC& getToken(const QUrl&);

private slots:
  bool save_cookie();
  bool load_cookie();


  /**
   * Login at shift.gearboxsoftware.com
   *
   * @param user The Username / E-Mail address
   * @param pw The Password
   */
  void login(const QString&, const QString&);

  /**
   * Get POST data for code redemption
   *
   * @params code The SHiFT code
   *
   * @return QUrlQuery with POST data.
   */
  StatusC getRedemptionData(const QString&);

  StatusC getFormData(const QUrl&, ReqCallback=0, ReqCallback=0);
  // void getAlert(??);
  // void getStatus(??);
  // void checkRedemptionStatus(??);
  // void redeemForm(??);

private:
  bool logged_in;

  QNetworkAccessManager network_manager;
  // QNetworkReply* reply;

  // QString current_token;
  // QByteArray current_data;
  StatusC current_status;
  // QDataStream stream;

public slots:
  /**
   * Login via Cookie or data-prompt
   */
  void login();

signals:
  void loggedin(bool);
};
