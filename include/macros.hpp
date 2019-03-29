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

#define ENUM_FUNCS(name, first)                                   \
  typedef name ## _enum::name ## _enum name;                      \
  inline bool is_ ## name (int i) {                               \
    return (i >= ((int) name::first) && i < ((int) name::SIZE));  \
  }                                                               \
  inline name to ## name(int i) {                                 \
    if (!is_ ## name(i)) return name::NONE;                       \
    return (name)i;                                               \
  }

// scoped enum that is explicitly convertible to int
#define FENUM(name, first, ...)                 \
  namespace name ## _enum {                     \
    enum name ## _enum {                        \
      first,                                    \
        __VA_ARGS__  ,                          \
        SIZE,                                   \
        NONE                                    \
        };                                      \
  }                                             \
  ENUM_FUNCS(name, first)

/*
FENUM(Foo,
      ASD,
      QWE)
=>
  namespace Foo_enum {
    enum Foo_enum {
      ASD,
      QWE
    };
  }
typedef Foo_enum::Foo_enum Foo;
*/

#define FENUM_T(name, type, first, ...)         \
  namespace name ## _enum {                     \
    enum name ## _enum : type {                 \
      first,                                    \
      __VA_ARGS__,                              \
        SIZE,                                   \
        NONE                                    \
        };                                      \
  }                                             \
  ENUM_FUNCS(name, first)
