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
#include <QUrl>
#include <QDataStream>

#include <QDialog>

#include <logger.hpp>
#include <macros.hpp>

#include "shift.hpp"
#include "ui_loginwindow.h"

#include "request.hpp"

#define SC ShiftClient

#define REQUEST(args...) Request(network_manager, args)
#define REQ(args...) req(network_manager, args)

const QUrl baseUrl("https://shift.gearboxsoftware.com");
const QString cookieFile(".cookie.sav");

SC::ShiftClient(QObject* parent)
  :QObject(parent), logged_in(false)
{
  // auto login
  if (load_cookie()) {
    DEBUG << "COOKIE LOADED" << endl;
    // Request* req = new Request(network_manager, baseUrl.resolved(QUrl("/account")));
    // req->send([&, req]() {
    //   // std::string str = req->data.toStdString();
    //   // if (str.size() < 200)
    //     DEBUG << "\n" << req->data.toStdString() << endl;
    //   // save_cookie();
    //   req->deleteLater();
    // });
  } else {
    // show login window
    QDialog loginDialog;
    Ui::Login loginWin;
    loginWin.setupUi(&loginDialog);

    // when clicked "Ok"
    if (loginDialog.exec()) {
      QString user = loginWin.userInput->text();
      QString pw = loginWin.pwInput->text();

      INFO << "trying to log in" << endl;
      login(user, pw);
    }
    // DEBUG << "login: " << loginDialog.exec() << endl;
    // DEBUG << loginWin.userInput->text() << endl;
    // DEBUG << "no cookie? :(" << endl;
  }
}

SC::~ShiftClient()
{
  // if (logged_in) {
  //   DEBUG << "logout" << endl;
  //   Request REQ(baseUrl.resolved(QUrl("/logout")));
  //   req.send();
  //   wait(&req, &Request::finished);
  // }
}

Status SC::redeem(const QString& code)
{
  // TODO implement
}

bool SC::save_cookie()
{
  // write cookie
  QList<QNetworkCookie> cookies = network_manager.cookieJar()->cookiesForUrl(baseUrl);
  QByteArray data;
  for (auto& cookie: cookies) {
    if (cookie.name().toStdString() == "si") {
      data = cookie.toRawForm();
      break;
    }
  }
  // no cookie found
  if (data.isEmpty()) return false;
  DEBUG << "FOUND COOKIE" << endl;

  QFile cookie_f(cookieFile);
  if (!cookie_f.open(QIODevice::WriteOnly))
    return false;

  int data_size = data.size();
  cookie_f.write((char*)&data_size, sizeof(int));
  cookie_f.write(data);

  cookie_f.close();

  return true;
}

bool SC::load_cookie()
{
  QFile cookie_f(cookieFile);
  if (!cookie_f.open(QIODevice::ReadOnly))
    return false;

  // read cookie
  QDataStream in(&cookie_f);
  int data_size;
  in.readRawData((char*)&data_size, sizeof(data_size));
  char* data = new char[data_size];
  in.readRawData(data, data_size);
  QByteArray cookieString(data, data_size);
  delete[] data;

  QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(cookieString);

  network_manager.cookieJar()->setCookiesFromUrl(cookies, baseUrl);

  cookie_f.close();
  logged_in = true;
  return true;
}

void findNext(QXmlStreamReader& xml, const QString& token)
{
  while (!xml.atEnd())
    if (xml.readNextStartElement() && xml.name() == token) return;
}

StatusC& SC::getToken(const QUrl& url)
{
  current_status.reset();

  Request req(network_manager, url);
  // get request
  req.send();

  // wait for answer
  if (!wait(&req, &Request::finished)) {
    DEBUG << "timeout" << endl;
    DEBUG << "\n" << req.data.toStdString() << endl;
    current_status.code = Status::UNKNOWN;
    current_status.message = "Timeout on Token Request";
    return current_status;
  }

  // extract token
  bool found = false;

  if (req.reply->error()) {
    current_status.code = Status::UNKNOWN;
    current_status.message = req.reply->errorString();

  } else {
    // extract token from DOM
    QXmlStreamReader xml(req.data);
    while (!found) {
      findNext(xml, "meta");
      if (xml.atEnd()) break;

      bool is_token = false;
      for (auto& attr: xml.attributes()) {
        if (is_token) {
          current_status.message = attr.value().toString();
          found = true;
          break;
        }
        is_token = (attr.value() == "csrf-token");
      }
    }
  }

  if (found) {
    current_status.code = Status::SUCCESS;
  }

  return current_status;
}

void SC::login(const QString& user, const QString& pw)
{
  QUrl the_url = baseUrl.resolved(QUrl("/home"));

  StatusC& s = getToken(the_url);

  if (s.code != Status::SUCCESS) {
    ERROR << "Token Request Error: " << s.message << endl;
    return;
  }

  QString& token(s.message);

  // setup post data
  QUrlQuery postData;
  postData.addQueryItem("authenticity_token", token);
  postData.addQueryItem("user[email]", user);
  postData.addQueryItem("user[password]", pw);

  Request* req = new REQUEST(baseUrl.resolved(QUrl("/sessions")),
                            postData, request_t::POST);
  // setup request
  // QNetworkRequest request(baseUrl.resolved(QUrl("/sessions")));
  req->req.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
  req->req.setRawHeader("Referer", the_url.toEncoded());

  req->followRedirects(true);
  // send request and follow redirects
  req->send([&, req](QByteArray data) mutable {
    DEBUG << "============= LOGIN ============" << endl;
    logged_in = save_cookie();
    req->deleteLater();
  });
}
}
// void SC::getRedemptionForm(const QString&);

// StatusC SC::getAlert(??);
// std::tuple<QString, QString> SC::getStatus(??);
// StatusC SC::checkRedemptionStatus(??);
// StatusC SC::redeemForm(??);

void SC::slotAuthenticationRequired(QNetworkReply*, QAuthenticator* auth)
{

}
void SC::sslErrors(QNetworkReply*, const QList<QSslError>& err)
{

}
#undef SC
#undef REQ
#undef REQUEST
