#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libpq-fe.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;
PGconn* connectDatabase() {
    const char* url = getenv("DATABASE_URL");

    if (!url) {
        cerr << "DATABASE_URL is not set\n";
        return nullptr;
    }

    PGconn* db = PQconnectdb(url);

    if (PQstatus(db) != CONNECTION_OK) {
        cerr << PQerrorMessage(db) << '\n';
        PQfinish(db);
        return nullptr;
    }
    PGresult* table = PQexec(db,
    "CREATE TABLE IF NOT EXISTS products ("
    "id SERIAL PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "price NUMERIC(10,2) NOT NULL DEFAULT 0,"
    "quantity INTEGER NOT NULL DEFAULT 0"
    ")"
);

if (PQresultStatus(table) != PGRES_COMMAND_OK) {
    cerr << "Products table error: "
         << PQerrorMessage(db) << '\n';
    PQclear(table);
    PQfinish(db);
    return nullptr;
}

PQclear(table);

    cout << "PostgreSQL connected\n";
    return db;
}

string jsonEscape(const string& s) {
    string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') continue;
        out += c;
    }
    return out;
}

void sendResponse(int client, const string& body,
                  const string& status = "200 OK") {
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "Content-Length: " + to_string(body.size()) + "\r\n\r\n" + body;

    size_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = send(client, response.data() + sent,
                         response.size() - sent, 0);
        if (n <= 0) break;
        sent += static_cast<size_t>(n);
    }
}

string getProducts(PGconn* db) {
    PGresult* r = PQexec(
        db, "SELECT id, name, price, quantity FROM products ORDER BY id"
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQclear(r);
        return "{\"error\":\"Failed to fetch products\"}";
    }

    string json = "[";
    for (int i = 0; i < PQntuples(r); ++i) {
        if (i) json += ",";
        json += "{\"id\":" + string(PQgetvalue(r, i, 0));
        json += ",\"name\":\"" + jsonEscape(PQgetvalue(r, i, 1)) + "\"";
        json += ",\"price\":" + string(PQgetvalue(r, i, 2));
        json += ",\"quantity\":" + string(PQgetvalue(r, i, 3)) + "}";
    }
    json += "]";
    PQclear(r);
    return json;
}

string addProduct(PGconn* db, const string& body) {
    // Accept a JSON object with name, price, and quantity.
    // This simple parser expects unescaped product names.
    size_t p = body.find("\"name\":\"");
    if (p == string::npos) return "{\"error\":\"Invalid name\"}";
    p += 8;
    size_t e = body.find('"', p);
    if (e == string::npos) return "{\"error\":\"Invalid name\"}";
    string name = body.substr(p, e - p);

    p = body.find("\"price\":");
    if (p == string::npos) return "{\"error\":\"Invalid price\"}";
    p += 8;
    size_t pe = body.find_first_of(",}", p);
    string price = body.substr(p, pe - p);

    p = body.find("\"quantity\":");
    if (p == string::npos) return "{\"error\":\"Invalid quantity\"}";
    p += 11;
    size_t qe = body.find_first_of(",}", p);
    string quantity = body.substr(p, qe - p);

    char* end = nullptr;
    double priceValue = strtod(price.c_str(), &end);
    if (!end || *end != '\0' || priceValue < 0)
        return "{\"error\":\"Invalid price\"}";

    long quantityValue = strtol(quantity.c_str(), &end, 10);
    if (!end || *end != '\0' || quantityValue < 0)
        return "{\"error\":\"Invalid quantity\"}";

    const char* values[] = {
        name.c_str(), price.c_str(), quantity.c_str()
    };

    PGresult* r = PQexecParams(
        db,
        "INSERT INTO products (name, price, quantity) VALUES ($1, $2, $3)",
        3, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        PQclear(r);
        return "{\"error\":\"Could not add product\"}";
    }

    PQclear(r);
    return "{\"message\":\"Product added successfully\"}";
}

string deleteProduct(PGconn* db, const string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == string::npos) return "{\"error\":\"Invalid product ID\"}";

    string id = path.substr(slash + 1);
    if (id.empty() || !all_of(id.begin(), id.end(),
                              [](unsigned char c) { return isdigit(c); }))
        return "{\"error\":\"Invalid product ID\"}";

    const char* values[] = { id.c_str() };
    PGresult* r = PQexecParams(
        db, "DELETE FROM products WHERE id = $1",
        1, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        PQclear(r);
        return "{\"error\":\"Product delete failed\"}";
    }

    bool deleted = string(PQcmdTuples(r)) != "0";
    PQclear(r);

    return deleted ? "{\"message\":\"Product deleted successfully\"}"
                   : "{\"error\":\"Product not found\"}";
}

int main() {
    PGconn* db = connectDatabase();
    if (!db) return 1;

    const char* portEnv = getenv("PORT");
    int port = portEnv ? atoi(portEnv) : 8080;
    if (port <= 0 || port > 65535) port = 8080;

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        cerr << "Socket creation failed\n";
        PQfinish(db);
        return 1;
    }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0 || listen(server, 20) < 0) {
        cerr << "Could not bind/listen on port " << port << '\n';
        close(server);
        PQfinish(db);
        return 1;
    }

    cout << "YaluMart API listening on port " << port << '\n';

    while (true) {
        int client = accept(server, nullptr, nullptr);
        if (client < 0) continue;

        string request;
        char buffer[4096];
        ssize_t n;
        while ((n = recv(client, buffer, sizeof(buffer), 0)) > 0) {
            request.append(buffer, static_cast<size_t>(n));
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != string::npos) {
                size_t contentLength = 0;
                size_t cp = request.find("Content-Length:");
                if (cp != string::npos && cp < headerEnd) {
                    cp += 15;
                    while (cp < headerEnd && request[cp] == ' ') ++cp;
                    contentLength = strtoul(request.c_str() + cp, nullptr, 10);
                }
                if (request.size() >= headerEnd + 4 + contentLength) break;
            }
            if (request.size() > 1024 * 1024) break;
        }

        size_t lineEnd = request.find("\r\n");
        string line = request.substr(0, lineEnd);
        size_t h = request.find("\r\n\r\n");
        string body = h == string::npos ? "" : request.substr(h + 4);
        string result;

        if (line.rfind("OPTIONS ", 0) == 0) {
            sendResponse(client, "{}");
        } else if (line.rfind("GET /api/products ", 0) == 0) {
            result = getProducts(db);
            sendResponse(client, result);
        } else if (line.rfind("POST /api/products/add ", 0) == 0) {
            result = addProduct(db, body);
            sendResponse(client, result);
        } else if (line.rfind("DELETE /api/products/", 0) == 0) {
            size_t start = line.find(' ') + 1;
            size_t end = line.find(' ', start);
            string path = line.substr(start, end - start);
            result = deleteProduct(db, path);
            sendResponse(client, result);
        } else {
            sendResponse(client, "{\"message\":\"YaluMart API is running\"}",
                         "404 Not Found");
        }

        close(client);
    }

    close(server);
    PQfinish(db);
    return 0;
}