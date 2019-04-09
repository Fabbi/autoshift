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
#include <boost/program_options.hpp>

#include <QApplication>
#include <QSqlDatabase>
#include <controlwindow.hpp>
#include <misc/fsettings.hpp>

#include <shift.hpp>
#include <query.hpp>

namespace po = boost::program_options;

int main(int argc, char *argv[])
{
  FSettings& settings = FSettings::get();

  bool no_gui = false;

  po::options_description desc("Options");
  desc.add_options()
    ("help", "produce help message")
    ("no-gui", po::bool_switch(&no_gui), "deactivate GUI")
    // ...
    ;
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  // --help
  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  settings.setValue("no_gui", no_gui);

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(".keys.db");
  db.open();

  QApplication a(argc, argv);
  ControlWindow w;

  ////// PARSER
  BL2Parser bl2(w);
  BLPSParser blps(w);

  //////////////////////
  w.init();

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

  if (!no_gui) {
    w.show();
    w.bringToFront();
  }

  // main loop
  int exec = a.exec();
  db.close();
  return exec;
}
