#include <request.hpp>
#include <QtNetwork>

#include <misc/logger.hpp>
#include <misc/fsettings.hpp>
// #define DEBUG_HEADER

Request::Request(const QUrl& _url, QNetworkAccessManager* _man,
                 request_t _type)
  : url(_url), reply(0), data(""),
    type(_type), req(url), follow_redirects(false), status_code(-1),
    timed_out(false), timeout_timer(0), manager(_man)
{
  setParent(manager);
}

Request::Request(const QUrl& _url, QNetworkAccessManager* _man,
                 const QUrlQuery& _data, request_t _type)
  :Request(_url, _man, _type)
{
  query_data = _data.toString(QUrl::FullyEncoded)
    .replace("+", "%2B").toUtf8();
}

Request::Request(const QUrl& _url,
                 const QUrlQuery& _data, request_t _type)
  :Request(_url, static_cast<QNetworkAccessManager*>(FSETTINGS["nman"].value<void*>()),
           _data, _type)
{}

Request::Request(const QUrl& _url,
                 request_t _type)
  :Request(_url, static_cast<QNetworkAccessManager*>(FSETTINGS["nman"].value<void*>()),
           _type)
{}

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

  // set header if not present
  if (!req.hasRawHeader("Accept"))
    req.setRawHeader("Accept", "*/*");

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
    if (req.header(QNetworkRequest::ContentTypeHeader).isNull())
      req.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");
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
  if (timeout_timer) {
    timeout_timer->stop();
    delete timeout_timer;
  }
  if (timeout > 0) {
    timeout_timer = new QTimer(this);
    timeout_timer->setSingleShot(true);
    connect(timeout_timer, &QTimer::timeout, this, [&](){
      disconnect(reply, &QNetworkReply::finished, this, 0);
      reply->abort();
      timed_out = true;
      // disconnect from `finished` to not further handle this one.
      emit finished(this);
    });
    timeout_timer->start(timeout);
  }

  // TODO error handling
  // connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
  //         this, [](QNetworkReply::NetworkError code) {
  //           ERROR << code << endl;
  //         });

  connect(reply, &QIODevice::readyRead, this, [&]() {
    data.append(reply->readAll());
  });

  // connect to reply
  connect(reply, &QNetworkReply::finished, this, [&]() {
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    status_code = statusCode.toInt();
#ifdef DEBUG_HEADER
    DEBUG << "    STATUS CODE " << statusCode.toInt() << endl;
    DEBUG << "==================" << endl;
    DEBUG << "== REQUEST HDRS ==" << endl;
    const QList<QByteArray>& hdr_list = reply->request().rawHeaderList();
    for (auto& hdr: hdr_list) {
      DEBUG << hdr.toStdString() << ": " << reply->request().rawHeader(hdr).toStdString() << endl;
    }
    DEBUG << "== REPLY   HDRS ==" << endl;
    const QList<QPair<QByteArray,QByteArray>>& hdr_list2 = reply->rawHeaderPairs();
    for (auto& pair: hdr_list2) {
      DEBUG << pair.first.toStdString() << ": " << pair.second.toStdString() << endl;
    }
    DEBUG << "==================\n" << endl;
#endif
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

      // recursive call with 500ms delay (don't spam)
      QTimer::singleShot(500, [&](){send();});
    } else {
      if (timeout_timer) {
        timeout_timer->stop();
        delete timeout_timer;
        timeout_timer = nullptr;
      }
      // our job here is done
      emit finished(this);
    }
  });
}
