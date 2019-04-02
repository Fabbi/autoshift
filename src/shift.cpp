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

  // if (logged_in) {
  //   DEBUG << "LOGGED IN; TRYING TO REDEEM TEST CODE" << endl;
  //   getRedemptionData("W55J3-BXJ9R-SBRKJ-STWJ3-F6C99");
  // }
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

  // test
  Request REQ(baseUrl.resolved(QUrl("/account")), request_t::HEAD);
  req.send();
  if (!wait(&req, &Request::finished))
    return false;

  logged_in = req.status_code == 200;
  return logged_in;
}

bool findNext(QXmlStreamReader& xml, const QString& token)
{
  while (!xml.atEnd())
    if (xml.readNextStartElement() && xml.name() == token) break;
  return !xml.atEnd();
}

template<typename FUNC>
bool findNext(QXmlStreamReader& xml, const QString& token, FUNC f)
{
  while (!xml.atEnd())
    if (xml.readNextStartElement() && xml.name() == token && f(xml)) break;

  return !xml.atEnd();
}

bool findNextWithAttr(QXmlStreamReader& xml, const QString& token,
                      const QString& attr_name, const QString& attr_val)
{
  return findNext(xml, token, [&](QXmlStreamReader& _xml) {
    auto attrs = xml.attributes();
    return (attrs.hasAttribute(attr_name) && attrs.value(attr_name) == attr_val);
  });
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

void SC::login()
{
  // auto login
  if (load_cookie()) {
    DEBUG << "COOKIE LOADED" << endl;
    Request REQ(baseUrl.resolved(QUrl("/account")), request_t::HEAD);
    req.send();
    wait(&req, &Request::finished);
    DEBUG << "\n" << req.data.toStdString() << endl;
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

void SC::login(const QString& user, const QString& pw)
{
  // TODO redirect_to=false => wrong pw
  QUrl the_url = baseUrl.resolved(QUrl("/home"));

  StatusC formData = getFormData(the_url);
  if (formData.code != Status::SUCCESS) {
    DEBUG << formData.message << endl;
    return;
  }

  // extract form data
  QStringList lis = formData.data.value<QStringList>();
  QUrlQuery postData;
  for(size_t i = 0; i < lis.size(); i += 2) {
    postData.addQueryItem(lis[i], lis[i+1]);
  }

  // inject our data
  postData.removeQueryItem("user[email]");
  postData.addQueryItem("user[email]", user);
  postData.removeQueryItem("user[password]");
  postData.addQueryItem("user[password]", pw);

  // send login request
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
    if (logged_in)
      emit loggedin();
    req->deleteLater();
  });
}

StatusC SC::getFormData(const QUrl& url, ReqCallback before, ReqCallback after)
{
  StatusC ret;

  // query site
  Request REQ(url);
  if (before)
    if (!before(req)) // for setting headers ...
      return ret;
  req.send();

  if (after)
    after(req);

  if (!wait(&req, &Request::finished)) {
    ret.message = "Could not query " + url.toString();
    ret.code = Status::UNKNOWN;
    return ret;
  }

  QStringList formData;
  // only parse body as the parser doesn't like <meta .. > tags
  QRegExp body_r("<body[^>]*>(.*)</body>");
  if (body_r.indexIn(req.data) == -1) {
    ret.code = Status::UNKNOWN;
    ret.message = "Error in Document";
    return ret;
  }

  QString body = body_r.cap(1);

  // find form on url
  QXmlStreamReader xml(body);
  if (!findNext(xml, "form")) {
    ret.message = "No Form found on " + url.toString();
    ret.code = Status::UNKNOWN;
    return ret;
  }

  // find input fields
  while (true) {
    bool tmp = xml.readNextStartElement();
    if (xml.atEnd()) break;
    // are we at the </form> tag?
    if (!tmp && xml.name() == "form")
      break;
    // continue on end-tags or non-input tags
    if (!tmp || xml.name() != "input")
      continue;

    auto attrs = xml.attributes();
    // collect name and value
    formData << attrs.value("name").toString() << attrs.value("value").toString();
  }
  ret.code = Status::SUCCESS;
  ret.data = QVariant::fromValue(formData);

  return ret;
}

StatusC SC::getRedemptionData(const QString& code)
{

  StatusC ret;

  ReqCallback cb = [&](Request& req) -> bool {
    StatusC s = getToken(baseUrl.resolved(QUrl("/code_redemptions/new")));

    if (s.code != Status::SUCCESS) {
      ERROR << "Token Request Error: " << s.message << endl;
      return false;
    }

    QString& token(s.message);

    req.req.setRawHeader("x-csrf-token", token.toUtf8());
    req.req.setRawHeader("x-requested-with", "XMLHttpRequest");

    // send and follow redirections
    req.followRedirects(true);
    return true;
  };

  ReqCallback cb_after = [&, ret](Request& req) mutable -> bool {
    ret.data = req.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    return true;
  };

  StatusC formData = getFormData(baseUrl.resolved(
                                   QUrl(QString("/entitlement_offer_codes?code=%1").arg(code))), cb);
  ret.code = formData.code;
  ret.message = formData.message;

  if (formData.code == Status::SUCCESS) {
    ret.data = formData.data;
  }


//   StatusC ret;
//   StatusC s = getToken(baseUrl.resolved(QUrl("/code_redemptions/new")));

//   if (s.code != Status::SUCCESS) {
//     ERROR << "Token Request Error: " << s.message << endl;
//     ret.code = Status::UNKNOWN;
//     ret.message = s.message;
//     return ret;
//   }

//   QString& token(s.message);

//   Request REQ(baseUrl.resolved(
//                 QUrl(QString("/entitlement_offer_codes?code=%1").arg(code))));
//   req.req.setRawHeader("x-csrf-token", token.toUtf8());
//   req.req.setRawHeader("x-requested-with", "XMLHttpRequest");

//   // send and follow redirections
//   req.followRedirects(true);
//   req.send();

//   ret.code = Status::UNKNOWN;
//   if (!wait(&req, &Request::finished)) {
//     ret.message = "Timeout while requesting";
//     return ret;
//   }

//   // extract form data information

//   QXmlStreamReader xml(req.data);
//   bool found = findNextWithAttr(xml, "form",
//                                 "class", "new_archway_code_redemption");

//   /*
//     <form class="new_archway_code_redemption" id="new_archway_code_redemption" action="/code_redemptions" accept-charset="UTF-8" method="post">
//     <input name="utf8" type="hidden" value="&#x2713;" />
//     <input type="hidden" name="authenticity_token" value="PhrgflDWCCId63GLoFVbuFgTKOLt745CQyAwq5RMcn7mFY1WeL401QFW7eQVAFG/YNMs2/51LYCHUeEoMJlUGA==" />
//         <input value="W55J3-BXJ9R-SBRKJ-STWJ3-F6C99" type="hidden" name="archway_code_redemption[code]" id="archway_code_redemption_code" />
//         <input value="b9f9a278a69ff77e511b3d78bb34bdc667fb1cbe71f40e9381fb843f7e874d29" type="hidden" name="archway_code_redemption[check]" id="archway_code_redemption_check" />
//         <input value="psn" type="hidden" name="archway_code_redemption[service]" id="archway_code_redemption_service" />
//         <input type="submit" name="commit" value="Redeem for PSN" class="submit_button redeem_button" data-disable-with="Redeem for PSN" />
// </form>
// '
//    */

//   if (found) {
//     DEBUG << "FOUND REDEMPTION FORM" << endl;
//     // get `archway_code_redemption_check`
//     findNextWithAttr(xml, "input", "id", "archway_code_redemption_check");
//     QString check(xml.attributes().value("value").toString());

//     // get `archway_code_redemption_service`
//     findNextWithAttr(xml, "input", "id", "archway_code_redemption_service");
//     QString service(xml.attributes().value("value").toString());

//     // fill POST data
//     QStringList postData;
//     postData << "authenticity_token" << token;
//     postData << "archway_code_redemption[code]" << code;
//     postData << "archway_code_redemption[check]" << check;
//     postData << "archway_code_redemption[service]" << service;
//     // QUrlQuery postData;
//     // postData.addQueryItem("authenticity_token", token);
//     // postData.addQueryItem("archway_code_redemption[code]", code);
//     // postData.addQueryItem("archway_code_redemption[check]", check);
//     // postData.addQueryItem("archway_code_redemption[service]", service);

//     ret.code = Status::SUCCESS;
//     ret.data = QVariant::fromValue(postData);
//   } else {
//     ret.message = req.data;
//     // return status code
//     ret.data = req.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
//   }

  return ret;
}

// StatusC SC::getAlert(??);
// std::tuple<QString, QString> SC::getStatus(??);
// StatusC SC::checkRedemptionStatus(??);
// StatusC SC::redeemForm(??);

#undef SC
#undef REQ
#undef REQUEST
