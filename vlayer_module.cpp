/***************************************************************************
             vlayer_module.cpp : SQLite module for QGIS virtual layers
begin                : Jan, 2015
copyright            : (C) 2015 Hugo Mercier, Oslandia
email                : hugo dot mercier at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
        r = sqlite3_exec( db, "CREATE TABLE _tables (id INTEGER PRIMARY KEY, name TEXT, provider TEXT, source TEXT, encoding TEXT, layer_id TEXT);", NULL, NULL, &errMsg );        
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

void copy_spatialite_single_wkb_to_qgsgeometry( uint32_t type, const unsigned char* iwkb, unsigned char* owkb, uint32_t& osize )
{
  int dims = type / 1000;
  int n_dims = 2 + (dims&1) + (dims&2);
  switch (type%1000)
  {
  case 1:
    // point
    memcpy( owkb, iwkb, n_dims*8 );
    iwkb += n_dims*8;
    iwkb += n_dims*8;
    osize = n_dims*8;
    break;
  case 2: {
    // linestring
    uint32_t n_points = *(uint32_t*)iwkb;
    memcpy( owkb, iwkb, 4 );
    iwkb+=4; owkb+=4;
    for ( uint32_t i = 0; i < n_points; i++ ) {
      memcpy( owkb, iwkb, n_dims*8 );
      iwkb += n_dims*8;
      owkb += n_dims*8;
    }
    osize += n_dims*8*n_points + 4;
    break;
  }
  case 3: {
    // polygon
    uint32_t n_rings = *(uint32_t*)iwkb;
    memcpy( owkb, iwkb, 4 );
    iwkb+=4; owkb+=4;
    osize = 4;
    for ( uint32_t i = 0; i < n_rings; i++ ) {
      uint32_t n_points = *(uint32_t*)iwkb;
      memcpy( owkb, iwkb, 4 );
      iwkb+=4; owkb+=4;
      osize += 4;
      for ( uint32_t j = 0; j < n_points; j++ ) {
        memcpy( owkb, iwkb, n_dims*8 );
        iwkb += n_dims*8;
        owkb += n_dims*8;
        osize += n_dims*8;
      }
    }
    break;
  }
  }
}

void copy_spatialite_collection_wkb_to_qgsgeometry( const unsigned char* iwkb, unsigned char* owkb, uint32_t& osize )
{
  memcpy( owkb, iwkb, 5 );
  owkb[0] = 0x01; // endianness
  size_t copy_size = 0;
  uint32_t type = *(uint32_t*)(iwkb+1);

  // convert type
  uint32_t type2 = (type > 1000) ? (type%1000 + 0x80000000) : type;
  *(uint32_t*)(owkb+1) = type2;

  if ( (type % 1000) >= 4 ) {
    // multi type
    uint32_t n_elements = *(uint32_t*)(iwkb+5);
    memcpy( owkb+5, iwkb+5, 4 );
    uint32_t p = 0;
    for ( uint32_t i = 0; i < n_elements; i++ ) {
      uint32_t rsize = 0;
      copy_spatialite_collection_wkb_to_qgsgeometry( iwkb+9+p, owkb+9+p, rsize );
      p += rsize;
    }
    osize = p+9;
  }
  else {
    osize = 0;
    copy_spatialite_single_wkb_to_qgsgeometry( type, iwkb+5, owkb+5, osize );
    osize += 5;
  }
}

std::unique_ptr<QgsGeometry> spatialite_blob_to_qgsgeometry( const unsigned char* blob, const size_t size )
{
    size_t header_size = 39;
    size_t wkb_size = size - header_size;
    unsigned char* wkb = new unsigned char[wkb_size];

    uint32_t osize = 0;
    copy_spatialite_collection_wkb_to_qgsgeometry( blob + header_size - 1, wkb, osize );

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
        init_();
    }

    VTable( sqlite3* db, const QString& provider, const QString& source, const QString& name, const QString& encoding )
        : sql_(db), pk_column_(-1), zErrMsg(0), name_(name), encoding_(encoding)
    {
        provider_ = static_cast<QgsVectorDataProvider*>(QgsProviderRegistry::instance()->provider( provider, source ));
        if ( provider_ == 0 || !provider_->isValid() ) {
            throw std::runtime_error( "Invalid provider" );
        }
        if ( provider_->capabilities() & QgsVectorDataProvider::SelectEncoding ) {
            provider_->setEncoding( encoding_ );
        }
        owned_ = true;
        init_();
    }

    ~VTable()
    {
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

    QString encoding_;

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
    QgsFeature current_feature_;
    QgsFeatureIterator iterator_;
    bool eof_;

    VTableCursor( VTable *vtab ) : vtab_(vtab), eof_(true) {}

    void filter( QgsFeatureRequest request )
    {
        iterator_ = vtab_->provider()->getFeatures( request );
        // get on the first record
        eof_ = false;
        next();
    }

    void next()
    {
        if ( !eof_ ) {
            eof_ = !iterator_.nextFeature( current_feature_ );
        }
    }

    bool eof() const { return eof_; }

    int n_columns() const { return vtab_->provider()->fields().count(); }

    sqlite3_int64 current_id() const { return current_feature_.id(); }

    QVariant current_attribute( int column ) const { return current_feature_.attribute(column); }

    QPair<unsigned char*, size_t> current_geometry() const
    {
        size_t blob_len;
        unsigned char* blob;
        // make it work for pre 2.10 and 2.10 qgis version
        QgsGeometry* g = const_cast<QgsFeature&>(current_feature_).geometry();
        qgsgeometry_to_spatialite_blob( *g, vtab_->crs(), blob, blob_len );
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
    else if ( argc == 5 || argc == 6 ) {
        // CREATE VIRTUAL TABLE vtab USING QgsVLayer(provider,source[,encoding])
        // vtab = argv[2]
        // provider = argv[3]
        // source = argv[4]
        // encoding = argv[5]
        QString provider = argv[3];
        QString source = argv[4];
        QString encoding = "UTF-8";
        if ( argc == 6 ) {
            encoding = argv[5];
        }
        if ( provider.size() >= 1 && provider[0] == '\'' ) {
          // trim and undouble single quotes
          provider = provider.mid(1, provider.size()-2).replace( "''", "'" );
        }
        if ( source.size() >= 1 && source[0] == '\'' ) {
          // trim and undouble single quotes
          source = source.mid(1, source.size()-2).replace( "''", "'" );
        }
        try {
            new_vtab.reset(new VTable( sql, provider, source, argv[2], encoding ));
        }
        catch (std::runtime_error& e) {
            std::string err(e.what());
            RETURN_CPPSTR_ERROR( err );
            return SQLITE_ERROR;
        }

        if ( is_created ) {
            r = sqlite3_prepare_v2( sql, "INSERT INTO _tables (name, source, provider, encoding) VALUES(?,?,?,?);", -1, &table_creation_stmt, NULL );
            if (r) {
                RETURN_CSTR_ERROR( sqlite3_errmsg(sql) );
                return r;
            }
            sqlite3_bind_text( table_creation_stmt, 1, argv[2], strlen(argv[2]), SQLITE_TRANSIENT );
            QByteArray sba( source.toLocal8Bit() );
            QByteArray pba( provider.toLocal8Bit() );
            QByteArray eba( encoding.toLocal8Bit() );
            sqlite3_bind_text( table_creation_stmt, 2, sba.constData(), sba.size(), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 3, pba.constData(), pba.size(), SQLITE_TRANSIENT );
            sqlite3_bind_text( table_creation_stmt, 4, eba.constData(), eba.size(), SQLITE_TRANSIENT );
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
        if ( geometry_wkb_type ) {
            columns_str += QString("INSERT INTO _columns VALUES(%1,'*geometry*','%2:%3:%4');").arg(table_id).arg(geometry_wkb_type).arg(geometry_dim).arg(srid);
            columns_str += QString( "INSERT OR REPLACE INTO virts_geometry_columns (virt_name, virt_geometry, geometry_type, coord_dimension, srid) "
                                    "VALUES ('%1', 'geometry', %2, %3, %4 );" )
                .arg(vname.toLower())
                .arg(geometry_wkb_type)
                .arg(geometry_dim)
                .arg(srid);
            // manually set column statistics (needed for QGIS spatialite provider)
            QgsRectangle extent = new_vtab->provider()->extent();
            columns_str += QString("INSERT OR REPLACE INTO virts_geometry_columns_statistics (virt_name, virt_geometry, last_verified, row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y) "
                                 "VALUES ('%1', 'geometry', datetime('now'), %2, %3, %4, %5, %6);")
                .arg(vname.toLower())
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
    return vtable_create_connect( sql, aux, argc, argv, out_vtab, out_err, /* is_created */ false );
}

int vtable_destroy( sqlite3_vtab *vtab )
{
    if (vtab) {

        VTable* vtable = reinterpret_cast<VTable*>(vtab);

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
            QString q = QString("DELETE FROM _columns WHERE table_id=%1;DELETE FROM _tables WHERE id=%1;DELETE FROM virts_geometry_columns WHERE virt_name='%2';").arg(table_id).arg(vtable->name().toLower());
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
    if (vtab) {
        delete reinterpret_cast<VTable*>(vtab);
    }
	return SQLITE_OK;
}

int vtable_rename( sqlite3_vtab *vtab, const char *new_name )
{
    return SQLITE_OK;
}

int vtable_bestindex( sqlite3_vtab *pvtab, sqlite3_index_info* index_info )
{
    VTable *vtab = (VTable*)pvtab;
    for ( int i = 0; i < index_info->nConstraint; i++ ) {
        if ( (index_info->aConstraint[i].usable) &&
             (vtab->pk_column() == index_info->aConstraint[i].iColumn) && 
             (index_info->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ) ) {
            // request for primary key filter
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
    VTableCursor *ncursor = new VTableCursor((VTable*)vtab);
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
    QgsFeatureRequest request;
    if ( idxNum == 1 ) {
        // id filter
        request.setFilterFid( sqlite3_value_int(argv[0]) );
    }
    else if ( idxNum == 2 ) {
        // rtree filter
        const unsigned char* blob = (const unsigned char*)sqlite3_value_blob( argv[0] );
        int bytes = sqlite3_value_bytes( argv[0] );
        std::unique_ptr<QgsGeometry> geom( spatialite_blob_to_qgsgeometry( blob, bytes ) );
        QgsRectangle r( geom->boundingBox() );
        request.setFilterRect( r );
    }
    VTableCursor *c = reinterpret_cast<VTableCursor*>(cursor);
    c->filter( request );
    return SQLITE_OK;
}

int vtable_next( sqlite3_vtab_cursor *cursor )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    c->next();
    return SQLITE_OK;
}

int vtable_eof( sqlite3_vtab_cursor *cursor )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    return c->eof();
}

int vtable_rowid( sqlite3_vtab_cursor *cursor, sqlite3_int64 *out_rowid )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    *out_rowid = c->current_id();

    return SQLITE_OK;
}

int vtable_column( sqlite3_vtab_cursor *cursor, sqlite3_context* ctxt, int idx )
{
    VTableCursor* c = reinterpret_cast<VTableCursor*>(cursor);
    if ( idx == 0 ) {
        // _search_frame_, return null
        sqlite3_result_null( ctxt );
        return SQLITE_OK;
    }
    if ( idx == c->n_columns() + 1) {
        QPair<unsigned char*, size_t> g = c->current_geometry();
        sqlite3_result_blob( ctxt, g.first, g.second, delete_geometry_blob );
        return SQLITE_OK;
    }
    QVariant v = c->current_attribute( idx - 1 );
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
            {
                sqlite3_result_text( ctxt, v.toString().toUtf8(), -1, SQLITE_TRANSIENT );
            }
            break;
        }
    }
    return SQLITE_OK;
}

int vtable_findfunction( sqlite3_vtab *pVtab,
                         int nArg,
                         const char *zName,
                         void (**pxFunc)(sqlite3_context*,int,sqlite3_value**),
                         void **ppArg )
{
    return SQLITE_OK;
}



sqlite3_module module;

static QCoreApplication* core_app = 0;

extern "C" {
void module_destroy( void * d )
{
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
    sqlite3_create_module_v2( db, "QgsVLayer", &module, context, module_destroy );

    return rc;
}
};
