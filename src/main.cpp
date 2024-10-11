#include "sqlite3pp.h"
#include <fmt/core.h>

int main() {
    fmt::print("version: {}\n", sqlite3_version);

    Sqlite3pp sqlconn;

    sqlconn.open(":memory:");

    sqlconn.execute(R"(
        CREATE TABLE IF NOT EXISTS user (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            age INTEGER
        );
    )");

    sqlconn.execute(R"(INSERT INTO user (name, age) VALUES ('Alice', 30))");
    sqlconn.execute(R"(INSERT INTO user (name, age) VALUES ('Bob', 25))");
    sqlconn.execute(R"(INSERT INTO user (name, age) VALUES ('Charlie', 35))");

    std::string name = "Alice";
    int age          = 30;
    auto r5          = sqlconn.execute<std::tuple<int>>(
        "select count(*) as cnt from user where age = ? and name = ?", age, name);
    fmt::print("cnt: {}\n", std::get<0>(r5.at(0)));

    fmt::print("--------------------------------------------\n");
    using user_row_t = std::tuple<int, std::string, int>;
    auto r2          = sqlconn.execute<user_row_t>("select * from user");
    for (const auto& [id, name, age] : r2) {
        fmt::print("id: {}, name: {}, age: {}\n", id, name, age);
    }

    sqlconn.close();
    return 0;
}
