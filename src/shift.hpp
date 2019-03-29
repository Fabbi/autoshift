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
  void* data;

  StatusC()
    :code(Status::NONE), message(""), data(0)
  {}
  StatusC(Status c, const QString& msg, void* _data = 0)
    :code(c), message(msg), data(_data)
  {}
  void reset() { code = Status::NONE; message = ""; data=0; }
};


class ShiftClient : public QObject
{
  Q_OBJECT

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
  template<typename FUNC> // (bool) -> void
  void getToken(const QUrl&, FUNC&);

private slots:
  void save_cookie();
  bool load_cookie();


  /**
   * Login at shift.gearboxsoftware.com
   *
   * @param user The Username / E-Mail address
   * @param pw The Password
   */
  void login(const QString&, const QString&);

  // void getRedemptionForm(const QString&);

  // void getAlert(??);
  // void getStatus(??);
  // void checkRedemptionStatus(??);
  // void redeemForm(??);

private:
  QUrl current_url;

  QNetworkAccessManager network_manager;
  QNetworkReply* reply;

  QString current_token;
  QByteArray current_data;
  StatusC current_status;
  // QDataStream stream;

public slots:
  void slotAuthenticationRequired(QNetworkReply*, QAuthenticator*);
  void sslErrors(QNetworkReply*, const QList<QSslError>&);

signals:
};
