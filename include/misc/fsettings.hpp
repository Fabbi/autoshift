/*****************************************************************************
 **
 ** Copyright (C) 2018 Fabian Schweinfurth
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

#include <QMap>
#include <QVariant>
#include <QSettings>

#include <QWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>

#include <QEvent>
// TODO autocomplete
#include <QCompleter>
#include <QStringListModel>

#include <misc/logger.hpp>

#define FSETTINGS FSettings::get()
#define LOADSTATE(t) _loadState(t* widget, const QString& path, const QVariant& var)

class FSettings : public QObject, public QMap<QString, QVariant>
{
  Q_OBJECT

public:
  static FSettings& get()
  {
    static FSettings inst;
    return inst;
  }

private:
  FSettings() :
    settings("fsettings.conf", QSettings::NativeFormat)
  {}

  ~FSettings()
  {
    settings.sync();
  }

public:
  FSettings(FSettings const&)      = delete;
  void operator=(FSettings const&) = delete;

private:

  inline QString getPath(QWidget* w)
  {
    QStringList l;
    QWidget* tmp = w;
    while (tmp) {
      l << tmp->objectName();
      tmp = tmp->parentWidget();
    }
    return l.join("/");
  }

  template<typename T>
  bool _precondition(T* widget)
  { return true; }

  bool _precondition(QCheckBox* cb)
  { return !cb->isTristate(); }

  void LOADSTATE(QComboBox)
  {
    widget->setCurrentIndex(var.toInt());
  }

  void LOADSTATE(QSpinBox)
  {
    widget->setValue(var.toInt());
    // QLineEdit* edit = widget->findChild<QLineEdit*>();
    // register_completer(edit, path);
    // widget->installEventFilter(this);
  }

  void LOADSTATE(QCheckBox)
  {
    // cb->setCheckState(saved_val.value<Qt::CheckState>());
    if (widget->isEnabled())
      widget->setChecked(var.toBool());
  }

  void LOADSTATE(QLineEdit)
  {
    widget->setText(var.value<QString>());

    // if (widget->objectName() == "qt_spinbox_lineedit") return;

    // setup autocomplete
    register_completer(widget, path);
    widget->installEventFilter(this);
  }

  void register_completer(QLineEdit* widget, const QString& path) {
    QStringList lis = settings.value("autocomplete/" + path,
                                     QStringList(widget->text())).value<QStringList>();
    completion_lists.insert(widget, lis);

    QCompleter* comp = new QCompleter(lis);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer.insert(widget, comp);

    widget->setCompleter(comp);
    connect(widget, SIGNAL(editingFinished()),
            this, SLOT(obj_editingFinished()));

  }

public:
// fake switch/case for readability..
#define CASE(tn) if (typ_name == tn)
#define BREAK return
#define IS(t) std::is_same<T, t>::value

  void setValue(const QString& key, const QVariant& value)
  { insert(key, value); }

  template<typename T, typename std::enable_if<IS(QCheckBox) ||
                                               IS(QLineEdit) ||
                                               IS(QSpinBox) ||
                                               IS(QComboBox)
                                               , int>::type=0>
  void registerWidget(T* widget)
  {
    const QString typ_name = QString(widget->metaObject()->className());

    // check for preconditions
    if (!_precondition(widget)) return;

    registered_widgets.insert(widget, typ_name);

    // connect to destroyed signal
    connect(widget, SIGNAL(destroyed(QObject*)),
            this, SLOT(obj_destroyed(QObject*)));

    // see if there is a previously saved value in the settings
    QString path = getPath(widget);
    QVariant saved_val = settings.value("state/" + path);

    if (saved_val.isValid())
      _loadState(widget, path, saved_val);

  }

  /* observe widgets and register their changes in a setting */
  template<typename T>
  void observe(T* widget, const QString& opt)
  {
    ERROR << "can't observe this kind of type" << endl;
  }

  void observe(QCheckBox* w, const QString& opt)
  {
    setValue(opt, w->isChecked());
    connect(w, &QCheckBox::toggled,
            [&, opt](bool v)
            {setValue(opt, v);});
  }
  void observe(QLineEdit* w, const QString& opt)
  {
    setValue(opt, w->text());
    connect(w, &QLineEdit::textChanged,
            [&, opt](const QString& v)
            {setValue(opt, v);});
  }
  void observe(QSpinBox* w, const QString& opt)
  {
    setValue(opt, w->value());
    connect(w, QOverload<int>::of(&QSpinBox::valueChanged),
            [&, opt](int v)
            {setValue(opt, v);});
  }
  template<typename T>
  void observe(QComboBox* w, const QString& opt)
  {
    setValue(opt, w->currentIndex());
    connect(w, QOverload<T>::of(&QComboBox::currentIndexChanged),
            [&, opt](T v)
            {setValue(opt, v);});
  }

  /***********************************************************/

  bool eventFilter(QObject* obj, QEvent* ev)
  {
    // DEBUG << obj->objectName() << " " << hlp::ev2str(ev->type()) << endl;
    if (ev->type() == QEvent::FocusIn) {
      QLineEdit* edit;
      if (qobject_cast<QSpinBox*>(obj))
        edit = obj->findChild<QLineEdit*>();
      else
        edit = qobject_cast<QLineEdit*>(obj);

      edit->completer()->complete();
    }
    return false;
  }

private:
  QMap<QWidget*, QString> registered_widgets;
  QSettings settings;

  QMap<QWidget*, QStringList> completion_lists;
  QMap<QWidget*, QCompleter*> completer;

private slots:
  /**
   * After `Return` or `Enter` or `Focus loss`.
   *
   * Add new value to completion list
   */
  void obj_editingFinished()
  {
    QLineEdit* le = qobject_cast<QLineEdit*>(QObject::sender());
    QString s = le->text();
    if (s.isEmpty()) return;

    QStringList& l = completion_lists[le];
    l.removeAll(s);
    l.insert(0, s);
    ((QStringListModel*)completer[le]->model())->setStringList(l);
  }

  /**
   * After destroying object, write its state to config object
   */
  void obj_destroyed(QObject* obj)
  {
    QWidget* w = qobject_cast<QWidget*>(obj);
    QString p = getPath(w);

    const QString typ_name = registered_widgets.value(w, "");

    // we need this hacky workaround because `obj` is only a `QWidget`
    // at this point. `qobject_cast` will always return 0x00
    CASE("QComboBox") {
      QComboBox* cb = static_cast<QComboBox*>(w);
      settings.setValue("state/" + p, cb->currentIndex());
      BREAK;
    }
    CASE("QLineEdit") {
      QLineEdit* edit = static_cast<QLineEdit*>(w);
      settings.setValue("state/" + p, edit->text());
      settings.setValue("autocomplete/" + p, completion_lists[edit]);
      BREAK;
    }
    CASE("QCheckBox") {
      QCheckBox* cb = static_cast<QCheckBox*>(w);
      // settings.setValue(p + "/state", cb->checkState());
      settings.setValue("state/" + p, cb->isChecked());
      BREAK;
    }
    CASE("QSpinBox") {
      QSpinBox* sb = static_cast<QSpinBox*>(w);
      settings.setValue("state/" + p, sb->value());
      settings.setValue("autocomplete/" + p, completion_lists[sb]);
      BREAK;
    }
  }
};

#undef IS
#undef BREAK
#undef CASE
#undef LOADSTATE
