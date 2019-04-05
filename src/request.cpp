#include "request.hpp"
#include <QtNetwork>

#include <logger.hpp>

Request::Request(QNetworkAccessManager& _manager, const QUrl& _url,
                 const QUrlQuery& _data, request_t _type)
  :Request(_manager, _url, _type)
{
  query_data = _data.toString(QUrl::FullyEncoded).toUtf8();
}
Request::Request(QNetworkAccessManager& _manager, const QUrl& _url,
                 request_t _type)
  : manager(&_manager), url(_url), reply(0), data(""),
    type(_type), req(url), follow_redirects(false), status_code(-1)
{
  // fake user-agent
  req.setHeader(QNetworkRequest::UserAgentHeader,
                "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Safari/605.1.15");
}

Request::~Request()
{if (reply) reply->deleteLater();}

void Request::send(int timeout)
{
  status_code = -1;
  timed_out = false;
  data.clear();
  if (reply) {
    delete reply;
    reply = nullptr;
  }

  // send request
  switch (type) {
  case request_t::GET:
  {
    DEBUG << "GET " << url.toString() << endl;
    reply = manager->get(req);
  } break;
  case request_t::POST:
  {
    DEBUG << "POST " << url.toString() << endl;
    reply = manager->post(req, query_data);
  } break;
  case request_t::HEAD:
  {
    DEBUG << "HEAD " << url.toString() << endl;
    reply = manager->head(req);
  } break;
  default: break;
  }

  // configure timeout
  QTimer::singleShot(timeout, [&](){
    reply->abort();
    timed_out = true;
    // disconnect from `finished` to not further handle this one.
    disconnect(reply, &QNetworkReply::finished, this, 0);
    emit finished(this);
  });
  connect(reply, &QIODevice::readyRead, this, [&]() {
    data.append(reply->readAll());
  });

  // connect to reply
  connect(reply, &QNetworkReply::finished, this, [&]() {
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    status_code = statusCode.toInt();
    // DEBUG << "    STATUS CODE " << statusCode.toInt() << endl;
    // DEBUG << "==================" << endl;
    // DEBUG << "== REQUEST HDRS ==" << endl;
    // const QList<QByteArray>& hdr_list = reply->request().rawHeaderList();
    // for (auto& hdr: hdr_list) {
    //   DEBUG << hdr.toStdString() << ": " << reply->request().rawHeader(hdr).toStdString() << endl;
    // }
    // DEBUG << "== REPLY   HDRS ==" << endl;
    // const QList<QPair<QByteArray,QByteArray>>& hdr_list2 = reply->rawHeaderPairs();
    // for (auto& pair: hdr_list2) {
    //   DEBUG << pair.first.toStdString() << ": " << pair.second.toStdString() << endl;
    // }
    // DEBUG << "==================" << endl;
    // follow redirects
    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (follow_redirects && redirectionTarget.isValid()) {
      // change to http GET
      type = request_t::GET;

      // only keep referer
      QByteArray oldReferer = req.rawHeader("Referer");
      url = url.resolved(redirectionTarget.toUrl());
      req = QNetworkRequest(url);
      req.setRawHeader("Referer", oldReferer);
      DEBUG << "auto redirect to " << url.toString() << endl;

      // recursive call with 500ms delay (don't spam)
      QTimer::singleShot(500, [&](){send();});
    } else {
      // our job here is done
      emit finished(this);
    }
  });
}
