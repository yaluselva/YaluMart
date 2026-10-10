#include <iostream>

using namespace std;

void createOrder()
{
    cout << "Creating new order..." << endl;

    int productId;
    int quantity;

    cout << "Enter Product ID: ";
    cin >> productId;

    cout << "Enter Quantity: ";
    cin >> quantity;

    cout << "Order created successfully!" << endl;
}

void getOrders()
{
    cout << "Displaying all orders..." << endl;
}

void updateOrderStatus()
{
    int orderId;
    string status;

    cout << "Enter Order ID: ";
    cin >> orderId;

    cout << "Enter New Status: ";
    cin >> status;

    cout << "Order status updated successfully!" << endl;
}