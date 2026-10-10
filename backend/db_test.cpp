#include <iostream>
#include <libpq-fe.h>

using namespace std;

int main()
{
    PGconn* conn = PQconnectdb(
        "host=localhost port=5432 dbname=yalu_mart user=postgres"
    );

    if (PQstatus(conn) == CONNECTION_OK)
    {
        cout << "PostgreSQL Connected Successfully!" << endl;
        cout << "Database: yalu_mart" << endl;
    }
    else
    {
        cout << "Connection Failed!" << endl;
        cout << PQerrorMessage(conn) << endl;
    }

    PQfinish(conn);

    return 0;
}