#include <memory>

#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgsmaplayerregistry.h>
#include <qgsproviderregistry.h>

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

namespace {
struct _blob_header {
    unsigned char start;
    unsigned char endianness;
    uint32_t srid;
    double mbr_min_x;
    double mbr_min_y;
    double mbr_max_x;
    double mbr_max_y;
    unsigned char end;
};
}

QgsRectangle spatialite_blob_bbox( const unsigned char* blob, const size_t size )
{
    // name    size    value
    // start     1      00
    // endian    1      01
    // srid      4      int
    // mbr_min_x 8      double
    // mbr_min_y 8      double
    // mbr_max_x 8      double
    // mbr_max_y 8      double

    _blob_header h;
    memcpy( &h, blob, sizeof(h) );

    return QgsRectangle( h.mbr_min_x, h.mbr_min_y, h.mbr_max_x, h.mbr_max_y );
}

std::unique_ptr<QgsGeometry> spatialite_blob_to_qgsgeometry( const unsigned char* blob, const size_t size )
{
    size_t header_size = 39;
    size_t wkb_size = size - header_size;
    unsigned char* wkb = new unsigned char[wkb_size];
    wkb[0] = 0x01; // endianness
    memcpy( wkb + 1, blob + header_size, wkb_size );

    std::unique_ptr<QgsGeometry> geom(new QgsGeometry());
    geom->fromWkb( wkb, wkb_size );
    return geom;
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

    // connection
    sqlite3* sql;

    // specific members
    // pointer to the underlying vector layer
    QgsVectorLayer* layer_;
    // whether the underlying is owned or not
    bool owned_;

    // primary key column (default = -1: none)
    int pk_column_;

    // CREATE TABLE string
    QString creation_str_;

    VTable( QgsVectorLayer* layer ) : layer_(layer), pk_column_(-1), zErrMsg(0), owned_(false)
    {
        init_();
    }

    VTable( const QString& provider, const QString& source, const QString& name ) : pk_column_(-1), zErrMsg(0)
    {
        std::cout << "source=" << source.toLocal8Bit().constData() << " name=" << name.toLocal8Bit().constData() << " provider=" << provider.toLocal8Bit().constData() << std::endl;
        layer_ = new QgsVectorLayer( source, name, provider );
        if ( layer_ == 0 || !layer_->isValid() ) {
            throw std::runtime_error( "Invalid layer" );
        }
        owned_ = true;
        init_();
    }

    ~VTable()
    {
        if (owned_ && layer_ ) {
            delete layer_;
        }
    }

    void init_()
    {
        // FIXME : connect to layer deletion signal
        QgsVectorDataProvider *pr = layer_->dataProvider();
        const QgsFields& fields = pr->fields();
        QStringList sql_fields;

        // add a hidden field for rtree filtering
        sql_fields << "_search_frame_ HIDDEN BLOB";

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
            pk_column_ = layer_->dataProvider()->pkAttributeIndexes()[0] + 1;
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

void get_geometry_type( const QgsVectorLayer* vlayer, QString& geometry_type_str, int& geometry_dim, int& geometry_wkb_type, long& srid )
{
    srid = vlayer->crs().postgisSrid();
    switch ( vlayer->dataProvider()->geometryType() ) {
    case QGis::WKBNoGeometry:
        geometry_type_str = "";
        geometry_dim = 0;
        geometry_wkb_type = 0;
        break;
    case QGis::WKBPoint:
        geometry_type_str = "POINT";
        geometry_dim = 2;
        geometry_wkb_type = 1;
        break;
    case QGis::WKBPoint25D:
        geometry_type_str = "POINT";
        geometry_dim = 3;
        geometry_wkb_type = 1001;
        break;
    case QGis::WKBMultiPoint:
        geometry_type_str = "MULTIPOINT";
        geometry_dim = 2;
        geometry_wkb_type = 4;
        break;
    case QGis::WKBMultiPoint25D:
        geometry_type_str = "MULTIPOINT";
        geometry_dim = 3;
        geometry_wkb_type = 1004;
        break;
    case QGis::WKBLineString:
        geometry_type_str = "LINESTRING";
        geometry_dim = 2;
        geometry_wkb_type = 2;
        break;
    case QGis::WKBLineString25D:
        geometry_type_str = "LINESTRING";
        geometry_dim = 3;
        geometry_wkb_type = 1002;
        break;
    case QGis::WKBMultiLineString:
        geometry_type_str = "MULTILINESTRING";
        geometry_dim = 2;
        geometry_wkb_type = 5;
        break;
    case QGis::WKBMultiLineString25D:
        geometry_type_str = "MULTILINESTRING";
        geometry_dim = 3;
        geometry_wkb_type = 1005;
        break;
    case QGis::WKBPolygon:
        geometry_type_str = "POLYGON";
        geometry_dim = 2;
        geometry_wkb_type = 3;
        break;
    case QGis::WKBPolygon25D:
        geometry_type_str = "POLYGON";
        geometry_dim = 3;
        geometry_wkb_type = 1003;
        break;
    case QGis::WKBMultiPolygon:
        geometry_type_str = "MULTIPOLYGON";
        geometry_dim = 2;
        geometry_wkb_type = 6;
        break;
    case QGis::WKBMultiPolygon25D:
        geometry_type_str = "MULTIPOLYGON";
        geometry_dim = 3;
        geometry_wkb_type = 1006;
        break;
    }
}

int vtable_create_connect( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err, bool is_created )
{
#define RETURN_CSTR_ERROR(err) if (out_err) {size_t s = strlen(err); *out_err=(char*)sqlite3_malloc(s+1); strncpy(*out_err, err, s);}
#define RETURN_CPPSTR_ERROR(err) if (out_err) {*out_err=(char*)sqlite3_malloc(err.size()+1); strncpy(*out_err, err.c_str(), err.size());}

    std::cout << "vtable_create" << std::endl;
    if ( argc < 4 ) {
        std::string err( "Missing arguments: layer_id | provider, source" );
        RETURN_CPPSTR_ERROR(err);
        return SQLITE_ERROR;
    }

    std::cout << "#providers: " << QgsProviderRegistry::instance()->providerList().size() << std::endl;

    QScopedPointer<VTable> new_vtab;
    QString vname( argv[2] );
    sqlite3_stmt *table_creation_stmt = 0;
    int r;
    if ( argc == 4 ) {
        // CREATE VIRTUAL TABLE vtab USING QgsVLayer(layer_id)
        // vtab = argv[2]
        // layer_id = argv[3]
        std::cout << argv[0] << " " << argv[1] << " " << argv[2] << " " << argv[3] << std::endl;
        QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( argv[3] );
        std::cout << l << std::endl;
        if ( l == 0 || l->type() != QgsMapLayer::VectorLayer ) {
            if ( out_err ) {
                std::string err("Cannot find layer ");
                err += argv[3];
                RETURN_CPPSTR_ERROR( err );
            }
            return SQLITE_ERROR;
        }
        new_vtab.reset(new VTable( static_cast<QgsVectorLayer*>(l) ));

        if ( is_created ) {
            r = sqlite3_prepare_v2( sql, "INSERT INTO _tables (name, layer_id) VALUES(?,?);", -1, &table_creation_stmt, NULL );
            if (r) {
                RETURN_CSTR_ERROR( sqlite3_errmsg(sql) );
                return r;
            }
            sqlite3_bind_text( table_creation_stmt, 1, argv[2], strlen(argv[2]), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 2, argv[3], strlen(argv[3]), SQLITE_TRANSIENT );
        }
    }
    else if ( argc == 5 ) {
        // CREATE VIRTUAL TABLE vtab USING QgsVLayer(provider,source)
        // vtab = argv[2]
        // provider = argv[3]
        // source = argv[4]
        QString provider = argv[3];
        QString source = argv[4];
        if ( provider.size() >= 1 && provider[0] == '\'' ) {
            provider = provider.mid(1, provider.size()-2);
        }
        if ( source.size() >= 1 && source[0] == '\'' ) {
            source = source.mid(1, source.size()-2);
        }
        try {
            new_vtab.reset(new VTable( provider, source, argv[2] ));
        }
        catch (std::runtime_error& e) {
            std::string err(e.what());
            RETURN_CPPSTR_ERROR( err );
            return SQLITE_ERROR;
        }

        if ( is_created ) {
            r = sqlite3_prepare_v2( sql, "INSERT INTO _tables (name, source, provider) VALUES(?,?,?);", -1, &table_creation_stmt, NULL );
            if (r) {
                RETURN_CSTR_ERROR( sqlite3_errmsg(sql) );
                return r;
            }
            sqlite3_bind_text( table_creation_stmt, 1, argv[2], strlen(argv[2]), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 2, argv[4], strlen(argv[4]), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 3, argv[3], strlen(argv[3]), SQLITE_TRANSIENT );
        }
    }

    r = sqlite3_declare_vtab( sql, new_vtab->creation_string().toUtf8().constData() );
    if (r) {
        RETURN_CSTR_ERROR( sqlite3_errmsg(sql) );
        return r;
    }

    if ( is_created ) {
        // insert table in metadata
        r = sqlite3_step( table_creation_stmt );
        if (r != SQLITE_DONE ) {
            RETURN_CSTR_ERROR( sqlite3_errmsg(sql) );
            return r;
        }
        sqlite3_finalize( table_creation_stmt );

        sqlite3_int64 table_id = sqlite3_last_insert_rowid( sql );
        const QgsFields& fields = new_vtab->layer()->dataProvider()->fields();
        QString columns_str;

        for ( int i = 0; i < fields.count(); i++ ) {
            columns_str += QString("INSERT INTO _columns (table_id,name,type) VALUES(%1,'%2','%3');")
                .arg(table_id)
                .arg(fields[i].name())
                .arg(fields[i].typeName());
        }

        // add geometry field, if any
        long srid;
        QString geometry_str;
        int geometry_dim, geometry_wkb_type = 0;
        get_geometry_type( new_vtab->layer(), geometry_str, geometry_dim, geometry_wkb_type, srid );
        std::cout << "geometry_wkb_type= " << geometry_wkb_type << std::endl;
        if ( geometry_wkb_type ) {
            columns_str += QString("INSERT INTO _columns VALUES(%1,'*geometry*','%2:%3:%4');").arg(table_id).arg(geometry_wkb_type).arg(geometry_dim).arg(srid);
            columns_str += QString( "INSERT OR REPLACE INTO virts_geometry_columns (virt_name, virt_geometry, geometry_type, coord_dimension, srid) "
                                    "VALUES ('%1', 'geometry', %2, %3, %4 );" )
                .arg(vname)
                .arg(geometry_wkb_type)
                .arg(geometry_dim)
                .arg(srid);
            // manually set column statistics (needed for QGIS spatialite provider)
            QgsRectangle extent = new_vtab->layer()->extent();
            columns_str += QString("INSERT OR REPLACE INTO virts_geometry_columns_statistics (virt_name, virt_geometry, last_verified, row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y) "
                                 "VALUES ('%1', 'geometry', datetime('now'), %2, %3, %4, %5, %6);")
                .arg(vname)
                .arg(new_vtab->layer()->featureCount())
                .arg(extent.xMinimum())
                .arg(extent.yMinimum())
                .arg(extent.xMaximum())
                .arg(extent.yMaximum());
        }

        char *errMsg;
        r = sqlite3_exec( sql, columns_str.toLocal8Bit().constData(), NULL, NULL, &errMsg );
        if (r) {
            RETURN_CSTR_ERROR( errMsg );
            return r;
        }
    }

    new_vtab->sql = sql;
    *out_vtab = (sqlite3_vtab*)new_vtab.take();
    return SQLITE_OK;
#undef RETURN_CSTR_ERROR
#undef RETURN_CPPSTR_ERROR
}

int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    return vtable_create_connect( sql, aux, argc, argv, out_vtab, out_err, /* is_created */ true );
}

int vtable_connect( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    return vtable_create_connect( sql, aux, argc, argv, out_vtab, out_err, /* is_created */ false );
}

int vtable_destroy( sqlite3_vtab *vtab )
{
    //    std::cout << "vtable_destroy @" << vtab << std::endl;
    if (vtab) {

        VTable* vtable = reinterpret_cast<VTable*>(vtab);

        QString query( "SELECT id FROM _tables WHERE name='" + vtable->layer()->name() + "'" );
        char *errMsg;
        sqlite3_stmt* stmt;
        int r = sqlite3_prepare_v2( vtable->sql, query.toLocal8Bit().constData(), -1, &stmt, NULL );
        if (r) {
            return r;
        }
        sqlite3_int64 table_id = 0;
        if ( sqlite3_step(stmt) ) {
            table_id = sqlite3_column_int64( stmt, 0 );
        }
        sqlite3_finalize(stmt);

        if ( table_id ) {
            QString q = QString("DELETE FROM _columns WHERE table_id=%1;DELETE FROM _tables WHERE id=%1;DELETE FROM virts_geometry_columns WHERE virt_name='%2';").arg(table_id).arg(vtable->layer()->name());
            char *errMsg;
            r = sqlite3_exec( vtable->sql, q.toLocal8Bit().constData(), NULL, NULL, &errMsg );
            if ( r ) return r;
        }

        delete vtable;
    }
    return SQLITE_OK;
}

int vtable_disconnect( sqlite3_vtab *vtab )
{
    //    std::cout << "vtable_disconnect @" << vtab << std::endl;
    if (vtab) {
        delete reinterpret_cast<VTable*>(vtab);
    }
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
        if ( (index_info->aConstraint[i].usable) &&
             (0 == index_info->aConstraint[i].iColumn) && 
             (index_info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ) ) {
            // request for rtree filtering
            std::cout << "RTree filter" << std::endl;
            index_info->aConstraintUsage[i].argvIndex = 1;
            index_info->idxNum = 2; // RTree filter
            index_info->estimatedCost = 1.0; // ??
            index_info->estimatedRows = 1;
            index_info->idxStr = NULL;
            index_info->needToFreeIdxStr = 0;
            return SQLITE_OK;
        }
    }
    index_info->idxNum = 0;
    index_info->estimatedCost = 10.0;
    index_info->estimatedRows = 10;
    index_info->idxStr = NULL;
    index_info->needToFreeIdxStr = 0;
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
    else if ( idxNum == 2 ) {
        // rtree filter
        std::cout << "argc= " << argc << std::endl;
        const unsigned char* blob = (const unsigned char*)sqlite3_value_blob( argv[0] );
        int bytes = sqlite3_value_bytes( argv[0] );
        std::unique_ptr<QgsGeometry> geom( spatialite_blob_to_qgsgeometry( blob, bytes ) );
        QgsRectangle r( geom->boundingBox() );
        std::cout << "rect filter: " << r.xMinimum() << " " << r.yMinimum() << " " << r.xMaximum() << " " << r.yMaximum() << std::endl;
        request.setFilterRect( r );
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
    if ( idx == 0 ) {
        // _search_frame_
        sqlite3_result_null( ctxt );
        return SQLITE_OK;
    }
    if ( idx == c->n_columns() + 1) {
        QPair<unsigned char*, size_t> g = c->current_geometry();
        sqlite3_result_blob( ctxt, g.first, g.second, delete_geometry_blob );
        //        std::cout << "geometry" << std::endl;
        return SQLITE_OK;
    }
    QVariant v = c->current_attribute( idx - 1 );
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
