#include <iostream>
#include <string>
#include "Database.h"

using namespace std;

void addReview()
{
    int userId, productId, rating;
    string review;

    cout << "User ID: ";
    cin >> userId;

    cout << "Product ID: ";
    cin >> productId;

    cout << "Rating (1-5): ";
    cin >> rating;

    cout << "Review: ";
    cin >> review;

    PGconn* conn = connectDatabase();

    if (conn == nullptr)
        return;

    string query =
        "INSERT INTO reviews (user_id, product_id, rating, review) VALUES (" +
        to_string(userId) + ", " +
        to_string(productId) + ", " +
        to_string(rating) + ", '" +
        review + "')";

    PGresult* result = PQexec(conn, query.c_str());

    if (PQresultStatus(result) == PGRES_COMMAND_OK)
        cout << "Review saved in database!" << endl;
    else
        cout << "Review save failed!" << endl;

    PQclear(result);
    PQfinish(conn);
}

void getReviews()
{
    PGconn* conn = connectDatabase();

    if (conn == nullptr)
        return;

    PGresult* result = PQexec(
        conn,
        "SELECT id, user_id, product_id, rating, review FROM reviews"
    );

    if (PQresultStatus(result) == PGRES_TUPLES_OK)
    {
        for (int i = 0; i < PQntuples(result); i++)
        {
            cout << "Review ID: " << PQgetvalue(result, i, 0)
                 << " | User: " << PQgetvalue(result, i, 1)
                 << " | Product: " << PQgetvalue(result, i, 2)
                 << " | Rating: " << PQgetvalue(result, i, 3)
                 << " | Review: " << PQgetvalue(result, i, 4)
                 << endl;
        }
    }

    PQclear(result);
    PQfinish(conn);
}