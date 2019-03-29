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
private:
  /**
   * Start the Magic.
   *
   * Starts redeeming SHiFT codes
   */
  void start();

private:
  Ui::ControlWindow *ui;

};
