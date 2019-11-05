#pragma once

#include <key.hpp>
#include <controlwindow.hpp>

class CodeParser: QObject
{
  Q_OBJECT

public:
  typedef std::function<void(bool)> Callback;

  CodeParser()
  {}
  CodeParser(ControlWindow& cw, Game _g, Platform _p, const QIcon& _i = QIcon()) :
    CodeParser(cw, { _g, Game::NONE }, { _p, Platform::NONE }, { _i })
  {}

  // CodeParser(ControlWindow& cw, Game _gs[], Platform _ps[], const QIcon& _i = QIcon()):
  CodeParser(ControlWindow& cw, std::initializer_list<Game> _gs,
    std::initializer_list<Platform> _ps, std::initializer_list<QIcon> _i) :
    CodeParser()
  {
    // register this parser
    auto icon_it = _i.begin();

    for (Game _g : _gs) {
      if (_g == Game::NONE) continue;
      QIcon icn;
      if (icon_it != _i.end()) {
        icn = *icon_it;
        ++icon_it;
      }
      for (Platform _p : _ps) {
        if (_p == Platform::NONE) continue;
        cw.registerParser(_g, _p, this, icn);
      }
    }
  }

public slots:
  /**
   * Parse keys and add them to the passed ShiftCollection.
   *
   * @param coll Output variable to contain the shift codes after parsing
   * @param cb Callback to trigger after parsing is done (receives bool success-value)
   */
  virtual void parseKeys(ShiftCollection& /* out */, Callback = 0) = 0;

protected:
};
