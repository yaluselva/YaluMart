#include "Database.h"

PGconn* connectDatabase()
{
    PGconn* conn = PQconnectdb(
        "host=localhost port=5432 dbname=yalu_mart user=postgres"
    );

    return conn;
}