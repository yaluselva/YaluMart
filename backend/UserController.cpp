#include <iostream>
#include <string>

using namespace std;

void registerUser()
{
    string name, email, password;

    cout << "Enter Name: ";
    cin >> name;

    cout << "Enter Email: ";
    cin >> email;

    cout << "Enter Password: ";
    cin >> password;

    cout << "User registered successfully!" << endl;
}

void loginUser()
{
    string email, password;

    cout << "Enter Email: ";
    cin >> email;

    cout << "Enter Password: ";
    cin >> password;

    cout << "Login successful!" << endl;
}