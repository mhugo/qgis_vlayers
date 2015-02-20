#ifndef QGSVIRTUALLAYER_SQLITE_UTILS_H
#define QGSVIRTUALLAYER_SQLITE_UTILS_H

extern "C" {
#include <sqlite3.h>
}

#define VIRTUAL_LAYER_VERSION 1

struct SqliteQuery
{
SqliteQuery( sqlite3* db, const QString& q ) : db_(db), nBind_(1)
    {
        int r = sqlite3_prepare_v2( db, q.toLocal8Bit().constData(), q.size(), &stmt_, NULL );
        if (r) {
            throw std::runtime_error( sqlite3_errmsg(db) );
        }
    }

    int step() { return sqlite3_step(stmt_); }

    SqliteQuery& bind( const QString& str, int idx )
    {
        int r = sqlite3_bind_text( stmt_, idx, str.toLocal8Bit().constData(), str.size(), SQLITE_TRANSIENT );
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
