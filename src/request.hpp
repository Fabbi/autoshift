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
#include <QNetworkAccessManager>

/**
 * Wrapper class for QNetworkRequest.
 *
 * It has:
 * - auto follow redirections
 * - auto collection of incoming data
 */

FENUM(request_t,
      GET,
      POST,
      HEAD);

class Request : public QObject
{
  Q_OBJECT

public:
  Request(QNetworkAccessManager& _manager, const QUrl& _url,
          const QUrlQuery& _data, request_t _type = request_t::GET);
  Request(QNetworkAccessManager& _manager, const QUrl& _url,
          request_t _type = request_t::GET);
  ~Request();

  template<typename FUNC>
  void send(FUNC fun)
  {
    // connect to this
    connect(this, &Request::finished, fun);

    send();
  }


  void send();

  void followRedirects(bool v=true)
  { follow_redirects = v; }
public:
  QNetworkAccessManager* manager;
  QUrl url;
  QNetworkReply* reply;

  // StatusC status;
  QByteArray data; ///< reply data
  QByteArray query_data; ///< post data
  int status_code;

  QNetworkRequest req;

private:
  request_t type;

  bool follow_redirects;

signals:
  void finished(QByteArray);
};
