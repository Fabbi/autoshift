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

#include <logger.hpp>

#include "shift.hpp"

#define SC ShiftClient

const QUrl baseUrl("https://shift.gearboxsoftware.com");
const QString cookieFile(".cookie.sav");

SC::ShiftClient(QObject* parent)
  :QObject(parent)
{
  connect(&network_manager, &QNetworkAccessManager::authenticationRequired,
          this, &SC::slotAuthenticationRequired);
  connect(&network_manager, &QNetworkAccessManager::sslErrors,
          this, &SC::sslErrors);

}
SC::~ShiftClient()
{}

Status SC::redeem(const QString& code)
{
  // TODO implement
}

void SC::save_cookie()
{
  // write cookie
  QList<QNetworkCookie> cookies = network_manager.cookieJar()->cookiesForUrl(baseUrl);
  QByteArray data;
  for (auto& cookie: cookies) {
    DEBUG << "COOKIE " << cookie.name().toStdString() << endl;
    if (cookie.name().toStdString() == "si") {
      data = cookie.toRawForm();
      break;
    }
  }
  // no cookie found
  if (data.isEmpty()) return;
  DEBUG << "FOUND COOKIE" << endl;

  QFile cookie_f(cookieFile);
  if (!cookie_f.open(QIODevice::WriteOnly))
    return;

  int data_size = data.size();
  cookie_f.write((char*)&data_size, sizeof(int));
  cookie_f.write(data);

  cookie_f.close();
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
  // TODO set them

  // read name
  // in.readRawData((char*)&data_size, size_of(data_size));
  // char* data = new char[data_size];
  // in.readRawData(data, data_size);
  // QByteArray name(data, data_size);

  // // read value
  // in.readRawData((char*)&data_size, size_of(data_size));
  // in.readRawData(data, data_size);
  // QByteArray value(data, data_size);

  // delete[] data;

  // QNetworkCookie cookie(name, value);
  // QByteArray name;
  // QDataStream nameS(&name, QIODevice::WriteOnly);
  // nameS.writeRawData(in.dat)
  // QByteArray value;

  cookie_f.close();
}

void findNext(QXmlStreamReader& xml, const QString& token)
{
  while (!xml.atEnd())
    if (xml.readNextStartElement() && xml.name() == token) return;
}

template<typename FUNC>
void SC::getToken(const QUrl& url, FUNC& callback)
{
  current_token = "";
  current_data.clear();
  current_url = url;
  current_status.reset();

  // get request
  reply = network_manager.get(QNetworkRequest(url));

  // read all data
  connect(reply, &QIODevice::readyRead, [&]() {
    current_data.append(reply->readAll());
  });

  // extract token
  connect(reply, &QNetworkReply::finished, this,
          [&, callback]() {
      bool found = false;

      if (reply->error()) {
        current_status.code = Status::UNKNOWN;
        current_status.message = reply->errorString();

      } else {
        QXmlStreamReader xml(current_data);
        while (!found) {
          findNext(xml, "meta");
          if (xml.atEnd()) break;

          bool is_token = false;
          for (auto& attr: xml.attributes()) {
            if (is_token) {
              current_token = attr.value().toString();
              found = true;
              break;
            }
            is_token = (attr.value() == "csrf-token");
          }
        }
      }

      // delete reply object later
      reply->deleteLater();
      reply = nullptr;

      if (found)
        DEBUG << "FOUND TOKEN: " << current_token << endl;

      // QList<QNetworkCookie> cookies = network_manager.cookieJar()->cookiesForUrl(baseUrl);
      // for (auto& cookie: cookies) {
      //   DEBUG << cookie.name().toStdString() << " " << cookie.domain() << endl;
      // }
      callback(current_token);
    });

}

void SC::login(const QString& user, const QString& pw)
{
  QUrl the_url = baseUrl.resolved(QUrl("/home"));

  auto callback = [this, the_url, user, pw](const QString& token) {
    // setup post data
    QUrlQuery postData;
    postData.addQueryItem("authenticity_token", token);
    postData.addQueryItem("user[email]", user);
    postData.addQueryItem("user[password]", pw);

    // setup request
    QNetworkRequest request(baseUrl.resolved(QUrl("/sessions")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    request.setRawHeader("Referer", the_url.toEncoded());

    current_data.clear();
    // post
    DEBUG << "login post" << endl;
    reply = network_manager.post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    // read all data
    connect(reply, &QIODevice::readyRead, this, [&]() {
      DEBUG << "new login post data" << endl;
      current_data.append(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [&]() {
      DEBUG << "============= LOGIN ============" << endl;
      DEBUG << "\n" << current_data.toStdString() << endl;
      const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
      reply->deleteLater();
      reply = nullptr;

      // automatic redirection following does not work....
      if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = redirectionTarget.toUrl();
        DEBUG << "redirect to " << redirectedUrl.toString() << endl;

        reply = network_manager.get(QNetworkRequest(redirectedUrl));
        connect(reply, &QNetworkReply::finished, this, [&]() {
          reply->deleteLater();
          reply = nullptr;
          save_cookie();
        });
      }


    });
  };
  // down the JS-style rabbit hole.. :(
  getToken(the_url, callback);
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
