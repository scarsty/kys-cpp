#pragma once

#include <string>
#include <vector>

struct sqlite3_stmt;
struct sqlite3;
struct sqlite3_blob;

class SQLite3Stmt
{
    friend class SQLite3Wrapper;

public:
    SQLite3Stmt() = default;
    ~SQLite3Stmt();
    SQLite3Stmt(const SQLite3Stmt&) = delete;
    SQLite3Stmt& operator=(const SQLite3Stmt&) = delete;
    SQLite3Stmt(SQLite3Stmt&& other) noexcept;
    SQLite3Stmt& operator=(SQLite3Stmt&& other) noexcept;

    bool step();
    void reset();
    int getColumnCount() const;
    const char* getColumnName(int index) const;
    int getColumnInt(int index) const;
    const char* getColumnText(int index) const;
    double getColumnDouble(int index) const;
    std::vector<char> getColumnBlob(int index) const;
    bool isValid() const
    {
        return is_valid_;
    }
    bool bind(int index, int value);
    bool bind(int index, const char* text);
    bool bind(int index, void* p, size_t size);

private:
    sqlite3_stmt* stmt_{};
    bool is_valid_{ false };
};

class SQLite3Wrapper
{
public:
    SQLite3Wrapper() = default;
    SQLite3Wrapper(const std::string& db_name);
    ~SQLite3Wrapper();
    SQLite3Wrapper(const SQLite3Wrapper&) = delete;
    SQLite3Wrapper& operator=(const SQLite3Wrapper&) = delete;

    bool open(const std::string& db_name);
    bool execute(const std::string& sql);
    SQLite3Stmt query(const std::string& sql);
    sqlite3* getDB() const
    {
        return db_;
    }
    const char* getErrorMessage() const;
    bool BeginTransaction()
    {
        return execute("BEGIN TRANSACTION;");
    }
    bool CommitTransaction()
    {
        return execute("COMMIT;");
    }

    SQLite3Stmt prepare(const std::string& sql);
    void close();

private:
    sqlite3* db_ = nullptr;
    std::string db_name_;
    bool is_open_{ false };
};

class SQLite3Blob
{
public:
    SQLite3Blob(SQLite3Wrapper& db, const char* tbl_name, const char* col_name, int row);
    ~SQLite3Blob();
    SQLite3Blob(const SQLite3Blob&) = delete;
    SQLite3Blob& operator=(const SQLite3Blob&) = delete;

    void write(void* p, size_t size);
    void read(void* p, size_t size);
    int getSize() const;

    bool isValid() const
    {
        return blob_ != nullptr;
    }

private:
    sqlite3_blob* blob_{ nullptr };
};