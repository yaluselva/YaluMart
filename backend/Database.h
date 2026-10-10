#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>

PGconn* connectDatabase();

#endif