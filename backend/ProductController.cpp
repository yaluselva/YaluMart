#include <iostream>
#include <string>
#include "Database.h"

using namespace std;

void getProducts()
{
    PGconn* conn = connectDatabase();

    if (conn == nullptr)
        return;

    PGresult* result = PQexec(
        conn,
        "SELECT id, name, price, quantity FROM products"
    );

    if (PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        int rows = PQntuples(result);

        for (int i = 0; i < rows; i++)
        {
            cout << "ID: " << PQgetvalue(result, i, 0)
                 << " | Name: " << PQgetvalue(result, i, 1)
                 << " | Price: " << PQgetvalue(result, i, 2)
                 << " | Quantity: " << PQgetvalue(result, i, 3)
                 << endl;
        }
    }

    PQclear(result);
    PQfinish(conn);
}

void addProduct()
{
    string name;
    double price;
    int quantity;

    cout << "Product Name: ";
    cin >> name;

    cout << "Product Price: ";
    cin >> price;

    cout << "Product Quantity: ";
    cin >> quantity;

    PGconn* conn = connectDatabase();

    if (conn == nullptr)
        return;

    string query =
        "INSERT INTO products (name, price, quantity) VALUES ('" +
        name + "', " + to_string(price) + ", " +
        to_string(quantity) + ")";

    PGresult* result = PQexec(conn, query.c_str());

    if (PQresultStatus(result) == PGRES_COMMAND_OK)
        cout << "Product saved in database!" << endl;
    else
        cout << "Product save failed!" << endl;

    PQclear(result);
    PQfinish(conn);
}

void deleteProduct()
{
    int productId;

    cout << "Enter Product ID: ";
    cin >> productId;

    PGconn* conn = connectDatabase();

    if (conn == nullptr)
        return;

    string query =
        "DELETE FROM products WHERE id = " +
        to_string(productId);

    PGresult* result = PQexec(conn, query.c_str());

    if (PQresultStatus(result) == PGRES_COMMAND_OK)
        cout << "Product deleted from database!" << endl;
    else
        cout << "Product delete failed!" << endl;

    PQclear(result);
    PQfinish(conn);
}