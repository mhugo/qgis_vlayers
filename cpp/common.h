#ifndef QGSVIRTUALLAYER_SQLITE_UTILS_H
#define QGSVIRTUALLAYER_SQLITE_UTILS_H

extern "C" {
#include <sqlite3.h>
}

#include <memory>

#define VIRTUAL_LAYER_VERSION 1

// custom deleter for QgsSqliteHandle
struct SqliteHandleDeleter
{
    void operator()(sqlite3* handle) {
        sqlite3_close(handle);
    }
};

typedef std::unique_ptr<sqlite3, SqliteHandleDeleter> QgsScopedSqlite;

struct Sqlite
{
    static QgsScopedSqlite open( const QString& path )
    {
        QgsScopedSqlite sqlite;
        int r;
        sqlite3* db;
        r = sqlite3_open( path.toLocal8Bit().constData(), &db );
        if (r) {
            throw std::runtime_error( sqlite3_errmsg(db) );
        }
        sqlite.reset(db);
        return sqlite;
    };
};

struct SqliteQuery
{
SqliteQuery( sqlite3* db, const QString& q ) : db_(db), nBind_(1)
    {
        QByteArray ba( q.toLocal8Bit() );
        int r = sqlite3_prepare_v2( db, ba.constData(), ba.size(), &stmt_, NULL );
        if (r) {
            throw std::runtime_error( sqlite3_errmsg(db) );
        }
    }

    int step() { return sqlite3_step(stmt_); }

    SqliteQuery& bind( const QString& str, int idx )
    {
        QByteArray ba( str.toLocal8Bit() );
        int r = sqlite3_bind_text( stmt_, idx, ba.constData(), ba.size(), SQLITE_TRANSIENT );
        if (r) {
            throw std::runtime_error( sqlite3_errmsg(db_) );
        }
        return *this;
    }
    SqliteQuery& bind( const QString& str )
    {
        return bind( str, nBind_++ );
    }

    static void exec( sqlite3* db, const QString& sql )
    {
        char *errMsg;
        int r = sqlite3_exec( db, sql.toLocal8Bit().constData(), NULL, NULL, &errMsg );
        if (r) {
            throw std::runtime_error( errMsg );
        }
    }


    sqlite3_stmt* stmt() { return stmt_; }

    ~SqliteQuery()
    {
        sqlite3_finalize( stmt_ );
    }
private:
    sqlite3* db_;
    sqlite3_stmt* stmt_;
    int nBind_;
};

#endif
