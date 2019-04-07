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

#ifdef __cplusplus

#include <type_traits>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <regex>

#include <QString>
// #include <QRectF>
// #include <QPointF>

#include "macros.hpp"

using std::endl;
#ifndef TIMESTAMP
# define DATESTAMP put_time(localtime(&time_now), "%y-%m-%d")
# define TIMESTAMP put_time(localtime(&time_now), "%OH:%OM:%OS")
#endif

#ifndef ASCII_START
#  define ASCII_START "["
#  define ASCII_SPECIAL (";1m")
#  define ASCII_RESET (ASCII_START"0m")
#  define ASCII_BOLD (ASCII_START"1m")
#  define ASCII_ITALICS (ASCII_START"3m")
#  define ASCII_UNDERLINE (ASCII_START"4m")
#  define ASCII_END_BOLD (ASCII_START"22m")
#  define ASCII_END_ITALICS (ASCII_START"23m")
#  define ASCII_END_UNDERLINE (ASCII_START"24m")

#  define ASCII_BLACK (ASCII_START"30m")
#  define ASCII_RED (ASCII_START"31m")
#  define ASCII_GREEN (ASCII_START"32m")
#  define ASCII_YELLOW (ASCII_START"33m")
#  define ASCII_BLUE (ASCII_START"34m")
#  define ASCII_MAGENTA (ASCII_START"35m")
#  define ASCII_CYAN (ASCII_START"36m")
#  define ASCII_WHITE (ASCII_START"37m")
#  define ASCII_BG_BLACK (ASCII_START"40m")
#  define ASCII_BG_RED (ASCII_START"41m")
#  define ASCII_BG_GREEN (ASCII_START"42m")
#  define ASCII_BG_YELLOW (ASCII_START"43m")
#  define ASCII_BG_BLUE (ASCII_START"44m")
#  define ASCII_BG_MAGENTA (ASCII_START"45m")
#  define ASCII_BG_CYAN (ASCII_START"46m")
#  define ASCII_BG_WHITE (ASCII_START"47m")
#endif


// #define SC sc:...
// static std::time_t time_now = time(nullptr);
// static std::time_t time_now = time(nullptr);
FENUM(ASCII,
      BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
      BG_BLACK, BG_RED, BG_GREEN, BG_YELLOW, BG_BLUE, BG_MAGENTA, BG_CYAN, BG_WHITE,
      RESET, BOLD, ITALICS, UNDERLINE, END_BOLD, END_ITALICS, END_UNDERLINE);
FENUM(cmd,
      x4,
      x3,
      timestamp,
      join,
      flush);

namespace logger {
struct Join {
  std::string str;
  template<typename T>
  Join(const T& _obj)
  {
    std::ostringstream tmp;
    tmp << _obj;
    str = tmp.str();
  }
};
}
#define JOIN logger::Join
#define JOINS logger::Join(" ")
#define JOINE logger::Join("")

namespace ashift {

using namespace std;

static const regex ansi_re(R"(\x1B\[([\d;]+)m)");

  class Logger {

    typedef void (*callback) (const string&, void* usrData);
  public:
    // enum cmd {
    //   comma,
    //   x4,
    //   x3,
    //   flush
    // };
  private:
    // using endl_type = decltype(std::endl);
    using endl_type = ostream&(ostream&);
    char _next_line;
    uint8_t _n_els;
    uint8_t _filler_els;
    ostream* os;
    string prefix;
    string filename;
    int line;

    iostream* strm;
    bool isFile;
    bool noAnsi;

    callback _callback;
    void* _usrData;

    chrono::time_point<chrono::system_clock> ch_now; // = chrono::system_clock::now();
    time_t time_now; // = chrono::system_clock::to_time_t(ch_now);
    tm* tm_now; // = localtime(&time_now);
    chrono::system_clock::time_point ch_tm_now; //= chrono::system_clock::from_time_t(mktime(tm_now));
    unsigned int ms;

    std::vector<cmd> commands;
    string filler;

    unsigned int calc_ms()
    {
      ch_now = chrono::system_clock::now();
      time_now = chrono::system_clock::to_time_t(ch_now);
      tm_now = localtime(&time_now);
      ch_tm_now= chrono::system_clock::from_time_t(mktime(tm_now));
      ms = chrono::duration_cast<chrono::microseconds>(ch_now - ch_tm_now).count();
      return ms;
    }

  public:
    /**
     * Constructs a Logger that logs to given ostream
     *
     * @param ostr pointer to ostream
     * @param _prefix prefix of logging output
     */
    Logger(ostream* ostr, string _prefix):
      os(ostr), _callback(0), _usrData(0),
      _next_line(1), _n_els(0), _filler_els(0), prefix(_prefix), filename(""), line(0), strm(0),
      isFile(false), noAnsi(false), filler("")
      {
        const char *env_p = std::getenv("TERM");
        noAnsi = env_p == nullptr; // no ansi codes for stupid terminals
        calc_ms();
      }

    /**
     * Constructs a Logger that logs to cout
     *
     * @param _prefix prefix of logging output
     */
    Logger(string _prefix):
      Logger(&cout, _prefix)
    {}


    /**
     * Constructs a Logger that logs to nowhere
     */
    Logger():
      os(0), _callback(0), _usrData(0), _next_line(0),
      _n_els(0), _filler_els(0), prefix("NULL"), filename(""), line(0),
      strm(0), isFile(false), noAnsi(false), filler("")
    {}

    /**
     * Constructs a Logger that logs to stringBuffer
     *
     * @param str pointer to stringbuf
     * @param _prefix prefix of logging output (default "Logger")
     */
    Logger(stringstream* str, string _prefix):
      Logger(_prefix)
    {toStr(str);}

    /**
     * Constructs a Logger that logs to a file
     *
     * @param file filename to log to
     * @param _prefix prefix of logging output (default "Logger")
     */
    Logger(const char* file, string _prefix):
      Logger(_prefix)
    {toFile(file);}

    Logger(callback cb, void* ud, string _prefix):
      Logger(_prefix)
    {withCallback(cb, ud);}

    ~Logger() {
      deleteStream();
    }

    bool isNull()
    { return prefix == "NULL"; }
    void deleteStream()
    {
      if (strm) {
        if (isFile)
          static_cast<fstream*>(strm)->close();
        delete strm;
        strm = 0;
      }
      isFile = false;
      noAnsi = false;
    }

    void withCallback(callback cb, void* ud)
    {
      // deleteStream();
      _callback = cb;
      _usrData = ud;
    }

    void toFile(const char* file)
    {
      deleteStream();
      isFile = true;
      noAnsi = true;

      fstream* fbuffer = new fstream;
      fbuffer->open(file, fstream::in | fstream::out | fstream::app);
      if (fbuffer->is_open()) {
        strm = fbuffer;
        os = fbuffer;
      } else {
        delete fbuffer;
      }
    }

    void toStr(stringstream* buffer)
    {
      deleteStream();

      strm = buffer;
      os = buffer;
    }

    void toNull()
    {
      toFile("/dev/null");
    }

    ostream* getOs()
    { return os; }

    template<typename T>
    Logger& add(const T& obj)
    {
      (*os) << obj;
      return *this;
    }

#define RETIFNULL if (isNull()) return *this;

    Logger& operator<<(logger::Join join)
    {
      RETIFNULL;
      filler = join.str;
      _filler_els = 0;
      return *this;
    }

    Logger& operator<<(cmd command)
    {
      RETIFNULL
      // non persistent commands
      switch (command) {
      case cmd::flush:
        this->flush();
        return *this;
      case cmd::timestamp:
        ostringstream tmp;
        calc_ms();
        string stmp = std::to_string(ms).append("0000");
        tmp << ASCII_CYAN << TIMESTAMP << ".";
        for (int i = 0; i < 4; ++i)
          tmp << stmp.at(i);
        tmp << ASCII_RESET;
        *this << tmp.str();
        return *this;
      }

      // persistent commands
      auto it = commands.begin();
      while (it != commands.end()) {
        if (*it == command) {
          commands.erase(it);
          return *this;
        }
        it++;
      }
      commands.push_back(command);
      _n_els = 0;
      return *this;
    }

    Logger& operator<<(endl_type endl)
    {
      RETIFNULL
      if (_callback)
        _callback("\n", _usrData);

      _next_line = 1;
      _n_els = 0;
      _filler_els = 0;
      commands.clear();
      (*os) << endl;
      return *this;
    }

    Logger& operator<<(ASCII c)
    {
      RETIFNULL
      Logger& l = *this;

      switch(c) {
      case(ASCII::BLACK): l << ASCII_BLACK; break;
      case(ASCII::RED): l << ASCII_RED; break;
      case(ASCII::GREEN): l << ASCII_GREEN; break;
      case(ASCII::YELLOW): l << ASCII_YELLOW; break;
      case(ASCII::BLUE): l << ASCII_BLUE; break;
      case(ASCII::MAGENTA): l << ASCII_MAGENTA; break;
      case(ASCII::CYAN): l << ASCII_CYAN; break;
      case(ASCII::WHITE): l << ASCII_WHITE; break;

      case(ASCII::BG_BLACK): l << ASCII_BG_BLACK; break;
      case(ASCII::BG_RED): l << ASCII_BG_RED; break;
      case(ASCII::BG_GREEN): l << ASCII_BG_GREEN; break;
      case(ASCII::BG_YELLOW): l << ASCII_BG_YELLOW; break;
      case(ASCII::BG_BLUE): l << ASCII_BG_BLUE; break;
      case(ASCII::BG_MAGENTA): l << ASCII_BG_MAGENTA; break;
      case(ASCII::BG_CYAN): l << ASCII_BG_CYAN; break;
      case(ASCII::BG_WHITE): l << ASCII_BG_WHITE; break;

      case(ASCII::RESET): l << ASCII_RESET; break;
      case(ASCII::BOLD): l << ASCII_BOLD; break;
      case(ASCII::ITALICS): l << ASCII_ITALICS; break;
      case(ASCII::UNDERLINE): l << ASCII_UNDERLINE; break;
      case(ASCII::END_BOLD): l << ASCII_END_BOLD; break;
      case(ASCII::END_ITALICS): l << ASCII_END_ITALICS; break;
      case(ASCII::END_UNDERLINE): l << ASCII_END_UNDERLINE; break;
      default: break;
      }

      return l;
    }

    Logger& operator<<(const QString& qstr)
    {
      RETIFNULL
      Logger& l = *this;
      l << qstr.toStdString();
      return l;
    }

    template<typename T>
    Logger& operator<<(const vector<T>& obj)
    {
      RETIFNULL
      Logger& l = *this;
      l << ASCII_RED << ASCII_SPECIAL << "{" << "vector " << ASCII_RESET;
      for (unsigned int i = 0; i < obj.size()-1; ++i) {
        l << obj[i] << ", ";
      }
      l << obj[obj.size()-1] << ASCII_MAGENTA << ASCII_SPECIAL << "}" << ASCII_RESET;
      return l;
    }

    Logger& operator<<(bool b)
    {
      RETIFNULL
      ostringstream tmp;
      if (b)
        tmp << ASCII_GREEN;
      else
        tmp << ASCII_RED;
      tmp << std::boolalpha << b;
      tmp << ASCII_RESET;

      Logger& l = *this;
      l << tmp.str();
      return l;
    }

    Logger& operator<<(const char c)
    {
      RETIFNULL
      Logger& l = *this;
      l << (int)c;
      return l;
    }

    template<typename T>
    Logger& operator<<(const T& obj) {
      RETIFNULL
      ostringstream tmp;
      if (_next_line) {
        // time_now = time(nullptr);
        calc_ms();
        string stmp = std::to_string(ms).append("0000");
        tmp << ASCII_CYAN << DATESTAMP << " " << TIMESTAMP << ".";
        for (int i = 0; i < 4; i++)
          tmp << stmp.at(i);
        tmp << ASCII_RESET;
        tmp << " [" << ASCII_BOLD << prefix << ASCII_END_BOLD << "] ";
        if (filename != "")
          tmp << filename << ":" << line;
        tmp << ASCII_RESET << ASCII_CYAN << ">> " << ASCII_RESET;
        tmp << obj;
        // tmp << std::boolalpha << obj;
      } else {
        for (cmd& c: commands)
        {
          if (_n_els > 0)
            switch (c) {
            case cmd::x3:
              if (_n_els % 3 == 0) tmp << "\n"; break;
            case cmd::x4:
              if (_n_els % 4 == 0) tmp << "\n"; break;
            default:
              break;
            }
        }
        if (!filler.empty() && _filler_els > 0)
          tmp << filler;
        tmp << obj;
      }

      if (noAnsi) {
        (*os) << regex_replace(tmp.str(), ansi_re, "");
      } else {
        (*os) << tmp.str();
      }

      ++_n_els;
      if (!filler.empty())
        ++_filler_els;

      if (_callback)
        _callback(tmp.str(), _usrData);
    ret:
      _next_line = 0;
      return *this;
    }

    Logger& operator()(string fn, int ln) {
      RETIFNULL
      filename = fn;
      line = ln;
      return *this;
    }

    void flush() {
      os->flush();
    }

    void out() {
      cout << &((*os) << endl);
      os->flush();
    }

  };

  extern Logger logger_error;
  extern Logger logger_info;
  extern Logger logger_debug;
  extern Logger logger_null;

#ifndef ERROR
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
# define NUL ashift::logger_null
# define ERROR ashift::logger_error(__FILENAME__, __LINE__)
#ifdef NOINFO
# define INFO NUL
#else
# define INFO ashift::logger_info
#endif
#ifdef NODEBUG
  # define DEBUG NUL
#else
  # define DEBUG ashift::logger_debug(__FILENAME__, __LINE__)
#endif
#endif
}
// #undef SC
#endif
