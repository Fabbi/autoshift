#pragma once
#include <QXmlStreamReader>

class XMLReader: public QXmlStreamReader
{
public:
  using QXmlStreamReader::QXmlStreamReader;
  inline bool findNext(const QString& token)
  {
    while (!atEnd())
      if (readNextStartElement() && name() == token) break;
    return !atEnd();
  }

  template<typename FUNC>
  inline bool findNext(const QString& token, FUNC f)
  {
    while (!atEnd())
      if (readNextStartElement() && name() == token && f(*this)) break;

    return !atEnd();
  }

  inline bool findNextWithAttr(const QString& token,
    const QString& attr_name, const QString& attr_val)
  {
    return findNext(token, [&](QXmlStreamReader& _xml) {
      auto attrs = attributes();
      return (attrs.hasAttribute(attr_name) && attrs.value(attr_name) == attr_val);
      });
  }
};

