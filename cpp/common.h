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

namespace Sqlite
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

    struct Query
    {
        Query( sqlite3* db, const QString& q ) : db_(db), nBind_(1)
        {
            QByteArray ba( q.toLocal8Bit() );
            int r = sqlite3_prepare_v2( db, ba.constData(), ba.size(), &stmt_, NULL );
            if (r) {
                throw std::runtime_error( sqlite3_errmsg(db) );
            }
        }

        ~Query()
        {
            sqlite3_finalize( stmt_ );
        }

        int step() { return sqlite3_step(stmt_); }

        Query& bind( const QString& str, int idx )
        {
            QByteArray ba( str.toLocal8Bit() );
            int r = sqlite3_bind_text( stmt_, idx, ba.constData(), ba.size(), SQLITE_TRANSIENT );
            if (r) {
                throw std::runtime_error( sqlite3_errmsg(db_) );
            }
            return *this;
        }
        Query& bind( const QString& str )
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

        void reset()
        {
            int r = sqlite3_reset( stmt_ );
            if (r) {
                throw std::runtime_error( sqlite3_errmsg(db_) );
            }
        }

        int column_count() const
        {
            return sqlite3_column_count(stmt_);
        }

        QString column_name(int i) const
        {
            return QString( sqlite3_column_name(stmt_, i) );
        }

        int column_int( int i ) const
        {
            return sqlite3_column_int( stmt_, i );
        }

        double column_double( int i ) const
        {
            return sqlite3_column_double( stmt_, i );
        }

        QString column_text( int i ) const
        {
            int size = sqlite3_column_bytes( stmt_, i );
            const char* str = (const char*)sqlite3_column_text( stmt_, i );
            return QString::fromUtf8( str, size );
        }

        QByteArray column_blob( int i ) const
        {
            int size = sqlite3_column_bytes( stmt_, i );
            const char* data = (const char*)sqlite3_column_blob( stmt_, i );
            // data is not copied. QByteArray is just here a augmented pointer
            return QByteArray::fromRawData( data, size );
        }

        sqlite3_stmt* stmt() { return stmt_; }

    private:
        sqlite3* db_;
        sqlite3_stmt* stmt_;
        int nBind_;
    };
}

#endif
