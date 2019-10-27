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

#include <misc/logger.hpp>
#include <misc/fsettings.hpp>
#include <misc/xmlreader.hpp>

#include <shift.hpp>

#include "ui_loginwindow.h"

#include <request.hpp>

#define SC ShiftClient

#define COOKIE_FILE_NAME ".cookie.sav"

const QUrl baseUrl("https://shift.gearboxsoftware.com");

const QRegularExpression rForm("(<form.*?>.*?</form>)",
                               QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rAlert("<div[^>]*class=\"[^\"]*?alert.*?\">(.*?)</div>",
                                QRegularExpression::DotMatchesEverythingOption);
const QRegularExpression rRed_status("(<div[^>]*id=\"[^\"]*?check_redemption_status.*?\">(.*?)</div>)",
                                     QRegularExpression::DotMatchesEverythingOption);

SC::ShiftClient(QObject* parent)
  :QObject(parent), logged_in(false), 
  cookieFile(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
    + "/" + COOKIE_FILE_NAME)
{
  connect(this, &ShiftClient::loggedin, this, &ShiftClient::loggedinAct);
}

SC::~ShiftClient()
{
}

void SC::loggedinAct(bool v)
{
  INFO << "login " << ((v)? "successfull" : "failed") << "!" << endl;
}

Status SC::redeem(const ShiftCode& code)
{
  StatusC formData = getRedemptionData(code);

  if (formData.code != Status::SUCCESS) {
    int status_code = formData.data.toInt();
    if (status_code == 500)
      return Status::INVALID;
    if (status_code == 429)
      return Status::SLOWDOWN;
    if (formData.message.contains("expired"))
      return Status::EXPIRED;
    if (formData.message.contains("not available"))
      return Status::UNAVAILABLE;
    // unknown
    ERROR << formData.message.trimmed() << endl;
    return Status::UNKNOWN;
  }

  QStringList lis = formData.data.value<QStringList>();
  QUrlQuery postData;
  for(size_t i = 0; i < lis.size(); i += 2) {
    postData.addQueryItem(lis[i], lis[i+1]);
  }

  StatusC status = redeemForm(postData);
  return status.code;
}

void SC::delete_cookies()
{
  auto* network_manager = static_cast<QNetworkAccessManager*>(FSETTINGS["nman"].value<void*>());
  QList<QNetworkCookie> cookies = network_manager->cookieJar()->cookiesForUrl(baseUrl);
  QByteArray data;
  for (auto& cookie: cookies) {
    network_manager->cookieJar()->deleteCookie(cookie);
  }

  QFile cookie_f(cookieFile);
  cookie_f.remove();
}

bool SC::save_cookie()
{
  auto* network_manager = static_cast<QNetworkAccessManager*>(FSETTINGS["nman"].value<void*>());
  // write cookie
  QList<QNetworkCookie> cookies = network_manager->cookieJar()->cookiesForUrl(baseUrl);
  QByteArray data;
  for (auto& cookie: cookies) {
    if (cookie.name().toStdString() == "si") {
      data = cookie.toRawForm();
      break;
    }
  }
  // no cookie found
  if (data.isEmpty()) return false;

  QFile cookie_f(cookieFile);
  if (!cookie_f.open(QIODevice::WriteOnly))
    return false;

  // write cookie to file
  int data_size = data.size();
  cookie_f.write((char*)&data_size, sizeof(int));
  cookie_f.write(data);

  // write user to file
  QString user = FSETTINGS["user"].toString();
  data_size = user.size();
  cookie_f.write((char*)&data_size, sizeof(int));
  cookie_f.write(user.toUtf8());

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

  // read user
  in.readRawData((char*)&data_size, sizeof(data_size));
  data = new char[data_size];
  in.readRawData(data, data_size);
  QString user = QString::fromUtf8(data, data_size);
  FSETTINGS["user"] = user;
  delete[] data;

  cookie_f.close();

  // set cookies on current manager
  QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(cookieString);

  auto* network_manager = static_cast<QNetworkAccessManager*>(FSETTINGS["nman"].value<void*>());
  network_manager->cookieJar()->setCookiesFromUrl(cookies, baseUrl);

  // test if no success
  Request req(baseUrl.resolved(QUrl("/account")), request_t::HEAD);
  req.send();
  if (!wait(&req, &Request::finished))
    return false;

  logged_in = req.status_code == 200;
  return logged_in;
}

StatusC SC::getToken(const QUrl& url)
{
  Request req(url);
  // get request
  req.send();

  // wait for answer
  if (!wait(&req, &Request::finished)) {
    return {Status::UNKNOWN, "Timeout on Token Request"};
  }

  return getToken(req.data);
}

StatusC SC::getToken(const QString& data)
{
  StatusC ret;
  // extract token from DOM
  XMLReader xml(data);
  while (true) {
    xml.findNext("meta");
    if (xml.atEnd()) break;

    auto attrs = xml.attributes();
    auto name = attrs.value("name");

    if (name != "csrf-token") continue;

    auto token = attrs.value("content");
    if (token.isEmpty()) continue;

    return {Status::SUCCESS, token.toString()};
  }
  return {};
}

void SC::logout()
{
  Request req(baseUrl.resolved(QUrl("/logout")));
  req.send();
  wait(&req, &Request::finished);
}
void SC::login()
{
  // auto login
  if (load_cookie()) {
    logged_in = true;
    emit loggedin(logged_in);
    return;
  }

  // delete old cookies
  delete_cookies();

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

void SC::login(const QString& user_name, const QString& pw)
{
  FSETTINGS.setValue("user", user_name);
  // TODO redirect_to=false => wrong pw
  // get login form
  QUrl the_url = baseUrl.resolved(QUrl("/home"));

  StatusC formData = getFormData(the_url);
  if (formData.code != Status::SUCCESS) {
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
  postData.addQueryItem("user[email]", user_name);
  postData.removeQueryItem("user[password]");
  postData.addQueryItem("user[password]", pw);

  // send login request
  Request* req = new Request(baseUrl.resolved(QUrl("/sessions")),
                            postData, request_t::POST);
  // setup request
  req->req.setRawHeader("Referer", the_url.toEncoded());

  // send request and follow redirects
  req->send([&](Request* req) mutable {
    logged_in = save_cookie();
    emit loggedin(logged_in);
    req->deleteLater();
  });
}

StatusC SC::getFormData(const QUrl& url)
{
  // query site
  Request req(url);
  req.send();

  if (!wait(&req, &Request::finished)) {
    return {Status::UNKNOWN, "Could not query " + url.toString()};
  }

  return getFormData(req.data, Platform::NONE);
}

StatusC SC::getFormData(const QString& data, Platform platform)
{
  QStringList formData;

  // TODO: filter for the services (psn, xbox, epic, steam)
  //    <input value="epic" type="hidden" name="archway_code_redemption[service]" id="archway_code_redemption_service">
  // only parse body as the parser doesn't like <meta .. > tags
  QString form;
  QRegularExpressionMatchIterator i = rForm.globalMatch(data);
  QString searchPlatform = QString::fromStdString(sPlatform(platform)).toLower();

  while (i.hasNext()) {
	  QRegularExpressionMatch match = i.next();
	  std::string tmp = match.captured(1).toStdString();
	  if (platform == Platform::NONE) {
		  form = match.captured(1);
		  break;
	  }
	  XMLReader xml(match.captured(1));
    bool found = xml.findNextWithAttr("input", "value", searchPlatform);

	  if (found) {
		  form = match.captured(1);
		  break;
	  }
  }

  if (form.isEmpty())
    return {Status::UNKNOWN, data};

  // find form on url
  XMLReader xml(form);

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

  return {Status::SUCCESS, form, formData};
}

StatusC SC::getRedemptionData(const ShiftCode& code)
{
  StatusC ret;

  StatusC s = getToken(baseUrl.resolved(QUrl("/code_redemptions/new")));

  if (s.code != Status::SUCCESS) {
    ERROR << "Token Request Error: " << s.message << endl;
    return ret;
  }

  QString& token(s.message);

  // get form
  Request req(baseUrl.resolved(
                QUrl(QString("/entitlement_offer_codes?code=%1").arg(code.code()))));
  req.req.setRawHeader("x-csrf-token", token.toUtf8());
  req.req.setRawHeader("x-requested-with", "XMLHttpRequest");

  // send and follow redirections
  req.followRedirects(true);

  req.send();
  if (!wait(&req, &Request::finished))
    return ret;

  ret.data = req.reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

  // now extract form-data
  StatusC formData = getFormData(req.data, code.platform());

  ret.code = formData.code;
  ret.message = formData.message;

  if (formData.code == Status::SUCCESS) {
    ret.data = formData.data;
  }

  return ret;
}

StatusC SC::getAlert(const QString& content)
{
  auto match = rAlert.match(content);
  if (!match.hasMatch())
    return {};

  QString alert = match.captured(1).trimmed();

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
  auto match = rRed_status.match(content);
  if (!match.hasMatch())
    return {};

  QString div = match.captured(1);
  QString text = match.captured(2).trimmed();

  // find form on url
  XMLReader xml(div);
  xml.readNextStartElement();

  QStringList urls;
  urls << xml.attributes().value("data-url").toString();
  urls << xml.attributes().value("data-fallback-url").toString();
  return {Status::REDIRECT, text, urls};
}

StatusC SC::checkRedemptionStatus(const Request& req)
{
  // redirection
  if (req.status_code == 302)
    return {Status::REDIRECT, req.reply->rawHeader("location")};

  // check if there is a alert box..
  StatusC alert = getAlert(req.data);
  if (alert.code != Status::NONE) {
    INFO << alert.message << endl;
    return alert;
  }

  StatusC status = getStatus(req.data);
  if (status.code == Status::REDIRECT) {
    // "follow" redirects
    QStringList urls = status.data.value<QStringList>();
    QString new_location = urls[0];
    QString token = getToken(req.data).message;
    QString msg;

    // only try 5 times
    for (uint8_t cnt=0; ; ++cnt) {
      // query for status
      if (cnt > 5) {
        return {Status::REDIRECT, urls[1]};
      }

      if (msg != status.message) {
        // don't spam..
        msg = status.message;
        INFO << status.message << endl;
      }

      // query url
      Request new_req(baseUrl.resolved(new_location));
      new_req.req.setRawHeader("x-csrf-token", token.toUtf8());
      new_req.req.setRawHeader("x-requested-with", "XMLHttpRequest");
      new_req.send();
      wait(&new_req, &Request::finished);

      // extract json and generate usable information
      QJsonDocument json = QJsonDocument::fromJson(new_req.data);
      if (json["text"] != QJsonValue::Undefined) {
        QString json_text = json["text"].toString();
        if (json_text.contains("success"))
          return {Status::SUCCESS, json_text};
        if (json_text.contains("failed"))
          return {Status::REDEEMED, json_text};
      }

      // wait for 500ms (wait for a signal that won't fire...)
      wait(&req, &Request::finished, 500);
    }

  }
  return {};
}

StatusC SC::redeemForm(const QUrlQuery& data)
{
  // cache old rewards
  // old_rewards = queryRewards();
  QUrl the_url = baseUrl.resolved(QUrl("/code_redemptions"));
  // bool redemption = false;

  StatusC status;
  request_t req_type = request_t::POST;
  do {
  Request req(the_url, data, req_type);
  req.req.setRawHeader("Referer", baseUrl.resolved(QUrl("/code_redemptions/new")).toEncoded());

  req.send();
  if (!wait(&req, &Request::finished))
    return {Status::UNKNOWN, "timed out"};

  status = checkRedemptionStatus(req);
  if (status.code == Status::REDIRECT) {
    the_url = baseUrl.resolved(QUrl(status.message));
    if (status.message.contains("home?redirect_to"))
      return {Status::UNKNOWN, "Something weird happened..."};
  }

  req_type = request_t::GET;
  } while (status.code == Status::REDIRECT);

  return status;
}

#undef SC
