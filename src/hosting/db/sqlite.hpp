#pragma once

#include <format>
#include <string>
#include <stdexcept>

#include <sqlite3.h>

namespace hosting::db
{
    struct sqlite
    {
        sqlite(const std::string &path)
        {
            if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
            {
                // create the database then
                createDatabase(path.c_str());
            }
        }

        ~sqlite() {
            close();
        }

        void ensure_table(std::string_view table, std::string_view fields)
        {
            std::string sql = std::format("CREATE TABLE IF NOT EXISTS {} ({})", table, fields);
            exec(sql);
        }

        void drop_table(std::string_view table)
        {
            std::string sql = std::format("DROP TABLE IF EXISTS {}", table);
            exec(sql);
        }

        void createDatabase(const char *dbName)
        {
            auto rc = sqlite3_open(dbName, &db_);
            if (rc)
            {
                throw std::runtime_error(std::format("Can't open database: {} - {}",
                    sqlite3_errmsg(db_), dbName));
            }
        }

        void close()
        {
            if (db_)
            {
                sqlite3_close(db_);
                db_ = nullptr;
            }
        }

        void exec(const std::string &sql) {
            auto rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
            if (rc != SQLITE_OK)
            {
                throw std::runtime_error(std::format("SQL error: {}", sqlite3_errmsg(db_)));
            }
        }

        void exec(const std::string &sql, auto callback) {
            auto rc = sqlite3_exec(db_, sql.c_str(), callback, nullptr, nullptr);
            if (rc != SQLITE_OK)
            {
                throw std::runtime_error(std::format("SQL error: {}", sqlite3_errmsg(db_)));
            }
        }
        
    private:
        sqlite3 *db_ {nullptr};
    };
}