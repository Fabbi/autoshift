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

#include <parser_orcz.hpp>

#define CW ControlWindow

#define SOURCE_COL 1
#define REWARD_COL 2
#define CODE_COL 3
#define EXPIRES_COL 4
#define NOTE_COL 5

static const QString messages[] {
    /*[Status::REDIRECT] = */ "",
    /*[Status::TRYLATER] =*/ CW::tr("Please launch a SHiFT-enabled title or wait 1 hour."),
    /*[Status::EXPIRED] =*/ CW::tr("This code expired by now.. (%1)"),
    /*[Status::REDEEMED] =*/ CW::tr("Already redeemed %1"),
    /*[Status::SUCCESS] =*/ CW::tr("Redeemed %1"),
    /*[Status::INVALID] =*/ CW::tr("The code `%2` is invalid"),
    /*[Status::UNAVAILABLE] =*/ "",
    /*[Status::UNKNOWN] =*/ CW::tr("An unknown Error occured: %3"),
    /*[Status::SLOWDOWN] =*/ CW::tr("Redeeming too fast, slowing down"),
    /* SIZE */ "",
    /*[Status::NONE] =*/ CW::tr("Something unexpected happened.."),
    };

static bool no_gui_out = false;
void logging_cb(const std::string& str, void* ud)
{
  if (no_gui_out) return;
  QString qstr = QString::fromStdString(str);
  static_cast<QAnsiTextEdit*>(ud)->append(qstr);
}

CW::ControlWindow(QWidget *parent) :
  QMainWindow(parent), ui(new Ui::ControlWindow),
  sClient(this), pStatus(new QLabel), tStatus(new QLabel),
  timer(new QTimer(this))
{
  ui->setupUi(this);

  // setup timer
  timer->setSingleShot(true);
  connect(timer, &QTimer::timeout,
          this, &CW::start);

  // setup statusbar
  statusBar()->addPermanentWidget(pStatus);
  statusBar()->addWidget(tStatus);

  // connect login button
  connect(ui->loginButton, &QPushButton::pressed,
          this, &ControlWindow::login);

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

  // installEventFilter(this);
  ashift::logger_debug.withCallback(logging_cb, ui->std_out);
  ashift::logger_info.withCallback(logging_cb, ui->std_out);
  ashift::logger_error.withCallback(logging_cb, ui->std_out);

  connect(&sClient, &ShiftClient::loggedin, this, &ControlWindow::loggedin);

  // automatically set setting values from ui input
  FSETTINGS.observe(ui->limitCB, "limit_keys");
  FSETTINGS.observe(ui->limitBox, "limit_num");
  FSETTINGS.observe(ui->autoClearCB, "auto_clear");
  FSETTINGS.observe<const QString&>(ui->dropDGame, "game");
  FSETTINGS.observe<const QString&>(ui->dropDPlatform, "platform");
  FSETTINGS.observe<const QString&>(ui->dropDType, "code_type");

  // change button text
  connect(ui->controlButton, &QPushButton::toggled,
          [&](bool val) {
            if (val) {
              current_limit = 255;
              if (FSETTINGS["limit_keys"].toBool())
                current_limit = FSETTINGS["limit_num"].toInt();

              start();
            } else {
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
        ShiftCode& code = *(collection.rbegin()+row);
        redeem(code);
      }
    });

  ui->keyTable->setColumnWidth(0, 15);
  ui->keyTable->setColumnWidth(SOURCE_COL, 100);
  ui->keyTable->horizontalHeader()->setSectionResizeMode(REWARD_COL, QHeaderView::Stretch);
  ui->keyTable->setColumnWidth(EXPIRES_COL, 132);
  ui->keyTable->setColumnWidth(CODE_COL, 265);

  // load state from settings
  for (uint8_t i = 0; i < ui->keyTable->columnCount(); ++i) {
    int w = FSETTINGS.loadValue(
      QString("state/keyTable/columnWidth/%1").arg(i),
      ui->keyTable->columnWidth(i))
      .toInt();
    ui->keyTable->setColumnWidth(i, w);
  }

  // setup networkmanager and make it globally available
  QNetworkAccessManager* nman = new QNetworkAccessManager(this);
  FSETTINGS.setValue("nman", qVariantFromValue((void*)nman));

  // create table if not present
  QSqlQuery create_table(CREATE_TABLE);
  collection.update_database();
}

CW::~ControlWindow()
{
  for (uint8_t i = 0; i < ui->keyTable->columnCount(); ++i) {
    int w = ui->keyTable->columnWidth(i);
    FSETTINGS.saveValue(QString("state/keyTable/columnWidth/%1").arg(i), w);
  }
}

void CW::init()
{
  connect(ui->dropDGame, QOverload<const QString&>::of(&QComboBox::currentIndexChanged),
          this, &ControlWindow::updateTable);

  connect(ui->dropDPlatform, QOverload<const QString&>::of(&QComboBox::currentIndexChanged),
          this, &ControlWindow::updateTable);
  // TODO ctrl_down => activate checkboxes to force-set redeemed flag of codes

  updateTable();
}

void CW::updateRedemption()
{
  int c = ui->keyTable->rowCount();
  auto keyIt = collection.rbegin();
  for (int row = 0; row < c; ++row) {
    ShiftCode& code = *(keyIt+row);
    QCheckBox* cb = dynamic_cast<QCheckBox*>(ui->keyTable->cellWidget(row, 0));

    cb->setChecked(code.redeemed());

    if (code.redeemed()) {
      QFont font = ui->keyTable->item(row, 1)->font();
      font.setStrikeOut(true);
      ui->keyTable->item(row, 1)->setFont(font);
      font = ui->keyTable->item(row, 2)->font();
      font.setStrikeOut(true);
      ui->keyTable->item(row, 2)->setFont(font);
    }
  }
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
  
  // only parse if there actually is a parser for this combination
  if (p) {
    // after parsing new keys
    CodeParser::Callback cb = [&](bool worked) {
      statusBar()->showMessage(QString(tr("Parsing %1")).arg((worked) ? tr("complete") : tr("failed")), 10000);
      collection.commit();
      addToTable();
    };

    p->parseKeys(collection, cb);
  }
    
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
  ui->keyTable->setItem(i, REWARD_COL, new QTableWidgetItem(code.desc()));
  // code
  ui->keyTable->setItem(i, CODE_COL, new QTableWidgetItem(code.code()));
  // expiration
  ui->keyTable->setItem(i, EXPIRES_COL, new QTableWidgetItem(code.expires()));
  // expiration
  ui->keyTable->setItem(i, NOTE_COL, new QTableWidgetItem(code.note()));
  // expiration
  ui->keyTable->setItem(i, SOURCE_COL, new QTableWidgetItem(code.source()));

  if (code.desc().contains("\n"))
    ui->keyTable->setRowHeight(i, 45);

  if (code.redeemed()) {
  QFont font = ui->keyTable->item(i, 1)->font();
  font.setStrikeOut(true);
  ui->keyTable->item(i, 1)->setFont(font);
  font = ui->keyTable->item(i, 2)->font();
  font.setStrikeOut(true);
  ui->keyTable->item(i, 2)->setFont(font);
  }
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
  ui->loginButton->setText((v)?tr("signed in"):tr("login"));

  ui->redeemButton->setEnabled(v);
  ui->controlButton->setEnabled(v);

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
  }

  // add platform to dropdown if not already there
  if (ui->dropDPlatform->findText(platform_s) == -1) {
    ui->dropDPlatform->addItem(platform_s);
  }

  // add to codeparser map
  if (!parsers.contains(game) || !parsers[game].contains(platform)) {
    if (!parsers.contains(game))
      parsers.insert(game, {});
    parsers[game].insert(platform, parser);
  }
}

void CW::start()
{
  QString code_type = FSETTINGS["code_type"].toString();
  if (!ui->controlButton->isChecked() ||
     (!current_limit && (code_type == "Golden"))) {
    stop();
    return;
  }

  ui->controlButton->setText(tr("Running ..."));

  DEBUG << "redeeming " << ((int)current_limit) << " Keys" << endl;
  // redeem next code
  StatusC st = redeemNext();

  if (st.code != Status::TRYLATER)
    // redeem every second unless we need to slow down.
    timer->start((st.code == Status::SLOWDOWN)? 60000 : 1000);
  else {
    // don't continue if there are no non-golden keys and limit is reached
    timer->start(3900000); // do this every hour + 5min

    // reparse now
    updateTable();
  }
}

void CW::stop()
{
  timer->stop();
  ui->controlButton->setText(tr("Start"));
  ui->controlButton->setChecked(false);
}

StatusC CW::redeemNext()
{
  QString code_type = FSETTINGS["code_type"].toString();
  bool golden = code_type == "Golden";
  bool non_golden = code_type == "Non-Golden";

  // find first unredeemed code
  auto it = collection.rbegin();
  for (; it != collection.rend(); ++it) {
    if ((golden && !it->golden()) || (non_golden && it->golden())) continue;
    if ((current_limit - it->golden()) < 0) continue;
    if (!it->redeemed()) break;
  }

  if (it == collection.rend()) {
    // no unredeemed key left
    statusBar()->showMessage(tr("There is no more unredeemed SHiFT code left."), 10000);
    return { Status::TRYLATER, "No more keys left" };
  }

  return redeem(*it);
}

StatusC CW::redeem(ShiftCode& code)
{
  if (code.redeemed()) {
    statusBar()->showMessage(tr("This code was already redeemed."), 10000);
    return { Status::REDEEMED, "" };
  }

  QString desc = code.desc();
  desc = desc.replace("\n", " / ");
  INFO << "Redeeming '" << desc << "'..." << endl;
  StatusC st = sClient.redeem(code);

  QString msg = messages[st.code];
  if (msg.contains("%1"))
    msg = msg.arg(desc);
  if (msg.contains("%2"))
    msg = msg.arg(code.code());
  if (msg.contains("%3"))
    msg = msg.arg(st.message.trimmed());

  INFO << msg << endl;
  statusBar()->showMessage(msg, 10000);

  switch (st.code) {
  case Status::SUCCESS:
    current_limit -= code.golden();
  case Status::REDEEMED:
  case Status::EXPIRED:
  case Status::INVALID:
    code.setRedeemed(true);
    code.commit();
    updateRedemption();
    break;
  default: break;
  };

  return st;
}
#undef CW
