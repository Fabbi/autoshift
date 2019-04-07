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

#include <misc/macros.hpp>
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
      UNAVAILABLE,
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

  // typedef std::function<void(Request*, StatusC)> Callback;
  // typedef std::function<void(Request*)> ReqCallback;
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
  StatusC getToken(const QUrl&);

  /**
   * Extract CSRF Token from page content
   *
   * @param data The page content
   */
  StatusC getToken(const QString&);
  /**
   * Get Reward list
   */
  // QStringList queryRewards();

private slots:
  void delete_cookies();
  bool save_cookie();
  bool load_cookie();


  void logout();
  /**
   * Login at shift.gearboxsoftware.com
   *
   * @param user_name The Username / E-Mail address
   * @param pw The Password
   */
  void login(const QString&, const QString&);

  /**
   * Get POST data for code redemption
   *
   * @param code The SHiFT code
   *
   * @return QUrlQuery with POST data.
   */
  StatusC getRedemptionData(const QString&);

  /**
   * Extract Name/Value-Pairs from HTML <form>.
   *
   * @param url The url to query
   *
   * @return Status
   */
  StatusC getFormData(const QUrl&);
  /**
   * Extract Name/Value-Pairs from HTML <form>.
   *
   * @param data The page content
   *
   * @return Status
   */
  StatusC getFormData(const QString&);

  /**
   * Extract Alert message from HTML
   *
   * @param content The page content
   *
   * @return Status
   */
  StatusC getAlert(const QString&);

  /**
   * Query redemption status
   *
   * @param content The page content
   *
   * @return Status
   */
  StatusC getStatus(const QString&);

  /**
   * Check redemption status (redirect, alert, status)
   *
   * @param req The Request object
   *
   * @return Status
   */
  StatusC checkRedemptionStatus(const Request&);

  /**
   * Redeem formdata
   *
   * @param data PostData
   *
   * @return Status
   */
  StatusC redeemForm(const QUrlQuery&);

private:
  bool logged_in; ///< are we logged in?

  QNetworkAccessManager network_manager;
  // QStringList old_rewards;

public slots:
  /**
   * Login via Cookie or data-prompt
   */
  void login();

signals:
  void loggedin(bool);
};
