#include <iostream>
#include "Database.h"

using namespace std;

void registerUser();
void getProducts();
void addProduct();
void deleteProduct();
void createOrder();
void getOrders();
void updateOrderStatus();
void addReview();
void getReviews();

int main()
{
    cout << "============================" << endl;
    cout << "       YaluMart Backend" << endl;
    cout << "============================" << endl;

    PGconn* conn = connectDatabase();

    if (conn == nullptr)
    {
        cout << "Database connection failed!" << endl;
        return 1;
    }

    cout << "Database connection successful!" << endl;

    PQfinish(conn);

    cout << "\nYaluMart Backend Ready!" << endl;

    return 0;
}