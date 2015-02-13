#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgsmaplayerregistry.h>

#include <sqlite3.h>
#include "vtable.h"
#include <string.h>
#include <iostream>
#include <stdio.h>

QString geometry_type_string( QGis::WkbType type )
{
    switch ( type ) {
    case QGis::WKBPoint:
    case QGis::WKBPoint25D:
        return "POINT";
    case QGis::WKBMultiPoint:
    case QGis::WKBMultiPoint25D:
        return "MULTIPOINT";
    case QGis::WKBLineString:
    case QGis::WKBLineString25D:
        return "LINESTRING";
    case QGis::WKBMultiLineString:
    case QGis::WKBMultiLineString25D:
        return "MULTILINESTRING";
    case QGis::WKBPolygon:
    case QGis::WKBPolygon25D:
        return "POLYGON";
    case QGis::WKBMultiPolygon:
    case QGis::WKBMultiPolygon25D:
        return "MULTIPOLYGON";
    }
    return "";
}

void qgsgeometry_to_spatialite_blob( const QgsGeometry& geom, int32_t srid, unsigned char *&blob, size_t& size )
{
    // BLOB header
    // name    size    value
    // start     1      00
    // endian    1      01
    // srid      4      int
    // mbr_min_x 8      double
    // mbr_min_y 8      double
    // mbr_max_x 8      double
    // mbr_max_y 8      double
    // mbr_end   1      7C
    //          4*8+4+3=4
    size_t header_len = 39;

    // wkb of the geometry is
    // name         size    value
    // endianness     1      01
    // type           4      int

    // blob geometry = header + wkb[1:] + 'end'

    size_t wkb_size = geom.wkbSize();
    size = header_len + wkb_size;
    blob = new unsigned char[size];

    QgsRectangle bbox = const_cast<QgsGeometry&>(geom).boundingBox();
    double mbr_min_x, mbr_min_y, mbr_max_x, mbr_max_y;
    mbr_min_x = bbox.xMinimum();
    mbr_max_x = bbox.xMaximum();
    mbr_min_y = bbox.yMinimum();
    mbr_max_y = bbox.yMaximum();

    unsigned char* p = blob;
    *p = 0x00; p++; // start
    *p = 0x01; p++; // endianness
    memcpy( p, &srid, sizeof(srid) ); p+= sizeof(srid);
    memcpy( p, &mbr_min_x, sizeof(double) ); p+= sizeof(double);
    memcpy( p, &mbr_min_y, sizeof(double) ); p+= sizeof(double);
    memcpy( p, &mbr_max_x, sizeof(double) ); p+= sizeof(double);
    memcpy( p, &mbr_max_y, sizeof(double) ); p+= sizeof(double);
    *p = 0x7C; p++; // mbr_end

    // copy wkb
    memcpy( p, geom.asWkb() + 1, wkb_size - 1 );
    p += wkb_size - 1;
    // end marker
    *p = 0xFE;
}

void delete_geometry_blob( void * p )
{
    delete (unsigned char*)p;
}

struct VTable
{
    // minimal set of members (see sqlite3.h)
    const sqlite3_module *pModule;  /* The module for this virtual table */
    int nRef;                       /* NO LONGER USED */
    char *zErrMsg;                  /* Error message from sqlite3_mprintf() */

    // specific members
    // pointer to the underlying vector layer, not owned
    QgsVectorLayer* layer_;

    // primary key column (default = -1: none)
    int pk_column_;

    // CREATE TABLE string
    QString creation_str_;

    VTable( QgsVectorLayer* layer ) : layer_(layer), pk_column_(-1), zErrMsg(0)
    {
        // FIXME : connect to layer deletion signal
        QgsVectorDataProvider *pr = layer_->dataProvider();
        const QgsFields& fields = pr->fields();
        QStringList sql_fields;
        for ( int i = 0; i < fields.count(); i++ ) {
            //            std::cout << fields.at(i).name().toUtf8().constData() << " " << fields.at(i).type() << std::endl;
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

        if ( layer_->hasGeometryType() ) {
            sql_fields << "geometry " + geometry_type_string(layer_->dataProvider()->geometryType());
        }

        if ( layer_->dataProvider()->pkAttributeIndexes().size() == 1 ) {
            pk_column_ = layer_->dataProvider()->pkAttributeIndexes()[0];
        }

        creation_str_ = "CREATE TABLE vtable (" + sql_fields.join(",") + ")";
    }

    QgsVectorLayer* layer() { return layer_; }

    QString creation_string() const { return creation_str_; }
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

    void filter( QgsFeatureRequest request )
    {
        std::cout << "filter" << std::endl;
        iterator_ = vtab_->layer()->dataProvider()->getFeatures( request );
        current_row_ = -1;
        // get on the first record
        eof_ = false;
        next();
    }

    void next()
    {
        if ( !eof_ ) {
            std::cout << "nextFeature" << std::endl;
            eof_ = !iterator_.nextFeature( current_feature_ );
        }
        if ( !eof_ ) {
            current_row_++;
        }
    }

    bool eof() const { return eof_; }

    int n_columns() const { return vtab_->layer()->dataProvider()->fields().count(); }

    sqlite3_int64 current_row() const { return current_row_; }

    QVariant current_attribute( int column ) const { return current_feature_.attribute(column); }

    QPair<unsigned char*, size_t> current_geometry() const
    {
        size_t blob_len;
        unsigned char* blob;
        qgsgeometry_to_spatialite_blob( *current_feature_.geometry(), vtab_->layer()->crs().postgisSrid(), blob, blob_len );
        return qMakePair( blob, blob_len );
    }
};

int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    //    std::cout << "vtable_create" << std::endl;
    if ( argc < 4 ) {
        std::string err( "Missing arguments: layer id" );
        *out_err = (char*)sqlite3_malloc(err.size()+1);
        strncpy( *out_err, err.c_str(), err.size() );
        return SQLITE_ERROR;
    }
    /*    for ( int i = 0; i < argc; i++ ) {
        std::cout << i << "=" << argv[i] << std::endl;
        }*/

    QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( argv[3] );
    if ( l == 0 || l->type() != QgsMapLayer::VectorLayer ) {
        std::string err("Cannot find layer");
        *out_err = (char*)sqlite3_malloc(err.size()+1);
        strncpy( *out_err, err.c_str(), err.size() );
        return SQLITE_ERROR;
    }
    VTable *new_vtab = new VTable( static_cast<QgsVectorLayer*>(l) );
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

    //    std::cout << "vtab created @" << new_vtab << std::endl;
    *out_vtab = (sqlite3_vtab*)new_vtab;
    return SQLITE_OK;
}

int vtable_rename( sqlite3_vtab *vtab, const char *new_name )
{
    //    std::cout << "vtable_rename" << std::endl;
    return SQLITE_OK;
}

int vtable_bestindex( sqlite3_vtab *pvtab, sqlite3_index_info* index_info )
{
    VTable *vtab = (VTable*)pvtab;
    std::cout << "vtable_bestindex" << std::endl;
    for ( int i = 0; i < index_info->nConstraint; i++ ) {
        if ( (index_info->aConstraint[i].usable) &&
             (vtab->pk_column_ == index_info->aConstraint[i].iColumn) && 
             (index_info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ) ) {
            // request for primary key filter
            std::cout << "PK filter" << std::endl;
            index_info->aConstraintUsage[i].argvIndex = 1;
            index_info->idxNum = 1; // PK filter
            index_info->estimatedCost = 1.0; // ??
            index_info->estimatedRows = 1;
            index_info->idxStr = NULL;
            index_info->needToFreeIdxStr = 0;
            return SQLITE_OK;
        }
    }
    index_info->idxNum = 0;
    index_info->estimatedCost = 1.0;
    index_info->estimatedRows = 10;
    index_info->idxStr = NULL;
    index_info->needToFreeIdxStr = 0;
    return SQLITE_OK;
}
int vtable_destroy( sqlite3_vtab *vtab )
{
    //    std::cout << "vtable_destroy @" << vtab << std::endl;
    if (vtab) {
        delete reinterpret_cast<VTable*>(vtab);
    }
    return SQLITE_OK;
}

int vtable_open( sqlite3_vtab *vtab, sqlite3_vtab_cursor **out_cursor )
{
    //    std::cout << "vtable_open @" << vtab << std::endl;
    VTableCursor *ncursor = new VTableCursor((VTable*)vtab);
    *out_cursor = (sqlite3_vtab_cursor*)ncursor;
    return SQLITE_OK;
}

int vtable_close( sqlite3_vtab_cursor * cursor)
{
    //    std::cout << "vtable_close" << std::endl;
    if ( cursor ) {
        delete reinterpret_cast<VTableCursor*>(cursor);
    }
    return SQLITE_OK;
}

int vtable_filter( sqlite3_vtab_cursor * cursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv )
{
    std::cout << "vtable_filter idxNum=" << idxNum << std::endl;
    QgsFeatureRequest request;
    if ( idxNum == 1 ) {
        // id filter
        request.setFilterFid( sqlite3_value_int(argv[0]) );
    }
    VTableCursor *c = reinterpret_cast<VTableCursor*>(cursor);
    c->filter( request );
    return SQLITE_OK;
}

int vtable_next( sqlite3_vtab_cursor *cursor )
{
    //    std::cout << "vtable_next" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    c->next();
    return SQLITE_OK;
}

int vtable_eof( sqlite3_vtab_cursor *cursor )
{
    //    std::cout << "vtable_eof" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    return c->eof();
}

int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid )
{
    //    std::cout << "vtable_rowid" << std::endl;
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    *out_rowid = c->current_row();

    return SQLITE_OK;
}

int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context* ctxt, int idx )
{
    //    std::cout << "vtable_column " << idx << " ";
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    if ( idx == c->n_columns() ) {
        QPair<unsigned char*, size_t> g = c->current_geometry();
        sqlite3_result_blob( ctxt, g.first, g.second, delete_geometry_blob );
        //        std::cout << "geometry" << std::endl;
        return SQLITE_OK;
    }
    QVariant v = c->current_attribute( idx );
    if ( v.isNull() ) {
        //        std::cout << "null";
        sqlite3_result_null( ctxt );
    }
    else {
        switch ( v.type() ) {
        case QVariant::Int:
        case QVariant::UInt:
            //            std::cout << "int " << v.toInt();
            sqlite3_result_int( ctxt, v.toInt() );
            break;
        case QVariant::Double:
            //            std::cout << "double " << v.toDouble();
            sqlite3_result_double( ctxt, v.toDouble() );
            break;
        default:
            //            std::cout << "text " << v.toString().toUtf8().constData();
            sqlite3_result_text( ctxt, v.toString().toUtf8(), -1, SQLITE_TRANSIENT );
            break;
        }
    }
    //    std::cout << std::endl;
    return SQLITE_OK;
}

int vtable_findfunction( sqlite3_vtab *pVtab,
                         int nArg,
                         const char *zName,
                         void (**pxFunc)(sqlite3_context*,int,sqlite3_value**),
                         void **ppArg )
{
    //    std::cout << "find function: " << zName << std::endl;
    return SQLITE_OK;
}
