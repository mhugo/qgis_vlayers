#include <sqlite3ext.h> /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

#include <stdio.h>
#include "vtable.h"

#ifdef _WIN32
__declspec(dllexport)
#endif

void module_destroy( void * d )
{
    printf("module destroy\n");
}

sqlite3_module module;

int sqlite3_extension_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
)
{
    int rc = SQLITE_OK;
    SQLITE_EXTENSION_INIT2(pApi);

    module.xCreate = vtable_create;
    module.xConnect = vtable_create;
    module.xBestIndex = vtable_bestindex;
    module.xDisconnect = vtable_destroy;
    module.xDestroy = vtable_destroy;
    module.xOpen = vtable_open;
    module.xClose = vtable_close;
    module.xFilter = vtable_filter;
    module.xNext = vtable_next;
    module.xEof = vtable_eof;
    module.xColumn = vtable_column;
    module.xRowid = vtable_rowid;
    module.xRename = vtable_rename;

    module.xUpdate = NULL;
    module.xBegin = NULL;
    module.xSync = NULL;
    module.xCommit = NULL;
    module.xRollback = NULL;
    module.xFindFunction = NULL;
    module.xSavepoint = NULL;
    module.xRelease = NULL;
    module.xRollbackTo = NULL;

    sqlite3_create_module_v2( db, "QgsVLayer", &module, NULL, module_destroy );

    return rc;
}
