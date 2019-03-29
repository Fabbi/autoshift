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

#include "controlwindow.hpp"
#include "ui_controlwindow.h"

#include <logger.hpp>

#include <fsettings.hpp>

#include <widgets/qansitextedit.hpp>

void logging_cb(const std::string& str, void* ud)
{
  QString qstr = QString::fromStdString(str);
  static_cast<QAnsiTextEdit*>(ud)->append(qstr);
}
#define CW ControlWindow

CW::ControlWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ControlWindow)
{
  ui->setupUi(this);

  if (FSETTINGS["no_gui"].toBool()) {
    DEBUG << "no_gui" << endl;
    ashift::logger_debug.withCallback(0, 0);
    ashift::logger_info.withCallback(0, 0);
    ashift::logger_error.withCallback(0, 0);
  } else {
    // installEventFilter(this);
    ashift::logger_debug.withCallback(logging_cb, ui->std_out);
    ashift::logger_info.withCallback(logging_cb, ui->std_out);
    ashift::logger_error.withCallback(logging_cb, ui->std_out);
  }

  FSETTINGS.observe(ui->limitCB, "limit_keys");
  FSETTINGS.observe(ui->limitBox, "limit_num");

  connect(ui->controlButton, &QPushButton::toggled,
          [&](bool val) {
            if (val) {
              ui->controlButton->setText("Running ...");
              start();
            } else {
              ui->controlButton->setText("Start");
            }
          });

  // setup cout widget
  QFont cout_font = ui->std_out->font();
  cout_font.setStyleHint(QFont::TypeWriter);
  ui->std_out->setFont(cout_font);

}

CW::~ControlWindow()
{}

void CW::start()
{
  // TODO write logic
}
#undef CW
