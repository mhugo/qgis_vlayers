#ifdef __cplusplus
extern "C" {
#endif
int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err);
int vtable_connect( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err);
int vtable_rename( sqlite3_vtab *vtab, const char *new_name );
int vtable_bestindex( sqlite3_vtab *vtab, sqlite3_index_info* );
int vtable_disconnect( sqlite3_vtab *vtab );
int vtable_destroy( sqlite3_vtab *vtab );

int vtable_open( sqlite3_vtab *vtab, sqlite3_vtab_cursor **out_cursor );
int vtable_close( sqlite3_vtab_cursor * );
int vtable_filter( sqlite3_vtab_cursor * cursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv );

int vtable_next( sqlite3_vtab_cursor *cursor );
int vtable_eof( sqlite3_vtab_cursor *cursor );
int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context*, int );
int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid );

int vtable_findfunction( sqlite3_vtab *pVtab,
                         int nArg,
                         const char *zName,
                         void (**pxFunc)(sqlite3_context*,int,sqlite3_value**),
                         void **ppArg );

#ifdef __cplusplus
}

#include <memory>
#include <qgsgeometry.h>

std::unique_ptr<QgsGeometry> spatialite_blob_to_qgsgeometry( const unsigned char* blob, const size_t size );

void initMetadata( sqlite3* db );

#endif

