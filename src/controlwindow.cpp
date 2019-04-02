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
#include <waitingspinnerwidget.h>

void logging_cb(const std::string& str, void* ud)
{
  QString qstr = QString::fromStdString(str);
  static_cast<QAnsiTextEdit*>(ud)->append(qstr);
}
#define CW ControlWindow

CW::ControlWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ControlWindow),
  sClient(this)
{
  ui->setupUi(this);
  connect(ui->loginButton, &QPushButton::pressed,
          this, &ControlWindow::login);

  if (FSETTINGS["no_gui"].toBool()) {
    DEBUG << "no_gui" << endl;
    ashift::logger_debug.withCallback(0, 0);
    ashift::logger_info.withCallback(0, 0);
    ashift::logger_error.withCallback(0, 0);
  } else {
    spinner = new WaitingSpinnerWidget(ui->loginButton);

    QGuiApplication* app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    QPalette palette = app->palette();
    QColor bgcolor = palette.color(QPalette::Window);

    // setup waiting spinner
    spinner->setNumberOfLines(10);
    spinner->setLineLength(5);
    spinner->setLineWidth(2);
    spinner->setInnerRadius(3);
    // spinner->setRevolutionsPerSecond(1);
    spinner->setColor(QColor(255-bgcolor.red(), 255-bgcolor.green(), 255-bgcolor.blue()));

    connect(&sClient, &ShiftClient::loggedin, this, &ControlWindow::loggedin);
    // installEventFilter(this);
    ashift::logger_debug.withCallback(logging_cb, ui->std_out);
    ashift::logger_info.withCallback(logging_cb, ui->std_out);
    ashift::logger_error.withCallback(logging_cb, ui->std_out);
  }

  // automatically set setting values from ui input
  FSETTINGS.observe(ui->limitCB, "limit_keys");
  FSETTINGS.observe(ui->limitBox, "limit_num");
  FSETTINGS.observe(ui->dropDGame, "game");
  FSETTINGS.observe(ui->dropDPlatform, "platform");
  FSETTINGS.observe(ui->dropDType, "code_type");

  // change button text
  connect(ui->controlButton, &QPushButton::toggled,
          [&](bool val) {
            if (val) {
              ui->controlButton->setText("Running ...");
              start();
            } else {
              ui->controlButton->setText("Start");
              stop();
            }
          });

  // setup cout widget
  QFont cout_font = ui->std_out->font();
  cout_font.setStyleHint(QFont::TypeWriter);
  ui->std_out->setFont(cout_font);

}

CW::~ControlWindow()
{}

void CW::login()
{
}
void CW::loggedin()
{
  spinner->stop();
  ui->loginButton->setText("signed in");
  ui->loginButton->setEnabled(false);
}

void CW::registerParser(const QString& game, const QString& platform, const QIcon& icon)
{
  // add game to dropdown if not already there
  if (ui->dropDGame->findText(game) >= 0)
    // add it with icon if there is one
    if (!icon.isNull())
      ui->dropDGame->addItem(icon, game);
    else
      ui->dropDGame->addItem(game);

  // add platform to dropdown if not already there
  if (ui->dropDPlatform->findText(platform) >= 0)
    ui->dropDPlatform->addItem(platform);
}

void CW::start()
{
  // TODO write logic
}

void CW::stop()
{
  // TODO write logic
}

bool CW::redeem()
{
  // TODO write logic
  return false;
}
#undef CW
