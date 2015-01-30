#include <sqlite3.h>
#include <stdio.h>
#include "vtable.h"

sqlite3_module module;

void module_destroy( void * d )
{
    printf("module destroy\n");
}

int qgsvlayer_module_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  void * unused /*const sqlite3_api_routines *pApi*/
)
{
    printf("qgsvalyer_module_init\n");
    int rc = SQLITE_OK;

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
    module.xFindFunction = vtable_findfunction;
    module.xSavepoint = NULL;
    module.xRelease = NULL;
    module.xRollbackTo = NULL;

    sqlite3_create_module_v2( db, "QgsVLayer", &module, NULL, module_destroy );

    return rc;
}
