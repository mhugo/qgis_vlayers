#define CORE_EXPORT
#define GUI_EXPORT
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>

#include <sqlite3.h>
#include "vtable.h"
#include <string.h>
#include <iostream>

struct VTable
{
    // minimal set of members (see sqlite3.h)
    const sqlite3_module *pModule;  /* The module for this virtual table */
    int nRef;                       /* NO LONGER USED */
    char *zErrMsg;                  /* Error message from sqlite3_mprintf() */

    // specific members
    QgsVectorLayer *layer_;

    // CREATE TABLE string
    QString creation_string_;

    VTable( const QString& ogr_key, const QString& path ) : zErrMsg(0)
    {
        layer_ = new QgsVectorLayer( path, "vtab_tmp", ogr_key );
        QgsVectorDataProvider *pr = layer_->dataProvider();
        const QgsFields& fields = pr->fields();
        QStringList sql_fields;
        for ( int i = 0; i < fields.count(); i++ ) {
            std::cout << fields.at(i).name().toUtf8().constData() << " " << fields.at(i).type() << std::endl;
            QString typeName = "TEXT";
            switch (fields.at(i).type()) {
            case QVariant::Int:
            case QVariant::UInt:
            case QVariant::Bool:
                typeName = "INT";
                break;
            case QVariant::Double:
                typeName = "REAL";
                break;
            case QVariant::String:
            default:
                typeName = "TEXT";
                break;
            }
            sql_fields << fields.at(i).name() + " " + typeName;
        }

        creation_string_ = "CREATE TABLE vtable (" + sql_fields.join(",") + ")";
    }

    ~VTable()
    {
        if (layer_) {
            delete layer_;
        }
    }

    QgsVectorDataProvider* provider() { return layer_->dataProvider(); }

    QString creation_string() const { return creation_string_; }
};

struct VTableCursor
{
    // minimal set of members (see sqlite3.h)
    VTable *vtab_;

    // specific members
    sqlite3_int64 current_row_;

    QgsFeature current_feature_;
    QgsFeatureIterator iterator_;
    bool eof_;

    VTableCursor( VTable *vtab ) : vtab_(vtab), current_row_(-1), eof_(true) {}

    void filter()
    {
        iterator_ = vtab_->provider()->getFeatures();
        current_row_ = -1;
        // got on the first record
        next();
    }

    void next()
    {
        eof_ = !iterator_.nextFeature( current_feature_ );
        if ( !eof_ ) {
            current_row_++;
        }
    }

    bool eof() const { return eof_; }

    sqlite3_int64 current_row() const { return current_row_; }

    QVariant current_attribute( int column ) const { return current_feature_.attribute(column); }
};

int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    std::cout << "vtable_create" << std::endl;
    if ( argc < 4 ) {
        std::string err( "Missing arguments: provider key and source" );
        *out_err = (char*)sqlite3_malloc(err.size()+1);
        strncpy( *out_err, err.c_str(), err.size() );
        return SQLITE_ERROR;
    }
    for ( int i = 0; i < argc; i++ ) {
        std::cout << i << "=" << argv[i] << std::endl;
    }

    VTable *new_vtab = new VTable( argv[3], argv[4] );
    int r = sqlite3_declare_vtab( sql, new_vtab->creation_string().toUtf8().constData() );
    if ( r ) {
        delete new_vtab;
        if ( out_err ) {
            size_t s = strlen(sqlite3_errmsg(sql));
            *out_err = (char*)sqlite3_malloc(s+1);
            strncpy( *out_err, sqlite3_errmsg(sql), s );
        }
        return r;
    }

    std::cout << "vtab created @" << new_vtab << std::endl;
    *out_vtab = (sqlite3_vtab*)new_vtab;
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
    VTableCursor *ncursor = new VTableCursor((VTable*)vtab);
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
    VTableCursor *c = reinterpret_cast<VTableCursor*>(cursor);
    c->filter();
    return SQLITE_OK;
}

int vtable_next( sqlite3_vtab_cursor *cursor )
{
    std::cout << "vtable_next" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    c->next();
    return SQLITE_OK;
}

int vtable_eof( sqlite3_vtab_cursor *cursor )
{
    std::cout << "vtable_eof" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    return c->eof();
}

int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid )
{
    std::cout << "vtable_rowid" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    *out_rowid = c->current_row();

    return SQLITE_OK;
}

int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context* ctxt, int idx )
{
    std::cout << "vtable_column" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    QVariant v = c->current_attribute( idx );
    if ( v.isNull() ) {
        sqlite3_result_null( ctxt );
    }
    else {
        switch ( v.type() ) {
        case QVariant::Int:
        case QVariant::UInt:
            sqlite3_result_int( ctxt, v.toInt() );
            break;
        case QVariant::Double:
            sqlite3_result_double( ctxt, v.toDouble() );
            break;
        default:
            sqlite3_result_text( ctxt, v.toString().toUtf8(), -1, SQLITE_TRANSIENT );
            break;
        }
    }
    return SQLITE_OK;
}

