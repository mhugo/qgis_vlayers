#include <sqlite3.h>
#include "vtable.h"
#include <string.h>
#include <iostream>

struct VTableCursor
{
    VTableCursor( sqlite3_vtab *vtab ) : pVtab(vtab), current_row_(0), n_rows_(10) {}
    sqlite3_vtab *pVtab;
    sqlite3_int64 n_rows_;
    sqlite3_int64 current_row_;
};

int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    std::cout << "vtable_create" << std::endl;
    for ( int i = 0; i < argc; i++ ) {
        std::cout << i << "=" << argv[i] << std::endl;
    }
    int r = sqlite3_declare_vtab( sql, "CREATE TABLE vtable (a INT)" );
    if ( r ) {
        if ( out_err ) {
            size_t s = strlen(sqlite3_errmsg(sql));
            *out_err = (char*)sqlite3_malloc(s+1);
            strncpy( *out_err, sqlite3_errmsg(sql), s );
        }
        return r;
    }

    sqlite3_vtab *nvtab = new sqlite3_vtab;
    std::cout << "vtab created @" << nvtab << std::endl;
    // don't forget that
    nvtab->zErrMsg = NULL;
    *out_vtab = nvtab;
    return SQLITE_OK;
}

int vtable_rename( sqlite3_vtab *vtab, const char *new_name )
{
    std::cout << "vtable_rename" << std::endl;
    return SQLITE_OK;
}

int vtable_bestindex( sqlite3_vtab *vtab, sqlite3_index_info* index_info )
{
    std::cout << "vtable_bestindex @" << vtab << " index_info: " << index_info << std::endl;
    index_info->idxNum = 0;
    index_info->estimatedCost = 1.0;
    index_info->estimatedRows = 10;
    index_info->idxStr = NULL;
    index_info->needToFreeIdxStr = 0;
    return SQLITE_OK;
}
int vtable_destroy( sqlite3_vtab *vtab )
{
    std::cout << "vtable_destroy @" << vtab << std::endl;
    if (vtab) {
        delete vtab;
    }
    return SQLITE_OK;
}

int vtable_open( sqlite3_vtab *vtab, sqlite3_vtab_cursor **out_cursor )
{
    std::cout << "vtable_open @" << vtab << std::endl;
    VTableCursor *ncursor = new VTableCursor(vtab);
    *out_cursor = (sqlite3_vtab_cursor*)ncursor;
    return SQLITE_OK;
}

int vtable_close( sqlite3_vtab_cursor * cursor)
{
    std::cout << "vtable_close" << std::endl;
    if ( cursor ) {
        delete reinterpret_cast<VTableCursor*>(cursor);
    }
    return SQLITE_OK;
}

int vtable_filter( sqlite3_vtab_cursor * cursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv )
{
    std::cout << "vtable_filter" << std::endl;
    return SQLITE_OK;
}

int vtable_next( sqlite3_vtab_cursor *cursor )
{
    std::cout << "vtable_next" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    if ( c->current_row_ < c->n_rows_ ) {
        c->current_row_++;
    }
    return SQLITE_OK;
}

int vtable_eof( sqlite3_vtab_cursor *cursor )
{
    std::cout << "vtable_eof" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    return c->current_row_ == c->n_rows_;
}

int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context* ctxt, int idx )
{
    std::cout << "vtable_column" << std::endl;
    sqlite3_result_int(ctxt, 42);
    return SQLITE_OK;
}

int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid )
{
    std::cout << "vtable_rowid" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    *out_rowid = c->current_row_;

    return SQLITE_OK;
}
