
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
#include <cctype>

using namespace std;

PGconn* connectDatabase() {
    const char* url = getenv("DATABASE_URL");
    if (!url) {
        cerr << "DATABASE_URL is not set\n";
        return nullptr;
    }

    PGconn* db = PQconnectdb(url);

    if (PQstatus(db) != CONNECTION_OK) {
        cerr << "Database connection failed: "
             << PQerrorMessage(db) << '\n';
        PQfinish(db);
        return nullptr;
    }

    const char* statements[] = {
        "CREATE EXTENSION IF NOT EXISTS pgcrypto",

        "CREATE TABLE IF NOT EXISTS products ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "price NUMERIC(10,2) NOT NULL DEFAULT 0,"
        "quantity INTEGER NOT NULL DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS users ("
        "id SERIAL PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "email TEXT NOT NULL UNIQUE,"
        "password_hash TEXT NOT NULL,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",

        "CREATE TABLE IF NOT EXISTS orders ("
        "id BIGSERIAL PRIMARY KEY,"
        "customer_name TEXT NOT NULL,"
        "address TEXT NOT NULL,"
        "phone TEXT NOT NULL,"
        "items JSONB NOT NULL,"
        "total NUMERIC(10,2) NOT NULL DEFAULT 0,"
        "status TEXT NOT NULL DEFAULT 'Pending',"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"
    };

    for (const char* sql : statements) {
        PGresult* r = PQexec(db, sql);

        if (PQresultStatus(r) != PGRES_COMMAND_OK) {
            cerr << "Database initialization failed: "
                 << PQresultErrorMessage(r) << '\n';
            PQclear(r);
            PQfinish(db);
            return nullptr;
        }

        PQclear(r);
    }

    cout << "PostgreSQL connected\n";
    return db;
}

string jsonEscape(const string& s) {
    string out;

    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 15];
                    out += hex[c & 15];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }

    return out;
}

string jsonString(const string& body, const string& key) {
    string pattern = "\"" + key + "\"";
    size_t p = body.find(pattern);

    if (p == string::npos) return "";

    p = body.find(':', p + pattern.size());
    if (p == string::npos) return "";

    ++p;

    while (p < body.size() &&
           isspace(static_cast<unsigned char>(body[p]))) ++p;

    if (p >= body.size() || body[p] != '"') return "";

    ++p;
    string value;
    bool escaped = false;

    for (; p < body.size(); ++p) {
        char c = body[p];

        if (escaped) {
            switch (c) {
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case '"': value += '"'; break;
                case '\\': value += '\\'; break;
                default: value += c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return value;
        } else {
            value += c;
        }
    }

    return "";
}

string jsonNumber(const string& body, const string& key) {
    string pattern = "\"" + key + "\"";
    size_t p = body.find(pattern);

    if (p == string::npos) return "";

    p = body.find(':', p + pattern.size());
    if (p == string::npos) return "";

    ++p;

    while (p < body.size() &&
           isspace(static_cast<unsigned char>(body[p]))) ++p;

    size_t end = body.find_first_of(",}", p);
    if (end == string::npos) return "";

    string value = body.substr(p, end - p);

    while (!value.empty() &&
           isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    return value;
}

// Extract a complete JSON object or array, including nested values.
string jsonRawValue(const string& body, const string& key) {
    string pattern = "\"" + key + "\"";
    size_t p = body.find(pattern);

    if (p == string::npos) return "";

    p = body.find(':', p + pattern.size());
    if (p == string::npos) return "";

    ++p;

    while (p < body.size() &&
           isspace(static_cast<unsigned char>(body[p]))) ++p;

    if (p >= body.size() ||
        (body[p] != '[' && body[p] != '{')) return "";

    size_t start = p;
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (; p < body.size(); ++p) {
        char c = body[p];

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
        } else if (c == '[' || c == '{') {
            ++depth;
        } else if (c == ']' || c == '}') {
            --depth;

            if (depth == 0) {
                return body.substr(start, p - start + 1);
            }
        }
    }

    return "";
}

bool validPositiveId(const string& id) {
    if (id.empty()) return false;

    return all_of(id.begin(), id.end(), [](unsigned char c) {
        return isdigit(c);
    });
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
        "Content-Length: " + to_string(body.size()) +
        "\r\n\r\n" + body;

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
        db, "SELECT id, name, price, quantity "
            "FROM products ORDER BY id"
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        cerr << PQresultErrorMessage(r) << '\n';
        PQclear(r);
        return "{\"success\":false,\"message\":\"Failed to fetch products\"}";
    }

    string json = "[";

    for (int i = 0; i < PQntuples(r); ++i) {
        if (i) json += ",";

        json += "{\"id\":" + string(PQgetvalue(r, i, 0));
        json += ",\"name\":\"" + jsonEscape(PQgetvalue(r, i, 1)) + "\"";
        json += ",\"price\":" + string(PQgetvalue(r, i, 2));
        json += ",\"quantity\":" + string(PQgetvalue(r, i, 3));
        json += "}";
    }

    json += "]";
    PQclear(r);
    return json;
}

string addProduct(PGconn* db, const string& body) {
    string name = jsonString(body, "name");
    string price = jsonNumber(body, "price");
    string quantity = jsonNumber(body, "quantity");

    if (name.empty() || price.empty() || quantity.empty()) {
        return "{\"success\":false,\"message\":\"Name, price and quantity are required\"}";
    }

    char* end = nullptr;
    double pv = strtod(price.c_str(), &end);

    if (end == price.c_str() || *end != '\0' ||
        pv < 0 || !isfinite(pv)) {
        return "{\"success\":false,\"message\":\"Invalid price\"}";
    }

    long qv = strtol(quantity.c_str(), &end, 10);

    if (end == quantity.c_str() || *end != '\0' || qv < 0) {
        return "{\"success\":false,\"message\":\"Invalid quantity\"}";
    }

    const char* values[] = {
        name.c_str(), price.c_str(), quantity.c_str()
    };

    PGresult* r = PQexecParams(
        db,
        "INSERT INTO products(name,price,quantity) VALUES($1,$2,$3)",
        3, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        cerr << PQresultErrorMessage(r) << '\n';
        PQclear(r);
        return "{\"success\":false,\"message\":\"Could not add product\"}";
    }

    PQclear(r);
    return "{\"success\":true,\"message\":\"Product added successfully\"}";
}

string deleteProduct(PGconn* db, const string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == string::npos) {
        return "{\"success\":false,\"message\":\"Invalid product ID\"}";
    }

    string id = path.substr(slash + 1);

    if (!validPositiveId(id)) {
        return "{\"success\":false,\"message\":\"Invalid product ID\"}";
    }

    const char* values[] = { id.c_str() };

    PGresult* r = PQexecParams(
        db, "DELETE FROM products WHERE id=$1",
        1, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"Product delete failed\"}";
    }

    bool deleted = string(PQcmdTuples(r)) != "0";
    PQclear(r);

    if (!deleted) {
        return "{\"success\":false,\"message\":\"Product not found\"}";
    }

    return "{\"success\":true,\"message\":\"Product deleted successfully\"}";
}

string registerUser(PGconn* db, const string& body) {
    string name = jsonString(body, "name");
    string email = jsonString(body, "email");
    string password = jsonString(body, "password");

    if (name.empty() || email.empty() || password.empty()) {
        return "{\"success\":false,\"message\":\"Please fill all fields\"}";
    }

    if (password.size() < 8) {
        return "{\"success\":false,\"message\":\"Password must contain at least 8 characters\"}";
    }

    const char* values[] = {
        name.c_str(), email.c_str(), password.c_str()
    };

    PGresult* r = PQexecParams(
        db,
        "INSERT INTO users(name,email,password_hash) "
        "VALUES($1,$2,crypt($3,gen_salt('bf'))) "
        "RETURNING id,name,email",
        3, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        string error = PQresultErrorField(r, PG_DIAG_SQLSTATE)
            ? PQresultErrorField(r, PG_DIAG_SQLSTATE) : "";
        PQclear(r);

        if (error == "23505") {
            return "{\"success\":false,\"message\":\"Email is already registered\"}";
        }

        return "{\"success\":false,\"message\":\"Registration failed\"}";
    }

    string id = PQgetvalue(r, 0, 0);
    string n = jsonEscape(PQgetvalue(r, 0, 1));
    string e = jsonEscape(PQgetvalue(r, 0, 2));
    PQclear(r);

    return "{\"success\":true,\"message\":\"Registration successful\","
           "\"user\":{\"id\":" + id + ",\"name\":\"" + n +
           "\",\"email\":\"" + e + "\"}}";
}

string loginUser(PGconn* db, const string& body) {
    string email = jsonString(body, "email");
    string password = jsonString(body, "password");

    if (email.empty() || password.empty()) {
        return "{\"success\":false,\"message\":\"Email and password are required\"}";
    }

    const char* values[] = { email.c_str(), password.c_str() };

    PGresult* r = PQexecParams(
        db,
        "SELECT id,name,email FROM users "
        "WHERE email=$1 AND password_hash=crypt($2,password_hash)",
        2, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQcl
        f (PQntuples(r) == 0) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"Invalid email or password\"}";
    }

    string id = PQgetvalue(r, 0, 0);
    string n = jsonEscape(PQgetvalue(r, 0, 1));
    string e = jsonEscape(PQgetvalue(r, 0, 2));
    PQclear(r);

    return "{\"success\":true,\"message\":\"Login successful\","
           "\"user\":{\"id\":" + id + ",\"name\":\"" + n +
           "\",\"email\":\"" + e + "\"}}";
}

// Create an order in PostgreSQL.
string createOrder(PGconn* db, const string& body) {
    string name = jsonString(body, "customerName");
    if (name.empty()) name = jsonString(body, "name");

    string address = jsonString(body, "address");
    string phone = jsonString(body, "phone");
    string total = jsonNumber(body, "total");
    string items = jsonRawValue(body, "items");

    if (name.empty() || address.empty() || phone.empty() ||
        total.empty() || items.empty()) {
        return "{\"success\":false,\"message\":\"Customer details, items and total are required\"}";
    }

    if (items.front() != '[' || items.back() != ']') {
        return "{\"success\":false,\"message\":\"Items must be a JSON array\"}";
    }

    char* end = nullptr;
    double totalValue = strtod(total.c_str(), &end);

    if (end == total.c_str() || *end != '\0' ||
        totalValue < 0 || !isfinite(totalValue)) {
        return "{\"success\":false,\"message\":\"Invalid order total\"}";
    }

    const char* values[] = {
        name.c_str(), address.c_str(), phone.c_str(),
        items.c_str(), total.c_str()
    };

    PGresult* r = PQexecParams(
        db,
        "INSERT INTO orders(customer_name,address,phone,items,total) "
        "VALUES($1,$2,$3,$4::jsonb,$5) RETURNING id,status,created_at",
        5, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        cerr << "Order insert failed: " << PQresultErrorMessage(r) << '\n';
        PQclear(r);
        return "{\"success\":false,\"message\":\"Could not save order\"}";
    }

    string id = PQgetvalue(r, 0, 0);
    string status = jsonEscape(PQgetvalue(r, 0, 1));
    string created = jsonEscape(PQgetvalue(r, 0, 2));
    PQclear(r);

    return "{\"success\":true,\"message\":\"Order placed successfully\","
           "\"order\":{\"id\":" + id + ",\"customerName\":\"" +
           jsonEscape(name) + "\",\"address\":\"" + jsonEscape(address) +
           "\",\"phone\":\"" + jsonEscape(phone) +
           "\",\"items\":" + items + ",\"total\":" + total +
           ",\"status\":\"" + status + "\",\"created_at\":\"" +
           created + "\"}}";
}

// Retrieve orders, including their JSON items.
string getOrders(PGconn* db) {
    PGresult* r = PQexec(
        db,
        "SELECT id,customer_name,address,phone,items::text,total,status,"
        "created_at::text FROM orders ORDER BY id DESC"
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        cerr << "Orders query failed: " << PQresultErrorMessage(r) << '\n';
        PQclear(r);
        return "{\"success\":false,\"message\":\"Failed to fetch orders\"}";
    }

    string json = "[";

    for (int i = 0; i < PQntuples(r); ++i) {
        if (i) json += ",";

        json += "{\"id\":" + string(PQgetvalue(r, i, 0));
        json += ",\"customerName\":\"" + jsonEscape(PQgetvalue(r, i, 1)) + "\"";
        json += ",\"address\":\"" + jsonEscape(PQgetvalue(r, i, 2)) + "\"";
        json += ",\"phone\":\"" + jsonEscape(PQgetvalue(r, i, 3)) + "\"";
        json += ",\"items\":" + string(PQgetvalue(r, i, 4));
        json += ",\"total\":" + string(PQgetvalue(r, i, 5));
        json += ",\"status\":\"" + jsonEscape(PQgetvalue(r, i, 6)) + "\"";
        json += ",\"created_at\":\"" + jsonEscape(PQgetvalue(r, i, 7)) + "\"}";
    }

    json += "]";
    PQclear(r);
    return json;
}

// Return user information without password hashes.
string getUsers(PGconn* db) {
    PGresult* r = PQexec(
        db, "SELECT id,name,email,created_at::text FROM users ORDER BY id"
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"Failed to fetch users\"}";
    }

    string json = "[";

    for (int i = 0; i < PQntuples(r); ++i) {
        if (i) json += ",";

        json += "{\"id\":" + string(PQgetvalue(r, i, 0));
        json += ",\"name\":\"" + jsonEscape(PQgetvalue(r, i, 1)) + "\"";
        json += ",\"email\":\"" + jsonEscape(PQgetvalue(r, i, 2)) + "\"";
        json += ",\"created_at\":\"" + jsonEscape(PQgetvalue(r, i, 3)) + "\"}";
    }

    json += "]";
    PQclear(r);
    return json;
}
string deleteOrder(PGconn* db, const string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == string::npos)
        return "{\"success\":false,\"message\":\"Invalid order ID\"}";

    string id = path.substr(slash + 1);
    if (!validPositiveId(id))
        return "{\"success\":false,\"message\":\"Invalid order ID\"}";

    const char* values[] = { id.c_str() };
    PGresult* r = PQexecParams(
        db, "DELETE FROM orders WHERE id=$1",
        1, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"Order delete failed\"}";
    }

    bool deleted = string(PQcmdTuples(r)) != "0";
    PQclear(r);

    if (!deleted)
        return "{\"success\":false,\"message\":\"Order not found\"}";

    return "{\"success\":true,\"message\":\"Order deleted successfully\"}";
}

string updateOrderStatus(PGconn* db, const string& body) {
    string id = jsonNumber(body, "id");
    string status = jsonString(body, "status");

    if (!validPositiveId(id))
        return "{\"success\":false,\"message\":\"Invalid order ID\"}";

    if (status != "Pending" && status != "Confirmed" &&
        status != "Shipped" && status != "Delivered" &&
        status != "Cancelled") {
        return "{\"success\":false,\"message\":\"Invalid order status\"}";
    }

    const char* values[] = { id.c_str(), status.c_str() };
    PGresult* r = PQexecParams(
        db,
        "UPDATE orders SET status=$2 WHERE id=$1 RETURNING id",
        2, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"Order update failed\"}";
    }

    bool updated = PQntuples(r) > 0;
    PQclear(r);

    if (!updated)
        return "{\"success\":false,\"message\":\"Order not found\"}";

    return "{\"success\":true,\"message\":\"Order status updated\"}";
}

string deleteUser(PGconn* db, const string& path) {
    size_t slash = path.find_last_of('/');
    if (slash == string::npos)
        return "{\"success\":false,\"message\":\"Invalid user ID\"}";

    string id = path.substr(slash + 1);
    if (!validPositiveId(id))
        return "{\"success\":false,\"message\":\"Invalid user ID\"}";

    const char* values[] = { id.c_str() };
    PGresult* r = PQexecParams(
        db, "DELETE FROM users WHERE id=$1",
        1, nullptr, values, nullptr, nullptr, 0
    );

    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        PQclear(r);
        return "{\"success\":false,\"message\":\"User delete failed\"}";
    }

    bool deleted = string(PQcmdTuples(r)) != "0";
    PQclear(r);

    if (!deleted)
        return "{\"success\":false,\"message\":\"User not found\"}";

    return "{\"success\":true,\"message\":\"User deleted successfully\"}";
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
        size_t contentLength = 0;
        size_t headerEnd = string::npos;

        while (request.size() < 1024 * 1024) {
            ssize_t n = recv(client, buffer, sizeof(buffer), 0);
            if (n <= 0) break;

            request.append(buffer, static_cast<size_t>(n));
            headerEnd = request.find("\r\n\r\n");

            if (headerEnd != string::npos) {
                size_t cp = request.find("Content-Length:");

                if (cp != string::npos && cp < headerEnd) {
                    cp += 15;
                    while (cp < headerEnd && request[cp] == ' ') ++cp;
                    contentLength = strtoul(request.c_str() + cp, nullptr, 10);
                }

                if (request.size() >= headerEnd + 4 + contentLength)
                    break;
            }
        }

        size_t lineEnd = request.find("\r\n");
        string line = request.substr(0, lineEnd);

        string body = headerEnd == string::npos
            ? ""
            : request.substr(headerEnd + 4, contentLength);

        string result;
        string status = "200 OK";

        if (line.rfind("OPTIONS ", 0) == 0) {
            sendResponse(client, "{}");
        } else if (line.rfind("GET / ", 0) == 0) {
            sendResponse(client,
                "{\"success\":true,\"message\":\"YaluMart API is running\"}");
        } else if (line.rfind("GET /api/products ", 0) == 0) {
            sendResponse(client, getProducts(db));
        } else if (line.rfind("POST /api/products/add ", 0) == 0) {
            sendResponse(client, addProduct(db, body));
        } else if (line.rfind("POST /api/register ", 0) == 0) {
            sendResponse(client, registerUser(db, body));
        } else if (line.rfind("POST /api/login ", 0) == 0) {
            sendResponse(client, loginUser(db, body));
        } else if (line.rfind("POST /api/orders ", 0) == 0) {
            sendResponse(client, createOrder(db, body));
        } else if (line.rfind("GET /api/orders ", 0) == 0) {
            sendResponse(client, getOrders(db));
        } else if (line.rfind("GET /api/users ", 0) == 0) {
            sendResponse(client, getUsers(db));
            } else if (line.rfind("PUT /api/orders/status ", 0) == 0) {
            sendResponse(client, updateOrderStatus(db, body));
        } else if (line.rfind("DELETE /api/orders/", 0) == 0) {
            size_t start = line.find(' ') + 1;
            size_t end = line.find(' ', start);
            string path = line.substr(start, end - start);
            sendResponse(client, deleteOrder(db, path));
        } else if (line.rfind("DELETE /api/users/", 0) == 0) {
            size_t start = line.find(' ') + 1;
            size_t end = line.find(' ', start);
            string path = line.substr(start, end - start);
            sendResponse(client, deleteUser(db, path));

        } else if (line.rfind("DELETE /api/products/", 0) == 0) {
            size_t start = line.find(' ') + 1;
            size_t end = line.find(' ', start);
            string path = line.substr(start, end - start);
            sendResponse(client, deleteProduct(db, path));
        } else {
            sendResponse(client,
                "{\"success\":false,\"message\":\"Route not found\"}",
                "404 Not Found");
        }

        close(client);
    }

    close(server);
    PQfinish(db);
    return 0;
}
