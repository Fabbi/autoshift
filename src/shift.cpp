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

const QRegExp rBody("<body[^>]*>(.*)</body>");

SC::ShiftClient(QObject* parent)
  :QObject(parent), logged_in(false)
{

  getAlert("<body><div class='notice'>hallo welt</div></body>");
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
  StatusC formData = getRedemptionData(code);

  if (formData.code != Status::SUCCESS) {
    int status_code = formData.data.toInt();
    if (status_code == 500)
      return Status::INVALID;
    if (formData.message.contains("expired"))
      return Status::EXPIRED;
    // unknown
    ERROR << formData.message << endl;
    return Status::UNKNOWN;
  }

  QStringList lis = formData.data.value<QStringList>();
  QUrlQuery postData;
  for(size_t i = 0; i < lis.size(); i += 2) {
    postData.addQueryItem(lis[i], lis[i+1]);
  }

  StatusC status = redeemForm(postData);
  DEBUG << sStatus(status.code) << ": " << status.message << endl;
  return status.code;
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
    logged_in = true;
    emit loggedin(logged_in);
    return;
  }

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
  } else {
    emit loggedin(false);
  }
}

void SC::login(const QString& user, const QString& pw)
{
  // TODO redirect_to=false => wrong pw
  QUrl the_url = baseUrl.resolved(QUrl("/home"));

  StatusC formData = getFormData(the_url);
  if (formData.code != Status::SUCCESS) {
    DEBUG << formData.message << endl;
    emit loggedin(false);
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
  // req->req.setRawHeader("Origin", baseUrl.toEncoded());
  // req->req.setRawHeader("Connection", "keep-alive");
  // req->req.setRawHeader("Accept", "*/*");
  // req->req.setRawHeader("Accept-Encoding", "gzip, deflate");

  // send request and follow redirects
  req->send([&, req](QByteArray data) mutable {
    DEBUG << "============= LOGIN ============" << endl;
    logged_in = save_cookie();
    emit loggedin(logged_in);
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
  if (rBody.indexIn(req.data) == -1) {
    ret.code = Status::UNKNOWN;
    ret.message = "Error in Document";
    return ret;
  }

  QString body = rBody.cap(1);

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
                                   QUrl(QString("/entitlement_offer_codes?code=%1").arg(code))),
                                 cb, cb_after);
  ret.code = formData.code;
  ret.message = formData.message;

  if (formData.code == Status::SUCCESS) {
    ret.data = formData.data;
  }

  return ret;
}

StatusC SC::getAlert(const QString& content)
{
  if (rBody.indexIn(content) == -1) {
    return {Status::UNKNOWN, "Error in Document"};
  }

  QString body = rBody.cap(1);

  // find form on url
  QXmlStreamReader xml(body);

  if (!findNextWithAttr(xml, "div", "class", "notice")) {
    return {};
  }

  QString alert = xml.readElementText().trimmed();
  QString alert_l = alert.toLower();
  Status status = Status::NONE;
  if (alert_l.contains("to continue to redeem")) {
    status = Status::TRYLATER;
  } else if (alert_l.contains("expired")) {
    status = Status::EXPIRED;
  } else if (alert_l.contains("success")) {
    status = Status::SUCCESS;
  } else if (alert_l.contains("failed")) {
    status = Status::REDEEMED;
  }

  return {status, alert};
}

StatusC SC::getStatus(const QString& content)
{
  if (rBody.indexIn(content) == -1) {
    return {Status::UNKNOWN, "Error in Document"};
  }

  QString body = rBody.cap(1);

  // find form on url
  QXmlStreamReader xml(body);

  if (!findNextWithAttr(xml, "div", "id", "check_redemption_status")) {
    return {};
  }

  return {Status::REDIRECT, xml.readElementText().trimmed(),
          xml.attributes().value("data-url").toString()};
}

StatusC SC::checkRedemptionStatus(const Request& req)
{
  // redirection
  if (req.status_code == 302)
    return {Status::REDIRECT, req.req.rawHeader("location")};

  StatusC alert = getAlert(req.data);
  if (alert.code != Status::NONE) {
    DEBUG << alert.message << endl;
    return alert;
  }

  StatusC status = getStatus(req.data);
  if (status.code == Status::REDIRECT) {
    INFO << status.message << endl;
    QString new_location = status.data.toString();
    // wait for 500ms (wait for a signal that won't fire...)
    wait(&req, &Request::finished, 500);
    // redirect now
    Request new_req(network_manager, baseUrl.resolved(new_location));
    new_req.send();
    wait(&new_req, &Request::finished);

    // recursive call
    return checkRedemptionStatus(new_req);
  }

  QStringList new_rewards = queryRewards();
  if (!new_rewards.isEmpty() && (new_rewards != old_rewards))
    return {Status::SUCCESS, ""};

  return {};
}

QStringList SC::queryRewards()
{
  QStringList ret;
  Request REQ(baseUrl.resolved(QUrl("/rewards")));
  req.send();
  wait(&req, &Request::finished);

  if (rBody.indexIn(req.data) == -1) {
    return ret;
  }

  QString body = rBody.cap(1);

  // find form on url
  QXmlStreamReader xml(body);

  while (findNextWithAttr(xml, "div", "class", "reward_unlocked"))
    ret << xml.readElementText();

  return ret;
}

StatusC SC::redeemForm(const QUrlQuery& data)
{
  // cache old rewards
  old_rewards = queryRewards();

  QUrl the_url = baseUrl.resolved(QUrl("/code_redemptions"));
  Request REQ(the_url, data, request_t::POST);
  req.req.setRawHeader("Referer", the_url.toEncoded() + "/new");

  req.send();
  if (!wait(&req, &Request::finished))
    return {};

  StatusC status = checkRedemptionStatus(req);
  // did we visit /code_redemptions/...... route?
  bool redemption = false;
  // keep following redirects
  while (status.code == Status::REDIRECT) {
    if (status.message.contains("code_redemptions"))
      redemption = true;
    DEBUG << "redirect to " << status.message << endl;

    Request new_req(network_manager, baseUrl.resolved(QUrl(status.message)));
    new_req.send();

    if (!wait(&new_req, &Request::finished))
      return {};

    status = checkRedemptionStatus(new_req);
  }

  // workaround for new SHiFT website
  // it doesn't tell you to launch a "SHiFT-enabled title" anymore
  if (status.code == Status::NONE) {
    if (redemption)
      return {Status::REDEEMED, "Already Redeemed"};
    else
      return {Status::TRYLATER, "Launch a SHiFT-enabled title first!"};
  }

  return status;
}

#undef SC
#undef REQ
#undef REQUEST
