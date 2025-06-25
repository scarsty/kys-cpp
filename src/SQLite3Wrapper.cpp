#include "SQLite3Wrapper.h"
#include "sqlite3.h"

#include "GameUtil.h"

SQLite3Stmt::~SQLite3Stmt()
{
    if (stmt_)
    {
        sqlite3_finalize(stmt_);
    }
}

bool SQLite3Stmt::step()
{
    if (!stmt_)
    {
        return false;
    }
    int result = sqlite3_step(stmt_);
    if (result == SQLITE_ROW)
    {
        return true;    // 有数据
    }
    else if (result == SQLITE_DONE)
    {
        return false;    // 没有更多数据
    }
    else
    {
        // 处理错误
        return false;
    }
}

void SQLite3Stmt::reset()
{
    if (stmt_)
    {
        sqlite3_reset(stmt_);
    }
}

int SQLite3Stmt::getColumnCount() const
{
    return sqlite3_column_count(stmt_);
}

const char* SQLite3Stmt::getColumnName(int index) const
{
    return sqlite3_column_name(stmt_, index);
}

int SQLite3Stmt::getColumnInt(int index) const
{
    return sqlite3_column_int(stmt_, index);
}

const char* SQLite3Stmt::getColumnText(int index) const
{
    return reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
}

double SQLite3Stmt::getColumnDouble(int index) const
{
    return sqlite3_column_double(stmt_, index);
}

std::vector<char> SQLite3Stmt::getColumnBlob(int index) const
{
    const void* blob = sqlite3_column_blob(stmt_, index);
    auto size = sqlite3_column_bytes(stmt_, index);
    std::vector<char> data(size);
    memccpy(data.data(), blob, 0, size);
    return data;
}

bool SQLite3Stmt::bind(int index, int value)
{
    return sqlite3_bind_int(stmt_, index, value) == SQLITE_OK;
}

bool SQLite3Stmt::bind(int index, const char* text)
{
    return sqlite3_bind_text(stmt_, index, text, -1, SQLITE_STATIC) == SQLITE_OK;
}

bool SQLite3Stmt::bind(int index, void* p, size_t size)
{
    return sqlite3_bind_blob(stmt_, index, p, size, SQLITE_STATIC);
}

SQLite3Stmt::SQLite3Stmt(SQLite3Stmt&& other) noexcept :
    stmt_(other.stmt_)
{
    other.stmt_ = nullptr;
    is_valid_ = other.is_valid_;
}

SQLite3Stmt& SQLite3Stmt::operator=(SQLite3Stmt&& other) noexcept
{
    if (this != &other)
    {
        if (stmt_)
        {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    is_valid_ = other.is_valid_;
    return *this;
}

SQLite3Wrapper::SQLite3Wrapper(const std::string& db_name) :
    db_(nullptr), db_name_(db_name), is_open_(false)
{
    open(db_name);
}

SQLite3Wrapper::~SQLite3Wrapper()
{
    if (db_)
    {
        sqlite3_close(db_);
    }
}

bool SQLite3Wrapper::open(const std::string& db_name)
{
    db_name_ = db_name;
    const char* db_path = db_name.c_str();
    if (db_name.empty())
    {
        db_path = nullptr;    //在内存中创建数据库
    }
    int rc = sqlite3_open(db_path, &db_);
    if (rc != SQLITE_OK)
    {
        db_ = nullptr;
        is_open_ = false;
        return false;
    }
    is_open_ = true;
    return true;
}

bool SQLite3Wrapper::execute(const std::string& sql)
{
    if (!is_open_)
    {
        return false;
    }
    char* err_msg = nullptr;
    int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (result != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

SQLite3Stmt SQLite3Wrapper::query(const std::string& sql)
{
    SQLite3Stmt stmt;
    if (!is_open_)
    {
        return SQLite3Stmt();
    }
    char* err_msg = nullptr;
    int result = sqlite3_prepare(db_, sql.c_str(), sql.size(), &stmt.stmt_, nullptr);
    if (result != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return SQLite3Stmt();
    }
    stmt.is_valid_ = true;
    return stmt;
}

const char* SQLite3Wrapper::getErrorMessage() const
{
    return sqlite3_errmsg(db_);
}

SQLite3Stmt SQLite3Wrapper::prepare(const std::string& sql)
{
    SQLite3Stmt stmt;
    if (!is_open_)
    {
        return SQLite3Stmt();
    }
    char* err_msg = nullptr;
    int result = sqlite3_prepare_v2(db_, sql.c_str(), sql.size(), &stmt.stmt_, nullptr);
    if (result != SQLITE_OK)
    {
        if (err_msg)
        {
            sqlite3_free(err_msg);
        }
        return SQLite3Stmt();
    }
    stmt.is_valid_ = true;
    return stmt;
}

SQLite3Blob::SQLite3Blob(SQLite3Wrapper& db, const char* tbl_name, const char* col_name, int row)
{
    sqlite3_blob_open(db.getDB(), "main", tbl_name, col_name, row, 1, &blob_);
}

SQLite3Blob::~SQLite3Blob()
{
    if (blob_)
    {
        sqlite3_blob_close(blob_);
    }
}

void SQLite3Blob::write(void* p, size_t size)
{
    if (blob_)
    {
        sqlite3_blob_write(blob_, p, size, 0);
    }
}

void SQLite3Blob::read(void* p, size_t size)
{
    if (blob_)
    {
        sqlite3_blob_read(blob_, p, size, 0);
    }
}

int SQLite3Blob::getSize() const
{
    return sqlite3_blob_bytes(blob_);
}
