#ifndef DATABASE_IMPL_HPP_INCLUDED
#define DATABASE_IMPL_HPP_INCLUDED

#ifdef DATABASE_MYSQL
#include "socket.hpp"

#ifdef MARIADB_CC_2_1_0_WORKAROUND
#if defined(__EMX__) || !defined(HAVE_UINT)
typedef unsigned int uint;
#endif
#endif

#include <mysql.h>
#include <errmsg.h>
#endif
#ifdef DATABASE_SQLITE
#include <sqlite3.h>
#endif

#endif
