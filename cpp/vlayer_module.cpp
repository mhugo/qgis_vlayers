#include <QCoreApplication>

#include <qgsproviderregistry.h>
#include <qgsapplication.h>

#include <sqlite3.h>
#include <stdio.h>
#include "vtable.h"

sqlite3_module module;

static QCoreApplication* core_app = 0;

static void initMetadata( sqlite3* db )
{
    bool create_meta = false;
    bool create_tables = false;
    bool create_columns = false;

    sqlite3_stmt *stmt;
    int r;
    r = sqlite3_prepare_v2( db, "SELECT name FROM sqlite_master WHERE name='_meta'", -1, &stmt, NULL );
    if (r) {
        throw std::runtime_error( sqlite3_errmsg( db ) );
    }
    create_meta = sqlite3_step( stmt ) != SQLITE_ROW;
    sqlite3_finalize( stmt );

    r = sqlite3_prepare_v2( db, "SELECT name FROM sqlite_master WHERE name='_tables'", -1, &stmt, NULL );
    if (r) {
        throw std::runtime_error( sqlite3_errmsg( db ) );
    }
    create_tables = sqlite3_step( stmt ) != SQLITE_ROW;
    sqlite3_finalize( stmt );

    r = sqlite3_prepare_v2( db, "SELECT name FROM sqlite_master WHERE name='_columns'", -1, &stmt, NULL );
    if (r) {
        throw std::runtime_error( sqlite3_errmsg( db ) );
    }
    create_columns = sqlite3_step( stmt ) != SQLITE_ROW;
    sqlite3_finalize( stmt );

    char *errMsg;
    if (create_meta) {
        r = sqlite3_exec( db, "CREATE TABLE _meta (version INT); INSERT INTO _meta VALUES(1);", NULL, NULL, &errMsg );        
        if (r) {
            throw std::runtime_error( errMsg );
        }
    }
    if (create_tables) {
        r = sqlite3_exec( db, "CREATE TABLE _tables (id INTEGER PRIMARY KEY, name TEXT, provider TEXT, source TEXT, layer_id TEXT);", NULL, NULL, &errMsg );        
        if (r) {
            throw std::runtime_error( errMsg );
        }
    }
    if (create_tables) {
        r = sqlite3_exec( db, "CREATE TABLE _columns (table_id INT, name TEXT, type TEXT);", NULL, NULL, &errMsg );        
        if (r) {
            throw std::runtime_error( errMsg );
        }
    }
}

extern "C" {
void module_destroy( void * d )
{
    printf("module destroy\n");
    if (core_app) {
        delete core_app;
    }
}

    static int module_argc = 1;
    static char module_name[] = "qgsvlayer_module";
    static char* module_argv[] = { module_name };

int qgsvlayer_module_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  void * unused /*const sqlite3_api_routines *pApi*/
)
{
    printf("qgsvlayer_module_init\n");
    int rc = SQLITE_OK;

    // check if qgis providers are loaded
    if ( QCoreApplication::instance() == 0 ) {
        core_app = new QCoreApplication( module_argc, module_argv );
        QgsApplication::init();
        QgsApplication::initQgis();
        printf("%s\n", QgsApplication::showSettings().toLocal8Bit().constData() );
    }

    // initialize spatialite tables
    int r = sqlite3_exec( db, "SELECT InitSpatialMetadata(1);", NULL, NULL, pzErrMsg );
    if (r) {
        return SQLITE_ERROR;
    }

    // create metadata tables
    try {
        initMetadata( db );
    }
    catch (std::exception& e) {
        *pzErrMsg = (char*)sqlite3_malloc( strlen(e.what())+1 );
        strcpy( *pzErrMsg, e.what() );
        return SQLITE_ERROR;
    }


    module.xCreate = vtable_create;
    module.xConnect = vtable_connect;
    module.xBestIndex = vtable_bestindex;
    module.xDisconnect = vtable_disconnect;
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
};
