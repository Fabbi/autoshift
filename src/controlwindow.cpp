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

#include <controlwindow.hpp>
#include "ui_controlwindow.h"

#include <misc/logger.hpp>

#include <misc/fsettings.hpp>

#include <widgets/qansitextedit.hpp>
#include <waitingspinnerwidget.h>

#include <QNetworkAccessManager>

#include <query.hpp>

static bool no_gui_out = false;
void logging_cb(const std::string& str, void* ud)
{
  if (no_gui_out) return;
  QString qstr = QString::fromStdString(str);
  static_cast<QAnsiTextEdit*>(ud)->append(qstr);
}
#define CW ControlWindow

CW::ControlWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ControlWindow),
  sClient(this), pStatus(new QLabel), tStatus(new QLabel)
{
  ui->setupUi(this);

  // setup statusbar
  statusBar()->addPermanentWidget(pStatus);
  statusBar()->addWidget(tStatus);

  // connect login button
  connect(ui->loginButton, &QPushButton::pressed,
          this, &ControlWindow::login);

  // did we start in no-gui mode?
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
  FSETTINGS.observe<const QString&>(ui->dropDGame, "game");
  FSETTINGS.observe<const QString&>(ui->dropDPlatform, "platform");
  FSETTINGS.observe<const QString&>(ui->dropDType, "code_type");

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

  // login();
  connect(ui->redeemButton, &QPushButton::released, this, [&] () {

      if (!ui->loginButton->isEnabled()) {
        // get selected row Item
        auto selection = ui->keyTable->selectedItems();
        if (selection.isEmpty()) return;

        int row = selection[0]->row();
        // QString code = static_cast<QLabel*>(ui->keyTable->cellWidget(row, 2))->text();
        QString code = ui->keyTable->item(row, 2)->text();
        // DEBUG << code << endl;
        Status st = sClient.redeem(code);
        // Status st = sClient.redeem(ui->codeInput->text());
        // Status st = sClient.redeem("WWK3B-SSBZF-9TFKJ-HBK3T-FRRST");
        // DEBUG << sStatus(st) << endl;
      }
    });

  ui->keyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  ui->keyTable->setColumnWidth(0, 15);
  ui->keyTable->setColumnWidth(2, 265);

  // setup networkmanager and make it globally available
  QNetworkAccessManager* nman = new QNetworkAccessManager(this);
  FSETTINGS.setValue("nman", qVariantFromValue((void*)nman));
}

CW::~ControlWindow()
{}

void CW::init()
{
  connect(ui->dropDGame, QOverload<const QString&>::of(&QComboBox::currentIndexChanged),
          this, &ControlWindow::updateTable);

  connect(ui->dropDPlatform, QOverload<const QString&>::of(&QComboBox::currentIndexChanged),
          this, &ControlWindow::updateTable);
  // TODO ctrl_down => activate checkboxes to force-set redeemed flag of codes

  updateTable();
}

void CW::updateTable()
{
  // commit changes
  collection.clear();

  Game game = tGame(ui->dropDGame->currentText().toStdString());
  Platform platform = tPlatform(ui->dropDPlatform->currentText().toStdString());

  if (game == Game::NONE || platform == Platform::NONE) {
    return;
  }

  // query from database
  collection.query(platform, game, true);
  addToTable();

  CodeParser* p = parsers[game][platform];

  // after parsing new keys
  CodeParser::Callback cb = [&](bool worked) {
    DEBUG << "parsing " << worked << endl;
    statusBar()->showMessage(QString("Parsing %1").arg((worked)? "complete" : "failed"), 10000);
    collection.commit();
    addToTable();
  };

  p->parseKeys(collection, cb);
}

void CW::addToTable()
{
  ui->keyTable->setRowCount(collection.size());
  size_t i = 0;
  // insert backwards
  for (auto it = collection.rbegin(); it != collection.rend(); ++it, ++i) {
    insertRow(*it, i);
  }

}

void CW::insertRow(const ShiftCode& code, size_t i)
{
  int c = ui->keyTable->rowCount();
  if (i >= c)
    ui->keyTable->insertRow(i);

  // QLabel *label = new QLabel;
  // label->setText(key.desc());
  // label->setTextFormat(Qt::RichText);
  // label->setWordWrap(true);
  // label->setOpenExternalLinks(true);

  // ui->keyTable->setCellWidget(c, 1, label);
  QCheckBox* cb = new QCheckBox;
  cb->setChecked(code.redeemed());
  cb->setEnabled(false);
  ui->keyTable->setCellWidget(i, 0, cb);

  // description
  ui->keyTable->setItem(i, 1, new QTableWidgetItem(code.desc()));
  // code
  ui->keyTable->setItem(i, 2, new QTableWidgetItem(code.code()));
  // expiration
  ui->keyTable->setItem(i, 3, new QTableWidgetItem(code.expires()));

  if (code.desc().contains("\n"))
    ui->keyTable->setRowHeight(i, 45);
}

void CW::login()
{
  ui->loginButton->setText("");
  spinner->start();

  sClient.login();
  // ui->loginButton->tit

  spinner->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void CW::loggedin(bool v)
{
  spinner->stop();
  ui->loginButton->setEnabled(!v);
  ui->loginButton->setText((v)?"signed in":"login");

  if (v) {
    QString user = FSETTINGS["user"].toString();
    pStatus->setText(user);
  }
}

void CW::registerParser(Game game, Platform platform, CodeParser* parser, const QIcon& icon)
{
  bool is_new = false;
  QString game_s(sGame(game).c_str());
  QString platform_s(sPlatform(platform).c_str());

  // add game to dropdown if not already there
  if (ui->dropDGame->findText(game_s) == -1) {
    // add it with icon if there is one
    if (!icon.isNull())
      ui->dropDGame->addItem(icon, game_s);
    else
      ui->dropDGame->addItem(game_s);

    is_new = true;
  }

  // add platform to dropdown if not already there
  if (ui->dropDPlatform->findText(platform_s) == -1) {
    ui->dropDPlatform->addItem(platform_s);
    is_new = true;
  }

  // add to codeparser map
  if (is_new) {
    DEBUG << "registerParser(" << sGame(game) << ", " << sPlatform(platform) << ")" << endl;
    if (!parsers.contains(game))
      parsers.insert(game, {});
    parsers[game].insert(platform, parser);
  }
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
