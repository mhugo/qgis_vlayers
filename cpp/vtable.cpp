#include "vtable.h"
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
    std::cout << "create" << std::endl;
    sqlite3_vtab *nvtab = new sqlite3_vtab;
    *out_vtab = nvtab;

    sqlite3_declare_vtab( sql, "CREATE TABLE vtable (a INT)" );
    return SQLITE_OK;
}

int vtable_rename( sqlite3_vtab *vtab, const char *new_name )
{
    return SQLITE_OK;
}

int vtable_bestindex( sqlite3_vtab *vtab, sqlite3_index_info* index_info )
{
    index_info->idxNum = 0;
    index_info->estimatedCost = 1.0;
    index_info->estimatedRows = 1;
    return SQLITE_OK;
}
int vtable_destroy( sqlite3_vtab *vtab )
{
    if (vtab) {
        delete vtab;
    }
    return SQLITE_OK;
}

int vtable_open( sqlite3_vtab *vtab, sqlite3_vtab_cursor **out_cursor )
{
    VTableCursor *ncursor = new VTableCursor( vtab );
    *out_cursor = (sqlite3_vtab_cursor*)ncursor;
    return SQLITE_OK;
}

int vtable_close( sqlite3_vtab_cursor * cursor)
{
    if ( cursor ) {
        delete reinterpret_cast<VTableCursor*>(cursor);
    }
    return SQLITE_OK;
}

int vtable_filter( sqlite3_vtab_cursor * cursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv )
{
    return SQLITE_OK;
}

int vtable_next( sqlite3_vtab_cursor *cursor )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    if ( c->current_row_ < c->n_rows_ ) {
        c->current_row_++;
    }
    return SQLITE_OK;
}

int vtable_eof( sqlite3_vtab_cursor *cursor )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    return c->current_row_ == c->n_rows_;
}

int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context* ctxt, int idx )
{
    sqlite3_result_int(ctxt, 42);
    return SQLITE_OK;
}

int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    *out_rowid = c->current_row_;

    return SQLITE_OK;
}
