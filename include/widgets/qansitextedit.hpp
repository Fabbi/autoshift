#pragma once

#include <QTextEdit>
// #include <QTextDocument>
#include <QRegExp>
// #include <QTextCursor>
// #include <QTextCharFormat>
// #include <QFontDatabase>

class QAnsiTextEdit : public QTextEdit
{
  Q_OBJECT

public:
  QAnsiTextEdit(QWidget* parent = Q_NULLPTR);
  QAnsiTextEdit(const QString &text, QWidget* parent = Q_NULLPTR);
  ~QAnsiTextEdit();

private:
  void parseEscapeSequence(int attribute, QListIterator<QString>& i,
                           QTextCharFormat& format, QTextCharFormat const & defaultFormat);
  void insertIntoDocument(QTextDocument* document, const QString& text);

  // Variables
public:

private:
  QRegExp escSeq;
  QTextCharFormat const defaultCharFormat;
  QTextCharFormat charFormat;
signals:

public slots:
  void append(const QString& text);
  void setText(const QString& text);

};
