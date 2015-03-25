#include <memory>
#include <string.h>
#include <iostream>
#include <stdint.h>

#include <QCoreApplication>

#include <qgsapplication.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsgeometry.h>
#include <qgsmaplayerregistry.h>
#include <qgsproviderregistry.h>

#include <sqlite3.h>
#include <spatialite.h>
#include <stdio.h>
#include "vlayer_module.h"

/**
 * Structure created in SQLITE module creation and passed to xCreate/xConnect
 */
struct ModuleContext
{
    // private pointer needed for spatialite_init_ex;
    // allows to know whether the database has been initialied (null or not)
    bool init;
    ModuleContext() : init(false) {}
};

/**
 * Create metadata tables if needed
 */
void initMetadata( sqlite3* db )
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
    uint32_t type = *(uint32_t*)p;
    uint32_t type2 = type & 0x3FFFFFFF;
    if ( type & 0x80000000 ) {
        // Z flag
        type2 += 1000;
    }
    else if ( type & 0x40000000 ) {
        // M flag
        type2 += 2000;
    }
    memcpy( p, &type2, 4 );
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

    // convert wkb types between spatialite and qgsgeometry
    uint32_t type = *(uint32_t*)(wkb+1);
    uint32_t type2 = type;
    if ( type > 1000 ) {
        // Z flag
        type2 = type2 - 1000 + 0x80000000;
    }
    *(uint32_t*)(wkb+1) = type2;

    std::unique_ptr<QgsGeometry> geom(new QgsGeometry());
    geom->fromWkb( wkb, wkb_size );
    return geom;
}

void delete_geometry_blob( void * p )
{
    delete[] (unsigned char*)p;
}

struct VTable
{
    // minimal set of members (see sqlite3.h)
    const sqlite3_module *pModule;  /* The module for this virtual table */
    int nRef;                       /* NO LONGER USED */
    char *zErrMsg;                  /* Error message from sqlite3_mprintf() */

    VTable( sqlite3* db, QgsVectorLayer* layer ) : sql_(db), provider_(layer->dataProvider()), pk_column_(-1), zErrMsg(0), owned_(false), name_(layer->name())
    {
        //std::cout << "VTable ref ctor sql@" << sql_ << " =" << this << " provider@" << provider_ << std::endl;
        init_();
    }

    VTable( sqlite3* db, const QString& provider, const QString& source, const QString& name ) : sql_(db), pk_column_(-1), zErrMsg(0), name_(name)
    {
        //std::cout << "source=" << source.toLocal8Bit().constData() << " name=" << name.toLocal8Bit().constData() << " provider=" << provider.toLocal8Bit().constData() << std::endl;
        provider_ = static_cast<QgsVectorDataProvider*>(QgsProviderRegistry::instance()->provider( provider, source ));
        if ( provider_ == 0 || !provider_->isValid() ) {
            throw std::runtime_error( "Invalid provider" );
        }
        owned_ = true;
        init_();
    }

    ~VTable()
    {
        //std::cout << "VTable dtor sql@" << sql_ << " @" << this << std::endl;
        if (owned_ && provider_ ) {
            delete provider_;
        }
    }

    QgsVectorDataProvider* provider()
    {
        return provider_;
    }

    QString name() const { return name_; }

    QString creation_string() const { return creation_str_; }

    long crs() const { return crs_; }

    sqlite3* sql() { return sql_; }

    int pk_column() const { return pk_column_; }

private:
    // connection
    sqlite3* sql_;

    // specific members
    // pointer to the underlying vector provider
    QgsVectorDataProvider* provider_;
    // whether the underlying provider is owned or not
    bool owned_;

    QString name_;

    // primary key column (default = -1: none)
    int pk_column_;

    // CREATE TABLE string
    QString creation_str_;

    long crs_;

    void init_()
    {
        // FIXME : connect to layer deletion signal
        const QgsFields& fields = provider_->fields();
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

        if ( provider_->geometryType() != QGis::WKBNoGeometry ) {
            sql_fields << "geometry " + geometry_type_string(provider_->geometryType());
        }

        if ( provider_->pkAttributeIndexes().size() == 1 ) {
            pk_column_ = provider_->pkAttributeIndexes()[0] + 1;
        }

        creation_str_ = "CREATE TABLE vtable (" + sql_fields.join(",") + ")";

        crs_ = provider_->crs().postgisSrid();
    }
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
        iterator_ = vtab_->provider()->getFeatures( request );
        current_row_ = -1;
        // get on the first record
        eof_ = false;
        next();
    }

    void next()
    {
        if ( !eof_ ) {
            //            std::cout << "nextFeature" << std::endl;
            eof_ = !iterator_.nextFeature( current_feature_ );
        }
        if ( !eof_ ) {
            current_row_++;
        }
    }

    bool eof() const { return eof_; }

    int n_columns() const { return vtab_->provider()->fields().count(); }

    sqlite3_int64 current_row() const { return current_row_; }

    QVariant current_attribute( int column ) const { return current_feature_.attribute(column); }

    QPair<unsigned char*, size_t> current_geometry() const
    {
        size_t blob_len;
        unsigned char* blob;
        qgsgeometry_to_spatialite_blob( *current_feature_.geometry(), vtab_->crs(), blob, blob_len );
        return qMakePair( blob, blob_len );
    }
};

void get_geometry_type( const QgsVectorDataProvider* provider, QString& geometry_type_str, int& geometry_dim, int& geometry_wkb_type, long& srid )
{
    srid = const_cast<QgsVectorDataProvider*>(provider)->crs().postgisSrid();
    switch ( provider->geometryType() ) {
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

    if ( argc < 4 ) {
        std::string err( "Missing arguments: layer_id | provider, source" );
        RETURN_CPPSTR_ERROR(err);
        return SQLITE_ERROR;
    }

    QScopedPointer<VTable> new_vtab;
    QString vname( argv[2] );
    sqlite3_stmt *table_creation_stmt = 0;
    int r;
    if ( argc == 4 ) {
        // CREATE VIRTUAL TABLE vtab USING QgsVLayer(layer_id)
        // vtab = argv[2]
        // layer_id = argv[3]
        //std::cout << argv[0] << " " << argv[1] << " " << argv[2] << " " << argv[3] << std::endl;
        QString layerid( argv[3] );
        if ( layerid.size() >= 1 && layerid[0] == '\'' ) {
            layerid = layerid.mid(1, layerid.size()-2);
        }
        QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( layerid );
        if ( l == 0 || l->type() != QgsMapLayer::VectorLayer ) {
            if ( out_err ) {
                std::string err("Cannot find layer ");
                err += argv[3];
                RETURN_CPPSTR_ERROR( err );
            }
            return SQLITE_ERROR;
        }
        new_vtab.reset(new VTable( sql, static_cast<QgsVectorLayer*>(l) ));

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
            new_vtab.reset(new VTable( sql, provider, source, argv[2] ));
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
            QByteArray sba( source.toLocal8Bit() );
            QByteArray pba( provider.toLocal8Bit() );
            sqlite3_bind_text( table_creation_stmt, 2, sba.constData(), sba.size(), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 3, pba.constData(), pba.size(), SQLITE_TRANSIENT );
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
        const QgsFields& fields = new_vtab->provider()->fields();
        QString columns_str;

        for ( int i = 0; i < fields.count(); i++ ) {
            columns_str += QString("INSERT INTO _columns (table_id,name,type) VALUES(%1,'%2','%3');")
                .arg(table_id)
                .arg(fields[i].name())
                .arg(QVariant::typeToName(fields[i].type()));
        }

        // add geometry field, if any
        long srid;
        QString geometry_str;
        int geometry_dim, geometry_wkb_type = 0;
        get_geometry_type( new_vtab->provider(), geometry_str, geometry_dim, geometry_wkb_type, srid );
        //std::cout << "geometry_wkb_type= " << geometry_wkb_type << std::endl;
        if ( geometry_wkb_type ) {
            columns_str += QString("INSERT INTO _columns VALUES(%1,'*geometry*','%2:%3:%4');").arg(table_id).arg(geometry_wkb_type).arg(geometry_dim).arg(srid);
            columns_str += QString( "INSERT OR REPLACE INTO virts_geometry_columns (virt_name, virt_geometry, geometry_type, coord_dimension, srid) "
                                    "VALUES ('%1', 'geometry', %2, %3, %4 );" )
                .arg(vname)
                .arg(geometry_wkb_type)
                .arg(geometry_dim)
                .arg(srid);
            // manually set column statistics (needed for QGIS spatialite provider)
            QgsRectangle extent = new_vtab->provider()->extent();
            columns_str += QString("INSERT OR REPLACE INTO virts_geometry_columns_statistics (virt_name, virt_geometry, last_verified, row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y) "
                                 "VALUES ('%1', 'geometry', datetime('now'), %2, %3, %4, %5, %6);")
                .arg(vname)
                .arg(new_vtab->provider()->featureCount())
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

    *out_vtab = (sqlite3_vtab*)new_vtab.take();
    return SQLITE_OK;
#undef RETURN_CSTR_ERROR
#undef RETURN_CPPSTR_ERROR
}

void db_init( sqlite3* db, ModuleContext* context )
{
    if ( context->init ) {
        // db already initialized
        return;
    }

    // create metadata tables
    initMetadata( db );
}

int vtable_create( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    //std::cout << "vtable_create sql@" << sql << " " << sqlite3_db_filename(sql, "main") << std::endl;
    try {
        db_init( sql, reinterpret_cast<ModuleContext*>(aux) );
    }
    catch (std::runtime_error& e) {
        if ( out_err ) {
            *out_err = (char*)sqlite3_malloc( strlen(e.what())+1 );
            strcpy( *out_err, e.what() );
        }
        return SQLITE_ERROR;
    }

    return vtable_create_connect( sql, aux, argc, argv, out_vtab, out_err, /* is_created */ true );
}

int vtable_connect( sqlite3* sql, void* aux, int argc, const char* const* argv, sqlite3_vtab **out_vtab, char** out_err)
{
    //std::cout << "vtable_connect sql@" << sql << std::endl;
    return vtable_create_connect( sql, aux, argc, argv, out_vtab, out_err, /* is_created */ false );
}

int vtable_destroy( sqlite3_vtab *vtab )
{
    if (vtab) {

        VTable* vtable = reinterpret_cast<VTable*>(vtab);
        //std::cout << "vtable_destroy sql@" << vtable->sql() << " " << sqlite3_db_filename( vtable->sql(), "main") << " vtab@" << vtab << " " << vtable->name().toUtf8().constData() << std::endl;

        QString query( "SELECT id FROM _tables WHERE name='" + vtable->name() + "'" );
        sqlite3_stmt* stmt;
        int r = sqlite3_prepare_v2( vtable->sql(), query.toLocal8Bit().constData(), -1, &stmt, NULL );
        if (r) {
            return r;
        }
        sqlite3_int64 table_id = 0;
        if ( sqlite3_step(stmt) == SQLITE_ROW ) {
            table_id = sqlite3_column_int64( stmt, 0 );
        }
        sqlite3_finalize(stmt);

        if ( table_id ) {
            QString q = QString("DELETE FROM _columns WHERE table_id=%1;DELETE FROM _tables WHERE id=%1;DELETE FROM virts_geometry_columns WHERE virt_name='%2';").arg(table_id).arg(vtable->name());
            char *errMsg;
            r = sqlite3_exec( vtable->sql(), q.toLocal8Bit().constData(), NULL, NULL, &errMsg );
            if ( r ) return r;
        }

        delete vtable;
    }
    return SQLITE_OK;
}

int vtable_disconnect( sqlite3_vtab *vtab )
{
    //std::cout << "vtable_disconnect sql@" << ((VTable*)vtab)->sql() << " vtab@" << vtab << std::endl;
    if (vtab) {
        delete reinterpret_cast<VTable*>(vtab);
    }
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
    //std::cout << "vtable_bestindex sql@" << vtab->sql() << " " << sqlite3_db_filename(vtab->sql(), "main")<< " vtab@" << vtab << std::endl;
    for ( int i = 0; i < index_info->nConstraint; i++ ) {
        //std::cout << index_info->aConstraint[i].iColumn << " " << (int)index_info->aConstraint[i].op << std::endl;
        if ( (index_info->aConstraint[i].usable) &&
             (vtab->pk_column() == index_info->aConstraint[i].iColumn) && 
             (index_info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ) ) {
            // request for primary key filter
            //std::cout << "PK filter" << std::endl;
            index_info->aConstraintUsage[i].argvIndex = 1;
            index_info->aConstraintUsage[i].omit = 1;
            index_info->idxNum = 1; // PK filter
            index_info->estimatedCost = 1.0; // ??
            //index_info->estimatedRows = 1;
            index_info->idxStr = NULL;
            index_info->needToFreeIdxStr = 0;
            return SQLITE_OK;
        }
        if ( (index_info->aConstraint[i].usable) &&
             (0 == index_info->aConstraint[i].iColumn) && 
             (index_info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ) ) {
            // request for rtree filtering
            //std::cout << "RTree filter" << std::endl;
            index_info->aConstraintUsage[i].argvIndex = 1;
            // do not test for equality, since it is used for filtering, not to return an actual value
            index_info->aConstraintUsage[i].omit = 1;
            index_info->idxNum = 2; // RTree filter
            index_info->estimatedCost = 1.0; // ??
            //index_info->estimatedRows = 1;
            index_info->idxStr = NULL;
            index_info->needToFreeIdxStr = 0;
            return SQLITE_OK;
        }
    }
    index_info->idxNum = 0;
    index_info->estimatedCost = 10.0;
    //index_info->estimatedRows = 10;
    index_info->idxStr = NULL;
    index_info->needToFreeIdxStr = 0;
    return SQLITE_OK;
}

int vtable_open( sqlite3_vtab *vtab, sqlite3_vtab_cursor **out_cursor )
{
    //std::cout << "vtable_open @" << vtab;
    VTableCursor *ncursor = new VTableCursor((VTable*)vtab);
    *out_cursor = (sqlite3_vtab_cursor*)ncursor;
    //    std::cout << " = " << *out_cursor << std::endl;
    return SQLITE_OK;
}

int vtable_close( sqlite3_vtab_cursor * cursor)
{
    //std::cout << "vtable_close vtab@" << cursor->pVtab << " cursor@" << cursor << std::endl;
    if ( cursor ) {
        delete reinterpret_cast<VTableCursor*>(cursor);
    }
    return SQLITE_OK;
}

int vtable_filter( sqlite3_vtab_cursor * cursor, int idxNum, const char *idxStr, int argc, sqlite3_value **argv )
{
    //std::cout << "vtable_filter vtab@" << cursor->pVtab << " cursor@" << cursor << " idxNum=" << idxNum << std::endl;
    QgsFeatureRequest request;
    if ( idxNum == 1 ) {
        // id filter
        request.setFilterFid( sqlite3_value_int(argv[0]) );
    }
    else if ( idxNum == 2 ) {
        // rtree filter
        //std::cout << "argc= " << argc << std::endl;
        const unsigned char* blob = (const unsigned char*)sqlite3_value_blob( argv[0] );
        int bytes = sqlite3_value_bytes( argv[0] );
        std::unique_ptr<QgsGeometry> geom( spatialite_blob_to_qgsgeometry( blob, bytes ) );
        QgsRectangle r( geom->boundingBox() );
        //std::cout << "rect filter: " << r.xMinimum() << " " << r.yMinimum() << " " << r.xMaximum() << " " << r.yMaximum() << std::endl;
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
    //std::cout << "vtable_column " << idx << " ";
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    if ( idx == 0 ) {
        // _search_frame_, return null
        sqlite3_result_null( ctxt );
        return SQLITE_OK;
    }
    if ( idx == c->n_columns() + 1) {
        QPair<unsigned char*, size_t> g = c->current_geometry();
        sqlite3_result_blob( ctxt, g.first, g.second, delete_geometry_blob );
        //std::cout << "geometry" << std::endl;
        return SQLITE_OK;
    }
    QVariant v = c->current_attribute( idx - 1 );
    if ( v.isNull() ) {
        //std::cout << "null";
        sqlite3_result_null( ctxt );
    }
    else {
        switch ( v.type() ) {
        case QVariant::Int:
        case QVariant::UInt:
            //std::cout << "int " << v.toInt();
            sqlite3_result_int( ctxt, v.toInt() );
            break;
        case QVariant::Double:
            //std::cout << "double " << v.toDouble();
            sqlite3_result_double( ctxt, v.toDouble() );
            break;
        default:
            //std::cout << "text " << v.toString().toUtf8().constData();
            sqlite3_result_text( ctxt, v.toString().toUtf8(), -1, SQLITE_TRANSIENT );
            break;
        }
    }
    //std::cout << std::endl;
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



sqlite3_module module;

static QCoreApplication* core_app = 0;

extern "C" {
void module_destroy( void * d )
{
    //    std::cout << "module destroy " << d << std::endl;
    delete reinterpret_cast<ModuleContext*>(d);

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
    //    std::cout << "qgsvlayer_module_init" << std::endl;
    int rc = SQLITE_OK;

    // check if qgis providers are loaded
    if ( QCoreApplication::instance() == 0 ) {
        core_app = new QCoreApplication( module_argc, module_argv );
        QgsApplication::init();
        QgsApplication::initQgis();
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
    module.xFindFunction = NULL;
    module.xSavepoint = NULL;
    module.xRelease = NULL;
    module.xRollbackTo = NULL;

    ModuleContext* context = new ModuleContext;
    //std::cout << "context = " << context << std::endl;
    sqlite3_create_module_v2( db, "QgsVLayer", &module, context, module_destroy );

    return rc;
}
};
