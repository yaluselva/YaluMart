#include <iostream>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <libpq-fe.h>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

// ===============================
// PostgreSQL Connection
// ===============================
PGconn* connectDatabase()
{
PGconn* conn = PQconnectdb(
    "host=localhost "
    "port=5432 "
    "dbname=yalu_mart "
    "user=postgres "
    "password=yalu@sm7126"
);    

    if (PQstatus(conn) != CONNECTION_OK)
    {
        cout << "Database connection failed!" << endl;
        cout << PQerrorMessage(conn) << endl;

        PQfinish(conn);
        return nullptr;
    }

    cout << "PostgreSQL Connected Successfully!" << endl;

    return conn;
}

// ===============================
// Send HTTP Response
// ===============================
void sendResponse(
    SOCKET clientSocket,
    const string& body,
    const string& status = "200 OK"
)
{
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: " + to_string(body.length()) + "\r\n"
        "\r\n" +
        body;

    send(
        clientSocket,
        response.c_str(),
        response.length(),
        0
    );
}

// ===============================
// GET PRODUCTS
// ===============================
string getProducts(PGconn* db)
{
    PGresult* result = PQexec(
        db,
        "SELECT id, name, price, quantity "
        "FROM products ORDER BY id"
    );

    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        PQclear(result);

        return "{\"error\":\"Failed to fetch products\"}";
    }

    string json = "[";

    int rows = PQntuples(result);

    for (int i = 0; i < rows; i++)
    {
        if (i > 0)
            json += ",";

        json += "{";

        json += "\"id\":";
        json += PQgetvalue(result, i, 0);
        json += ",";

        json += "\"name\":\"";
        json += PQgetvalue(result, i, 1);
        json += "\",";

        json += "\"price\":";
        json += PQgetvalue(result, i, 2);
        json += ",";

        json += "\"quantity\":";
        json += PQgetvalue(result, i, 3);

        json += "}";
    }

    json += "]";

    PQclear(result);

    return json;
}

// ===============================
// ADD PRODUCT
// ===============================
string addProduct(PGconn* db, const string& body)
{
    // Expected JSON:
    // {"name":"Laptop","price":45000,"quantity":5}

    size_t nameStart = body.find("\"name\":\"");

    if (nameStart == string::npos)
        return "{\"error\":\"Invalid product name\"}";

    nameStart += 8;

    size_t nameEnd = body.find("\"", nameStart);

    if (nameEnd == string::npos)
        return "{\"error\":\"Invalid product name\"}";

    string name = body.substr(
        nameStart,
        nameEnd - nameStart
    );

    size_t priceStart = body.find("\"price\":");

    if (priceStart == string::npos)
        return "{\"error\":\"Invalid price\"}";

    priceStart += 8;

    size_t priceEnd = body.find(",", priceStart);

    string priceText;

    if (priceEnd == string::npos)
        priceText = body.substr(priceStart);
    else
        priceText = body.substr(
            priceStart,
            priceEnd - priceStart
        );

    size_t quantityStart = body.find("\"quantity\":");

    if (quantityStart == string::npos)
        return "{\"error\":\"Invalid quantity\"}";

    quantityStart += 11;

    size_t quantityEnd = body.find("}", quantityStart);

    string quantityText =
        body.substr(
            quantityStart,
            quantityEnd - quantityStart
        );

    string query =
        "INSERT INTO products "
        "(name, price, quantity) VALUES ('" +
        name + "', " +
        priceText + ", " +
        quantityText + ")";

    PGresult* result = PQexec(
        db,
        query.c_str()
    );

    if (PQresultStatus(result) != PGRES_COMMAND_OK)
    {
        string error = PQerrorMessage(db);

        PQclear(result);

        return "{\"error\":\"" + error + "\"}";
    }

    PQclear(result);

    return "{\"message\":\"Product added successfully\"}";
}

// ===============================
// DELETE PRODUCT
// ===============================
string deleteProduct(PGconn* db, int productId)
{
    string query =
        "DELETE FROM products WHERE id = " +
        to_string(productId);

    PGresult* result = PQexec(
        db,
        query.c_str()
    );

    if (PQresultStatus(result) != PGRES_COMMAND_OK)
    {
        PQclear(result);

        return "{\"error\":\"Product delete failed\"}";
    }

    PQclear(result);

    return "{\"message\":\"Product deleted successfully\"}";
}

// ===============================
// MAIN SERVER
// ===============================
int main()
{
    cout << "Starting YaluMart Backend..." << endl;

    // Connect PostgreSQL
    PGconn* db = connectDatabase();

    if (db == nullptr)
    {
        cout << "Backend stopped because database connection failed."
             << endl;

        return 1;
    }

    // Start Windows Socket
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        cout << "Winsock startup failed!" << endl;

        PQfinish(db);

        return 1;
    }

    SOCKET serverSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (serverSocket == INVALID_SOCKET)
    {
        cout << "Socket creation failed!" << endl;

        PQfinish(db);
        WSACleanup();

        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(
        serverSocket,
        (sockaddr*)&serverAddress,
        sizeof(serverAddress)
    ) == SOCKET_ERROR)
    {
        cout << "Bind failed!" << endl;

        closesocket(serverSocket);
        PQfinish(db);
        WSACleanup();

        return 1;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR)
    {
        cout << "Listen failed!" << endl;

        closesocket(serverSocket);
        PQfinish(db);
        WSACleanup();

        return 1;
    }

    cout << endl;
    cout << "================================" << endl;
    cout << "        YaluMart Backend" << endl;
    cout << "================================" << endl;
    cout << "Server running on port 8080" << endl;
    cout << "Database: yalu_mart" << endl;
    cout << "================================" << endl;
    cout << endl;

    while (true)
    {
        SOCKET clientSocket =
            accept(
                serverSocket,
                nullptr,
                nullptr
            );

        if (clientSocket == INVALID_SOCKET)
            continue;

        char buffer[8192] = {};

        int received =
            recv(
                clientSocket,
                buffer,
                sizeof(buffer) - 1,
                0
            );

        if (received <= 0)
        {
            closesocket(clientSocket);
            continue;
        }

        buffer[received] = '\0';

        string request(buffer);

        string responseBody;

        // =========================
        // OPTIONS - CORS
        // =========================
        if (request.find("OPTIONS") == 0)
        {
            sendResponse(
                clientSocket,
                "{}"
            );
        }

        // =========================
        // GET PRODUCTS
        // =========================
        else if (
            request.find("GET /api/products") == 0
        )
        {
            responseBody =
                getProducts(db);

            sendResponse(
                clientSocket,
                responseBody
            );
        }

        // =========================
        // POST ADD PRODUCT
        // =========================
        else if (
            request.find("POST /api/products/add") == 0
        )
        {
            size_t bodyStart =
                request.find("\r\n\r\n");

            string body;

            if (bodyStart != string::npos)
            {
                body =
                    request.substr(
                        bodyStart + 4
                    );
            }

            responseBody =
                addProduct(
                    db,
                    body
                );

            sendResponse(
                clientSocket,
                responseBody
            );
        }

        // =========================
        // DELETE PRODUCT
        // =========================
        else if (
            request.find("DELETE /api/products/") == 0
        )
        {
            string path =
                request.substr(
                    21
                );

            size_t space =
                path.find(" ");

            if (space != string::npos)
                path =
                    path.substr(
                        0,
                        space
                    );

            int productId =
                stoi(path);

            responseBody =
                deleteProduct(
                    db,
                    productId
                );

            sendResponse(
                clientSocket,
                responseBody
            );
        }

        // =========================
        // UNKNOWN API
        // =========================
        else
        {
            responseBody =
                "{\"message\":\"YaluMart Backend is working!\"}";

            sendResponse(
                clientSocket,
                responseBody
            );
        }

        closesocket(clientSocket);
    }

    closesocket(serverSocket);

    PQfinish(db);

    WSACleanup();

    return 0;
}