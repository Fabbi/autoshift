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

#pragma once

#include "ui_controlwindow.h"
#include "shift.hpp"

class WaitingSpinnerWidget;
class ControlWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit ControlWindow(QWidget *parent = 0);
  ~ControlWindow();

  void bringToFront()
  {
    raise();
    activateWindow();
    setFocus();
  }

  /**
   * Register SHiFT code parser for querying new codes.
   *
   * @param game the game this parser searches codes for.
   * @param platform the platform this parser searches codes for.
   */
  void registerParser(const QString&, const QString&, const QIcon& = QIcon());

  // /**
  //  * Register SHiFT code parser for querying new codes.
  //  *
  //  * @param games the games this parser searches codes for.
  //  * @param platforms the platforms this parser searches codes for.
  //  */
  // void registerParser(const QStringList&,
  //                     const QStringList&);
public slots:
  void login();
  void loggedin(bool);

private:
  /**
   * Starts redeeming SHiFT codes
   */
  void start();
  /**
   * Stop redeeming SHiFT codes
   */
  void stop();

  /**
   * Redeem one SHiFT code
   */
  bool redeem();

private:
  Ui::ControlWindow *ui;
  WaitingSpinnerWidget* spinner;

  ShiftClient sClient;

  QString user;
};
