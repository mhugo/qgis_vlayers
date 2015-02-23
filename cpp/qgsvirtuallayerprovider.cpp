/***************************************************************************
           qgsvirtuallayerprovider.cpp Virtual layer data provider
begin                : Jan, 2014
copyright            : (C) 2014 Hugo Mercier, Oslandia
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

extern "C" {
#include <sqlite3.h>
#include <spatialite.h>
}

#include <QUrl>

#include "qgsvirtuallayerprovider.h"
#include <qgsvectorlayer.h>
#include <qgsmaplayerregistry.h>
#include <qgsdatasourceuri.h>
#include "common.h"

const QString VIRTUAL_LAYER_KEY = "virtual";
const QString VIRTUAL_LAYER_DESCRIPTION = "Virtual layer data provider";

// declaration of the spatialite module
extern "C" {
    int qgsvlayer_module_init();
}

#define PROVIDER_ERROR( msg ) do { mError = QgsError( msg, VIRTUAL_LAYER_KEY ); QgsDebugMsg( msg ); } while(0)

int geometry_type_to_wkb_type( const QString& wkb_str )
{
    QString w = wkb_str.toLower();
    if ( w == "point" ) {
        return 1;
    }
    else if ( w == "linestring" ) {
        return 2;
    }
    else if ( w == "polygon" ) {
        return 3;
    }
    else if ( w == "multipoint" ) {
        return 4;
    }
    else if ( w == "multilinestring" ) {
        return 5;
    }
    else if ( w == "multipolygon" ) {
        return 6;
    }
    return 0;
}

void QgsVirtualLayerProvider::getSqliteFields( sqlite3* db, const QString& table, QgsFields& fields, GeometryFields& gFields )
{
    SqliteQuery q( db, QString("PRAGMA table_info('%1')").arg(table) );
    int n = sqlite3_column_count( q.stmt() );
    while ( sqlite3_step( q.stmt() ) == SQLITE_ROW ) {
        QString field_name((const char*)sqlite3_column_text( q.stmt(), 1 ));
        QString field_type((const char*)sqlite3_column_text( q.stmt(), 2 ));

        if ( field_type == "INT" ) {
            fields.append( QgsField( field_name, QVariant::Int ) );
        }
        else if ( field_type == "REAL" ) {
            fields.append( QgsField( field_name, QVariant::Double ) );
        }
        else if ( field_type == "TEXT" ) {
            fields.append( QgsField( field_name, QVariant::String ) );
        }
        else if (( field_type == "POINT" ) || (field_type == "MULTIPOINT") ||
                 (field_type == "LINESTRING") || (field_type == "MULTILINESTRING") ||
                 (field_type == "POLYGON") || (field_type == "MULTIPOLYGON")) {
            GeometryField g;
            g.name = field_name;
            g.wkbType = geometry_type_to_wkb_type( field_type );
            gFields.append( g );
        }
        else {
            // force to TEXT
            fields.append( QgsField( field_name, QVariant::String ) );
        }
    }
}

QgsVirtualLayerProvider::QgsVirtualLayerProvider( QString const &uri )
    : QgsVectorDataProvider( uri ),
      mValid( true )
{
    QUrl url = QUrl::fromEncoded( uri.toUtf8() );
    if ( !url.isValid() ) {
        mValid = false;
        PROVIDER_ERROR("Malformed URL");
        return;
    }

    // xxxxx = open a virtual layer
    // xxxxx?key=value&key=value = create a virtual layer
    // ?key=value = create a temporary virtual layer

    mPath = url.path();

    // geometry field to consider (if more than one or if it cannot be detected)
    GeometryField geometryField;

    bool noGeometry = false;

    // overwritten fields
    QgsFields forcedFields;

    try {
    int layer_idx = 0;
    QList<QPair<QByteArray, QByteArray> > items = url.encodedQueryItems();
    for ( int i = 0; i < items.size(); i++ ) {
        QString key = QString(items.at(i).first);
        QString value = QString(items.at(i).second);
        if ( key == "layer_ref" ) {
            layer_idx++;
            // layer id, with optional layer_name
            int pos = value.indexOf(':');
            QString layer_id, vlayer_name;
            if ( pos == -1 ) {
                layer_id = value;
                vlayer_name = QString("vtab%1").arg(layer_idx);
            } else {
                layer_id = value.left(pos);
                vlayer_name = value.mid(pos+1);
            }
            QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( layer_id );
            if ( l == 0 ) {
                mValid = false;
                PROVIDER_ERROR( QString("Cannot find layer %1").arg(layer_id) );
                return;
            }
            if ( l->type() != QgsMapLayer::VectorLayer ) {
                mValid = false;
                PROVIDER_ERROR( QString("Layer %1 is not a vector layer").arg(layer_id) );
                return;
            }
            // add the layer to the list
            mLayers << SourceLayer(static_cast<QgsVectorLayer*>(l), vlayer_name);
        }
        if ( key == "layer" ) {
            layer_idx++;
            // syntax: layer=provider:url_encoded_source_URI(:name)?
            int pos = value.indexOf(':');
            if ( pos != -1 ) {
                QString providerKey, source, vlayer_name;

                providerKey = value.left(pos);
                int pos2 = value.indexOf( ':', pos + 1);
                if (pos2 != -1) {
                    source = QUrl::fromPercentEncoding(value.mid(pos+1,pos2-pos-1).toLocal8Bit());
                    vlayer_name = value.mid(pos2+1);
                }
                else {
                    source = QUrl::fromPercentEncoding(value.mid(pos+1).toLocal8Bit());
                    vlayer_name = QString("vtab%1").arg(layer_idx);
                }

                mLayers << SourceLayer(providerKey, source, vlayer_name);
            }
        }
        else if ( key == "geometry" ) {
            // geometry field definition, optional
            // geometry_column(:wkb_type:srid)?
            QRegExp reGeom( "(\\w+)(?::([a-zA-Z0-9]+):(\\d+))?" );
            int pos = reGeom.indexIn( value );
            if ( pos >= 0 ) {
                geometryField.name = reGeom.cap(1);
                if ( reGeom.captureCount() > 1 ) {
                    // not used by the spatialite provider for now ...
                    int wkb_type = geometry_type_to_wkb_type( reGeom.cap(2) );
                    if (wkb_type == 0) {
                        wkb_type = reGeom.cap(2).toLong();
                    }
                    geometryField.srid = reGeom.cap(3).toLong();
                }
            }
        }
        else if ( key == "nogeometry" ) {
            noGeometry = true;
        }
        else if ( key == "field" ) {
            QRegExp reGeom( "(\\w+):(int|integer|real|double|string|text)" );
            int pos = reGeom.indexIn( value );
            if ( pos >= 0 ) {
                QVariant::Type type;
                if ( (reGeom.cap(2) == "int")||(reGeom.cap(2) == "integer") ) {
                    type = QVariant::Int;
                }
                else if ( (reGeom.cap(2) == "real")||(reGeom.cap(2) == "double") ) {
                    type = QVariant::Double;
                }
                else {
                    type = QVariant::String;
                }
                forcedFields.append( QgsField( reGeom.cap(1), type, reGeom.cap(2) ) );
            }
        }
        else if ( key == "uid" ) {
            mUid = value;
        }
        else if ( key == "query" ) {
            // url encoded query
            mQuery = QUrl::fromPercentEncoding(value.toLocal8Bit());
        }
    }

    // consistency check
    if ( mLayers.size() > 1 && mQuery.isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Don't know how to join layers, please specify a query") );
        return;
    }

    if ( !mQuery.isEmpty() && mUid.isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Please specify a 'uid' column name") );
        return;
    }

    if ( mLayers.empty() && mPath.isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Please specify at least one source layer") );
        return;
    }

    bool openIt = mLayers.empty() && !mPath.isEmpty() && mQuery.isEmpty();
    if ( !mPath.isEmpty() && !openIt ) {
        // file creation, delete it if it already exists
        QFile::remove( mPath );
    }
    // use a temporary file if needed
    if ( mPath.isEmpty() ) {
        mTempFile.reset( new QTemporaryFile() );
        mTempFile->open();
        mPath = mTempFile->fileName();
        mTempFile->close();
    }

    spatialite_init(0);

    sqlite3* db;
    // open and create if it does not exist
    int r = sqlite3_open( mPath.toUtf8().constData(), &db );
    if ( r ) {
        mValid = false;
        PROVIDER_ERROR( QString( sqlite3_errmsg(db) ) );
        return;
    }
    mSqlite.reset(db);

    if (!openIt) {
        SqliteQuery::exec( mSqlite.data(), "SELECT InitSpatialMetadata(1);" );
    }

    bool has_geometry = false;
    // now create virtual tables based on layers
    for ( int i = 0; i < mLayers.size(); i++ ) {
        QgsVectorLayer* vlayer = mLayers.at(i).layer;
        QString vname = mLayers.at(i).name;
        if ( vlayer ) {
            QString createStr = QString("DROP TABLE IF EXISTS %1; CREATE VIRTUAL TABLE %1 USING QgsVLayer(%2);").arg(vname).arg(vlayer->id());
            SqliteQuery::exec( mSqlite.data(), createStr );
        }
        else {
            QString provider = mLayers.at(i).provider;
            QString source = mLayers.at(i).source;
            QString createStr = QString( "DROP TABLE IF EXISTS %1; CREATE VIRTUAL TABLE %1 USING QgsVLayer(%2,'%3')").arg(vname).arg(provider).arg(source);
            SqliteQuery::exec( mSqlite.data(), createStr );
        }

        // check geometry field
        {
            SqliteQuery q( db, QString("SELECT * FROM virts_geometry_columns WHERE virt_name='%1'").arg(vname) );
            if ( q.step() == SQLITE_ROW ) {
                has_geometry = true;
            }
        }
    }

    QString tableName;
    if ( !mQuery.isEmpty() ) {
        // create a temporary view, in order to call table_info on it
        QString viewStr = "CREATE TEMPORARY VIEW vview AS " + mQuery;

        SqliteQuery::exec( mSqlite.data(), viewStr );

        // look for column names and types
        getSqliteFields( mSqlite.data(), "vview", mFields, mGeometryFields );

        // process geometry field
        if ( !geometryField.name.isEmpty() ) {
            // requested geometry field is in mFields => remove it from mFields and add it to mGeometryFields
            bool found = false;
            for ( int i = 0; i < mFields.count(); i++ ) {
                if ( mFields[i].name() == geometryField.name ) {
                    mFields.remove(i);
                    GeometryField g;
                    g.name = geometryField.name;
                    mGeometryFields.append( g );
                    found = true;
                    break;
                }
            }
            // if not found in mFields, look in mGeometryFields
            if ( !found ) {
                for ( int i = 0; !found && i < mGeometryFields.size(); i++ ) {
                    if ( mGeometryFields[i].name == geometryField.name ) {
                        found = true;
                        break;
                    }
                }
            }
            if ( !found ) {
                mValid = false;
                PROVIDER_ERROR( "Cannot find the specified geometry field !" );
                return;
            }
        }
        if ( geometryField.name.isEmpty() && !noGeometry && mGeometryFields.size() > 0 ) {
            // take the first
            geometryField.name = mGeometryFields[0].name;
        }

        if ( !noGeometry ) {
            SqliteQuery::exec( mSqlite.data(), QString("INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, '%1', '::')" ).arg(geometryField.name) );
        }

        tableName = "(" + mQuery + ")";
        SqliteQuery q(mSqlite.data(), "INSERT OR REPLACE INTO _tables (id, name, source) VALUES (0, ?, ?)");
        q.bind(mUid).bind(mQuery);
        q.step();
    }
    else if ( openIt ) {
        // a path, but no layers and no query => open an existing virtual layer
        {
            SqliteQuery q( db, "SELECT name FROM sqlite_master WHERE name='_meta' OR name='_tables' OR name='_columns'" );
            int cnt = 0;
            while ( sqlite3_step(q.stmt()) == SQLITE_ROW ) {
                cnt++;
            }
            if ( cnt != 3 ) {
                mValid = false;
                PROVIDER_ERROR( "No metadata tables !" );
                return;            
            }
        }
        // look for the correct version
        {
            SqliteQuery q( db, "SELECT version FROM _meta" );
            int version = 0;
            if (sqlite3_step(q.stmt()) == SQLITE_ROW) {
                version = sqlite3_column_int( q.stmt(), 0 );
            }
            if (version != VIRTUAL_LAYER_VERSION) {
                mValid = false;
                PROVIDER_ERROR( "Bad virtual layer version !" );
                return;            
            }
        }
        // look for a query, if any
        {
            SqliteQuery q( db, "SELECT id, name, source, provider FROM _tables" );
            while ( sqlite3_step(q.stmt()) == SQLITE_ROW ) {
                int id = sqlite3_column_int( q.stmt(), 0 );
                if ( id == 0 ) { // query
                    mQuery = QString((const char*)sqlite3_column_text( q.stmt(), 2 ));
                    // name stores the UID field, if any
                    mUid = QString((const char*)sqlite3_column_text( q.stmt(), 1 ));
                }
                else {
                    mLayers.append( SourceLayer( (const char*)sqlite3_column_text( q.stmt(), 3 ),
                                                 (const char*)sqlite3_column_text( q.stmt(), 2 ),
                                                 (const char*)sqlite3_column_text( q.stmt(), 1 ) ) );
                }
            }
        }
        // look for field overloaded types and geometry
        {
            SqliteQuery q( db, "SELECT name, type FROM _columns WHERE table_id=0" );
            while ( sqlite3_step(q.stmt()) == SQLITE_ROW ) {
                QString colname( (const char*)sqlite3_column_text( q.stmt(), 0 ) );
                QString coltype( (const char*)sqlite3_column_text( q.stmt(), 1 ) );
                if ( coltype.contains(':') ) {
                    // geometry field
                    geometryField.name = colname;
                }
                else {
                    QVariant::Type t = QVariant::nameToType( coltype.toLocal8Bit().constData() );
                    forcedFields.append( QgsField( colname, t, coltype ) );
                }
            }
        }

        if (mQuery.isEmpty()) {
            tableName = mLayers[0].name;
        }
        else {
            tableName = "(" + mQuery + ")";
        }
    }
    else {
        // no query => implies we must only have one virtual table
        if (has_geometry && !noGeometry) {
            geometryField.name = "geometry";
            std::cout << "write geometry column" << std::endl;

            SqliteQuery::exec( mSqlite.data(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', '::')" );
        }
        tableName = mLayers[0].name;
        mUid = "";
    }

    QgsDataSourceURI source;
    source.setDatabase( mPath );
    source.setDataSource( "", tableName, geometryField.name.isEmpty() ? "" : geometryField.name, "", mUid );
    std::cout << "Spatialite uri: " << source.uri().toUtf8().constData() << std::endl;
    mSpatialite.reset( new QgsSpatiaLiteProvider( source.uri() ) );
    mFields = mSpatialite->fields();
        
    // force type of fields, if needed
    for ( int i = 0; i < mFields.count(); i++ ) {
        for ( int j = 0; j < forcedFields.count(); j++ ) {
            if (forcedFields[j].name() == mFields[i].name()) {
                mFields[i] = forcedFields[j];
                break;
            }
        }
    }

    // write metadata
    mValid = mSpatialite->isValid();
    }
    catch (std::runtime_error& e) {
        mValid = false;
        PROVIDER_ERROR( e.what() );
        return;
    }

    // connect to layer removal signals
    connect( QgsMapLayerRegistry::instance(), SIGNAL(layerRemoved(QString)), this, SLOT(onLayerRemoved(QString)) );
}


QgsVirtualLayerProvider::~QgsVirtualLayerProvider()
{
    disconnect( QgsMapLayerRegistry::instance(), SIGNAL(layerRemoved(QString)), this, SLOT(onLayerRemoved(QString)) );
}

void QgsVirtualLayerProvider::onLayerRemoved( QString layerid )
{
    for ( auto& layer : mLayers ) {
        if (layer.layer && layer.layer->id() == layerid ) {
            // must drop the corresponding virtual table
            SqliteQuery::exec( mSqlite.data(), QString("DROP TABLE %1").arg(layer.name) );
        }
    }
}

QgsAbstractFeatureSource* QgsVirtualLayerProvider::featureSource() const
{
    return mSpatialite->featureSource();
}

QString QgsVirtualLayerProvider::storageType() const
{
    return "No storage per se, view data from other data sources";
}

QgsCoordinateReferenceSystem QgsVirtualLayerProvider::crs()
{
    return mSpatialite->crs();
}

QgsFeatureIterator QgsVirtualLayerProvider::getFeatures( const QgsFeatureRequest& request )
{
    return mSpatialite->getFeatures( request );
}

QString QgsVirtualLayerProvider::subsetString()
{
    return mSpatialite->subsetString();
}

bool QgsVirtualLayerProvider::setSubsetString( QString theSQL, bool updateFeatureCount )
{
    return mSpatialite->setSubsetString( theSQL, updateFeatureCount );
}


QGis::WkbType QgsVirtualLayerProvider::geometryType() const
{
    return mSpatialite->geometryType();
}

size_t QgsVirtualLayerProvider::layerCount() const
{
    return mLayers.size();
}

long QgsVirtualLayerProvider::featureCount() const
{
    return mSpatialite->featureCount();
}

QgsRectangle QgsVirtualLayerProvider::extent()
{
    return mSpatialite->extent();
}

void QgsVirtualLayerProvider::updateExtents()
{
    mSpatialite->updateExtents();
}

const QgsFields & QgsVirtualLayerProvider::fields() const
{
    return mFields;
}

QVariant QgsVirtualLayerProvider::minimumValue( int index )
{
    return mSpatialite->minimumValue( index );
}

QVariant QgsVirtualLayerProvider::maximumValue( int index )
{
    return mSpatialite->maximumValue( index );
}

void QgsVirtualLayerProvider::uniqueValues( int index, QList < QVariant > &uniqueValues, int limit )
{
    return mSpatialite->uniqueValues( index, uniqueValues, limit );
}

bool QgsVirtualLayerProvider::isValid()
{
    return mValid;
}

int QgsVirtualLayerProvider::capabilities() const
{
    return 0; //SelectAtId | SelectGeometryAtId;
}

QString QgsVirtualLayerProvider::name() const
{
    return VIRTUAL_LAYER_KEY;
}

QString QgsVirtualLayerProvider::description() const
{
    return VIRTUAL_LAYER_DESCRIPTION;
}

QgsAttributeList QgsVirtualLayerProvider::pkAttributeIndexes()
{
    return mSpatialite->pkAttributeIndexes();
}

/**
 * Class factory to return a pointer to a newly created
 * QgsSpatiaLiteProvider object
 */
QGISEXTERN QgsVirtualLayerProvider *classFactory( const QString * uri )
{
    // register sqlite extension
    // BE CAREFUL it will also be loaded for every new SQLite connections
    sqlite3_auto_extension( (void(*)())qgsvlayer_module_init );

    return new QgsVirtualLayerProvider( *uri );
}

/** Required key function (used to map the plugin to a data store type)
*/
QGISEXTERN QString providerKey()
{
  return VIRTUAL_LAYER_KEY;
}

/**
 * Required description function
 */
QGISEXTERN QString description()
{
  return VIRTUAL_LAYER_DESCRIPTION;
}

/**
 * Required isProvider function. Used to determine if this shared library
 * is a data provider plugin
 */
QGISEXTERN bool isProvider()
{
  return true;
}

QGISEXTERN void cleanupProvider()
{
    // unregister sqlite extension
    sqlite3_cancel_auto_extension( (void(*)())qgsvlayer_module_init );
}
