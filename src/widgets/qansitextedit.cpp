// #include <misc/qansitextedit.hpp>
// #include <QAnsiTextEdit>
#include <widgets/qansitextedit.hpp>
#include <widgets/moc_qansitextedit.cpp>
#include <QFontDatabase>
#include <QColor>
#include <array>
#include <logger.hpp>

#define QTE QAnsiTextEdit
#define ESCSEQ R"(\x1B\[([\d;]+)m)"

std::array<QColor, 8> colors = {
  Qt::black, // 0
  // Qt::red,      // 1
  QColor(0xFFF9324B),
  // Qt::green,    // 2
  QColor(0xFF8AE234),
  // Qt::yellow,   // 3
  QColor(0xFFFCE94F),
  Qt::blue,     // 4
  Qt::magenta,  // 5
  // Qt::cyan,     // 6
  QColor(0xFF34E2E2),
  Qt::white     // 7
    };

std::array<QColor, 8> colors2 = {
  Qt::black,
  // Qt::darkRed,
  QColor(0xFFDC322F),
  // Qt::darkGreen,
  QColor(0xFF4E9A06),
  // Qt::darkYellow,
  QColor(0xFFC4A000),
  Qt::darkBlue,
  Qt::darkMagenta,
  // Qt::darkCyan,
  QColor(0xFF06989A),
  Qt::lightGray
};

QTE::QAnsiTextEdit(QWidget* parent):
  QTextEdit(parent), escSeq(ESCSEQ)
{}

QTE::QAnsiTextEdit(const QString& text, QWidget* parent):
  QTextEdit(text, parent), escSeq(ESCSEQ), defaultCharFormat(QTextCursor(document()).charFormat())
{}

QTE::~QAnsiTextEdit()
{}

void QTE::append(const QString& text)
{
  insertIntoDocument(document(), text);
}

void QTE::setText(const QString& text)
{
  QTextDocument* newDocument = new QTextDocument(this);
  insertIntoDocument(newDocument, text);
  setDocument(newDocument);
}

void QTE::insertIntoDocument(QTextDocument* document, const QString& text)
{
  if (text.isEmpty() || !document) return;
  QTextCursor cursor(document);
  cursor.movePosition(QTextCursor::End);
  // QTextCharFormat const defaultCharFormat = cursor.charFormat();
  // QTextCharFormat charFormat = defaultCharFormat;
  QStringList capturedTexts;
  bool ok = false;
  int searchStart = 0;
  int offset = escSeq.indexIn(text, searchStart);

  cursor.beginEditBlock();
  // insert everything until first `escSeq`
  cursor.insertText(text.mid(searchStart, offset), charFormat);
  // find every occurrence of `escSeq` and handle it
  while (offset != -1) {
    searchStart = offset + escSeq.matchedLength();
    capturedTexts = escSeq.capturedTexts().back().split(';');
    QListIterator<QString> it(capturedTexts);
    while (it.hasNext()) {
      int attribute = it.next().toInt(); // we only search for \d, so this should not fail
      parseEscapeSequence(attribute, it, charFormat, defaultCharFormat);
    }
    offset = escSeq.indexIn(text, searchStart);

    cursor.insertText(text.mid(searchStart, offset-searchStart), charFormat);
  }
  // cursor.setCharFormat(defaultCharFormat);
  cursor.endEditBlock();

  setTextCursor(cursor);
}

void QTE::parseEscapeSequence(int attr, QListIterator<QString>& it,
                              QTextCharFormat& format, QTextCharFormat const & defaultFormat)
{
    switch (attr) {
    case 0 : // Normal/Default (reset all attrs)
      {format = defaultFormat; break;}
    case 1 : // Bold/Bright (bold or increased intensity)
      {format.setFontWeight(QFont::Bold); break;}
    case 2 : // Dim/Faint (decreased intensity)
      {format.setFontWeight(QFont::Light); break;}
    case 3 : // Italicized (italic on)
      {format.setFontItalic(true); break;}
    case 4 : {// Underscore (single underlined)
      format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
      format.setFontUnderline(true); break;}
    case 5 : // Blink (slow, appears as Bold)
      {format.setFontWeight(QFont::Bold); break;}
    case 6 : // Blink (rapid, appears as very Bold)
      {format.setFontWeight(QFont::Black); break;}
    case 7 : {// Reverse/Inverse (swap foreground and background)
      QBrush foregroundBrush = format.foreground();
      format.setForeground(format.background());
      format.setBackground(foregroundBrush); break;}
    case 8 : // Concealed/Hidden/Invisible (usefull for passwords)
      {format.setForeground(format.background()); break;}
    case 9 : // Crossed-out characters
      {format.setFontStrikeOut(true); break;}
    case 10 : // Primary (default) font
      {format.setFont(defaultFormat.font()); break;}
    case 11 ... 19 : {
      QFontDatabase fontDatabase;
      QString fontFamily = format.fontFamily();
      QStringList fontStyles = fontDatabase.styles(fontFamily);
      int fontStyleIndex = attr - 11;
      if (fontStyleIndex < fontStyles.length())
        format.setFont(fontDatabase.font(fontFamily, fontStyles.at(fontStyleIndex), format.font().pointSize()));
      break;}
    case 20 : // Fraktur (unsupported)
      {break;}
    case 21 : // Set Bold off
      {format.setFontWeight(QFont::Normal); break;}
    case 22 : // Set Dim off
      {format.setFontWeight(QFont::Normal); break;}
    case 23 : // Unset italic and unset fraktur
      {format.setFontItalic(false); break;}
    case 24 : // Unset underlining
      {format.setUnderlineStyle(QTextCharFormat::NoUnderline);
      format.setFontUnderline(false); break;}
    case 25 : // Unset Blink/Bold
      {format.setFontWeight(QFont::Normal); break;}
    case 26 : // Reserved
      {break;}
    case 27 : // Positive (non-inverted)
      {QBrush backgroundBrush = format.background();
      format.setBackground(format.foreground());
      format.setForeground(backgroundBrush); break;}
    case 28 :
      {format.setForeground(defaultFormat.foreground());
      format.setBackground(defaultFormat.background()); break;}
    case 29 :
      {format.setUnderlineStyle(QTextCharFormat::NoUnderline);
      format.setFontUnderline(false); break;}
    case 30 ... 37 : {
      int colorIndex = attr - 30;
      QColor color;
      if (QFont::Normal < format.fontWeight()) {
        color = colors2[colorIndex];
      } else {
        color = colors[colorIndex];
        }
      format.setForeground(color);
      break;
    }
    case 39 :
      {format.setForeground(defaultFormat.foreground()); break;}
    case 40 ... 47 : {
      int colorIndex = attr - 40;
      QColor color = colors[colorIndex];
      format.setBackground(color);
      break;
    }
    case 38 :
    case 48 : {
      if (it.hasNext()) {
        bool ok = false;
        int selector = it.next().toInt(&ok);
        Q_ASSERT(ok);
        QColor color;
        switch (selector) {
        case 2 : {
          if (!it.hasNext()) {break;}
          int red = it.next().toInt(&ok);
          Q_ASSERT(ok);
          if (!it.hasNext()) {break;}
          int green = it.next().toInt(&ok);
          Q_ASSERT(ok);
          if (!it.hasNext()) {break;}
          int blue = it.next().toInt(&ok);
          Q_ASSERT(ok);
          color.setRgb(red, green, blue);
          break;
        }
        case 5 : {
          if (!it.hasNext()) {break;}
          int index = it.next().toInt(&ok);
          Q_ASSERT(ok);
          switch (index) {
          case 0x00 ... 0x07 : { // 0x00-0x07:  standard colors (as in ESC [ 40..47 m)
            return parseEscapeSequence(index - 0x00 + ((attr==38)?30:40), it, format, defaultFormat);
          }
          case 0x08 ... 0x0F : { // 0x08-0x0F:  high intensity colors (as in ESC [ 100..107 m)
            return parseEscapeSequence(index - 0x08 + ((attr==38)?90:100), it, format, defaultFormat);
          }
          case 0x10 ... 0xE7 : { // 0x10-0xE7:  6*6*6=216 colors: 16 + 36*r + 6*g + b (0≤r,g,b≤5)
            index -= 0x10;
            int red = index % 6; index /= 6;
            int green = index % 6; index /= 6;
            int blue = index % 6; index /= 6;
            Q_ASSERT(index == 0);
            color.setRgb(red, green, blue);
            break;
          }
          case 0xE8 ... 0xFF : { // 0xE8-0xFF:  grayscale from black to white in 24 steps
            qreal intensity = qreal(index - 0xE8) / (0xFF - 0xE8);
            color.setRgbF(intensity, intensity, intensity);
            break;
          }
          }
          if (attr==38)
            format.setForeground(color);
          else
            format.setBackground(color);
          break;
        }
        default : {break;}
        }
      }
      break;
    }
    case 49 :
      {format.setBackground(defaultFormat.background()); break;}
    case 90 ... 97 :
    case 100 ... 107 : {
      int colorIndex = attr - 100;
      QColor color = colors[colorIndex];

      color.setRedF(color.redF() * 0.8);
      color.setGreenF(color.greenF() * 0.8);
      color.setBlueF(color.blueF() * 0.8);
      if (attr <= 97)
        format.setForeground(color);
      else
        format.setBackground(color);
      break;
    }
    default : {
      break;
    }
    }
  }

#undef QTE
#undef ESCSEQ
