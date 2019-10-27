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

#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QSqlDatabase>
#include <controlwindow.hpp>
#include <misc/fsettings.hpp>

#include <shift.hpp>
#include <parser_orcz.hpp>

#if defined(_WIN32) && defined(QT_STATICPLUGIN)
  #include <QtPlugin>
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
  Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
#endif

int main(int argc, char *argv[])
{
  QApplication::setApplicationName("AutoShift");
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
  ControlWindow w;

  ////// PARSER
  BL1Parser bl1(w);
  BL2Parser bl2(w);
  BLPSParser blps(w);
  BL3Parser bl3(w);

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

  DEBUG << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/.keys.db" << endl;
  // main loop
  int exec = a.exec();
  db.close();
  return exec;
}
