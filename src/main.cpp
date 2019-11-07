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
#include <misc/logger.hpp>
namespace ashift {
Logger logger_error = Logger(&cerr, "[31mERROR[0m");
Logger logger_info = Logger("[33mINFO[0m");
Logger logger_debug = Logger("DEBUG");
Logger logger_null = Logger();
}

#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <controlwindow.hpp>
#include <misc/fsettings.hpp>

#include <shift.hpp>
#include <parser_orcz.hpp>
#include <parser_orcicorn.hpp>

#if defined(_WIN32) && defined(QT_STATICPLUGIN)
  #include <QtPlugin>
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
  Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
#endif

void setupParser(QCommandLineParser& parser)
{
  parser.addHelpOption();
  parser.addOption({ QStringList() << "v" << "verbose",
    "Turn on verbose logging." });

  //parser.addOption({ "headless",
  //  "Turn on headless mode." });

}

int main(int argc, char *argv[])
{
  QApplication::setApplicationName("AutoShift");
  QApplication::setApplicationVersion("0.4-alpha");

  // create settings/database path
  QDir appLocalData(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
  appLocalData.mkpath(".");

#if defined(_WIN32)
  QApplication::setStyle("fusion");
#endif
  FSettings& settings = FSettings::get();

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/.keys.db");
  db.open();

  QApplication a(argc, argv);

  // setup command line parser
  QCommandLineParser parser;
  setupParser(parser);

  parser.process(a);

  if (parser.isSet("verbose"))
    ashift::logger_debug.toFile("debug.log");
  else
    ashift::logger_debug.toNull();

  ControlWindow w;

  DEBUG << "Using Keys Database '" << db.databaseName() << "'" << endl;

  ////// PARSER
  orcz::BL1Parser bl1(w);
  orcicorn::BL1Parser obl1(w);
  orcz::BL2Parser bl2(w);
  orcicorn::BL2Parser obl2(w);
  orcz::BLPSParser blps(w);
  orcicorn::BLPSParser oblps(w);
  orcz::BL3Parser bl3(w);
  orcicorn::BL3Parser obl3(w);

  //////////////////////

  // get every QLineEdit and register it for sate memoization
  QList<QWidget*> widgets = w.findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);

#define REGISTER(typ) if (auto* t = qobject_cast<typ*>(wi)) \
  { FSETTINGS.registerWidget(t); continue; }

  for (size_t i = 0; i < widgets.size(); ++i) {
    QWidget* wi = widgets[i];
    REGISTER(QLineEdit);
    REGISTER(QCheckBox);
    REGISTER(QSpinBox);
    REGISTER(QComboBox);
    // else
    widgets.append(wi->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
  }
#undef REGISTER

  w.show();
  w.bringToFront();

  w.init();

  // main loop
  int exec = a.exec();
  db.close();
  return exec;
}
