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
  : manager(&_manager), url(_url), reply(0), data(""), type(_type), req(url)
{
  // fake user-agent
  req.setHeader(QNetworkRequest::UserAgentHeader,
                "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Safari/605.1.15");
}

Request::~Request()
{if (reply) reply->deleteLater();}

template<typename FUNC>
void Request::send(FUNC fun, bool follow)
{
  // connect to this
  connect(this, &Request::finished, fun);

  send(follow);
}

void Request::send(bool follow)
{
  data.clear();
  if (reply) {
    delete reply;
    reply = nullptr;
  }

  // get or post
  if (type == request_t::GET) {
    DEBUG << "GET " << url.toString() << endl;
    reply = manager->get(req);
  } else {
    DEBUG << "POST " << url.toString() << endl;
    reply = manager->post(req, query_data);
  }

  connect(reply, &QIODevice::readyRead, this, [&]() {
    data.append(reply->readAll());
  });

  // connect to reply
  connect(reply, &QNetworkReply::finished, this, [&, follow]() {
    QVariant statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute );
    const QList<QPair<QByteArray,QByteArray>>& hdr_list = reply->rawHeaderPairs();
    // DEBUG << "== HDRS ==" << endl;
    // for (auto& pair: hdr_list) {
    //   DEBUG << pair.first.toStdString() << ": " << pair.second.toStdString() << endl;
    // }
    DEBUG << "    STATUS CODE " << statusCode.toInt() << endl;
    // follow redirects
    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (follow && redirectionTarget.isValid()) {
      // change to http GET
      type = request_t::GET;

      // only keep referer
      QByteArray oldReferer = req.rawHeader("Referer");
      url = url.resolved(redirectionTarget.toUrl());
      req = QNetworkRequest(url);
      req.setRawHeader("Referer", oldReferer);
      DEBUG << "auto redirect to " << url.toString() << endl;

      // recursive call with 500ms delay (don't spam)
      QTimer::singleShot(500, [&, follow](){send(follow);});
    } else {
      // our job here is done
      emit finished(data);
      deleteLater();
    }
  });
}
