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
#include "qgsvirtuallayerdefinition.h"
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

int QgsVirtualLayerProvider::mNonce = 0;

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

    // read url
    mDefinition = QgsVirtualLayerDefinition( url );

    // geometry field to consider (if more than one or if it cannot be detected)
    GeometryField geometryField;

    // consistency check
    if ( mDefinition.sourceLayers().size() > 1 && mDefinition.query().isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Don't know how to join layers, please specify a query") );
        return;
    }

    if ( !mDefinition.query().isEmpty() && mDefinition.uid().isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Please specify a 'uid' column name") );
        return;
    }

    if ( mDefinition.sourceLayers().empty() && mDefinition.uri().isEmpty() ) {
        mValid = false;
        PROVIDER_ERROR( QString("Please specify at least one source layer") );
        return;
    }

    mPath = mDefinition.uri();
    bool openIt = mDefinition.sourceLayers().empty() && !mPath.isEmpty() && mDefinition.query().isEmpty();
    // use a temporary file if needed
    if ( mPath.isEmpty() ) {
        mTempFile.reset( new QTemporaryFile() );
        mTempFile->open();
        // The spatialite QGIS provider has a strange behaviour when the same filename is used, even after closing and re-opening (bug #12266)
        // handles are not freed. It results in tables that are not found, because it is still pointing on an old version of the file (in memory ?)
        // We then add something changing to the filename to make sure it is unique
        mPath = mTempFile->fileName() + QString("%1").arg(mNonce++);
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

    bool noGeometry = false;
    bool has_geometry = false;

    if ( openIt ) {
        // fill mDefinition with information from the sqlite file
        mDefinition = virtualLayerDefinitionFromSqlite( mPath );
        // check geometry field
        {
            SqliteQuery q( db, "SELECT * FROM virts_geometry_columns" );
            if ( q.step() == SQLITE_ROW ) {
                has_geometry = true;
            }
        }
    }

    noGeometry = mDefinition.geometryField() == "*no*";
    if (!noGeometry) {
        geometryField.name = mDefinition.geometryField();
    }

    for ( auto& layer : mDefinition.sourceLayers() ) {
        if ( layer.isReferenced() ) {
            QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( layer.reference() );
            if ( l == 0 ) {
                mValid = false;
                PROVIDER_ERROR( QString("Cannot find layer %1").arg(layer.reference()) );
                return;
            }
            if ( l->type() != QgsMapLayer::VectorLayer ) {
                mValid = false;
                PROVIDER_ERROR( QString("Layer %1 is not a vector layer").arg(layer.reference()) );
                return;
            }
            // add the layer to the list
            mLayers << SourceLayer(static_cast<QgsVectorLayer*>(l), layer.name());
        }
        else {
            mLayers << SourceLayer(layer.provider(), layer.source(), layer.name());
        }
    }

    if ( !openIt ) {
        try {
            resetSqlite();

            // now create virtual tables based on layers
            for ( int i = 0; i < mLayers.size(); i++ ) {
                QgsVectorLayer* vlayer = mLayers.at(i).layer;
                QString vname = mLayers.at(i).name;
                if ( vlayer ) {
                    QString createStr = QString("DROP TABLE IF EXISTS %1; CREATE VIRTUAL TABLE %1 USING QgsVLayer(%2);").arg(vname).arg(vlayer->id());
                    SqliteQuery::exec( mSqlite.get(), createStr );
                }
                else {
                    QString provider = mLayers.at(i).provider;
                    QString source = mLayers.at(i).source;
                    QString createStr = QString( "DROP TABLE IF EXISTS %1; CREATE VIRTUAL TABLE %1 USING QgsVLayer(%2,'%3')").arg(vname).arg(provider).arg(source);
                    SqliteQuery::exec( mSqlite.get(), createStr );
                }

                // check geometry field
                {
                    SqliteQuery q( db, QString("SELECT * FROM virts_geometry_columns WHERE virt_name='%1'").arg(vname) );
                    if ( q.step() == SQLITE_ROW ) {
                        has_geometry = true;
                    }
                }
            }

            QList<QString> geometryFields;
            if ( !mDefinition.query().isEmpty() ) {
                // create a temporary view, in order to call table_info on it
                QString viewStr = "CREATE TEMPORARY VIEW vview AS " + mDefinition.query();

                SqliteQuery::exec( mSqlite.get(), viewStr );

                // look for column names and types
                GeometryFields gFields;
                getSqliteFields( mSqlite.get(), "vview", mFields, gFields );

                // process geometry field
                if ( !geometryField.name.isEmpty() ) {
                    // requested geometry field is in mFields => remove it from mFields and add it to mGeometryFields
                    bool found = false;
                    for ( int i = 0; i < mFields.count(); i++ ) {
                        if ( mFields[i].name() == geometryField.name ) {
                            mFields.remove(i);
                            GeometryField g;
                            g.name = geometryField.name;
                            gFields.append( g );
                            found = true;
                            break;
                        }
                    }
                    // if not found in mFields, look in mGeometryFields
                    if ( !found ) {
                        for ( int i = 0; !found && i < gFields.size(); i++ ) {
                            if ( gFields[i].name == geometryField.name ) {
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
                if ( geometryField.name.isEmpty() && !noGeometry && gFields.size() > 0 ) {
                    // take the first
                    geometryField.name = gFields[0].name;
                }

                if ( !noGeometry ) {
                    SqliteQuery::exec( mSqlite.get(), QString("INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, '%1', '::')" ).arg(geometryField.name) );
                }
                else {
                    SqliteQuery::exec( mSqlite.get(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', 'no::')" );
                }

                SqliteQuery q(mSqlite.get(), "INSERT OR REPLACE INTO _tables (id, name, source) VALUES (0, ?, ?)");
                q.bind(mDefinition.uid()).bind(mDefinition.query());
                q.step();
            }
            else {
                // no query => implies we must only have one virtual table
                if (has_geometry && !noGeometry) {
                    SqliteQuery::exec( mSqlite.get(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', '::')" );
                }
                else {
                    SqliteQuery::exec( mSqlite.get(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', 'no::')" );
                }
            }
        }
        catch (std::runtime_error& e) {
            mValid = false;
            PROVIDER_ERROR( e.what() );
            return;
        }
    }

    QString tableName;
    if ( mDefinition.query().isEmpty() ) {
        tableName = mLayers[0].name;
        if ( has_geometry && !noGeometry ) {
            geometryField.name = "geometry";
        }
        mDefinition.setUid("");
    }
    else {
        tableName = "(" + mDefinition.query() + ")";
    }
    QgsDataSourceURI source;
    source.setDatabase( mPath );
    source.setDataSource( "", tableName, geometryField.name, "", mDefinition.uid() );
    std::cout << "Spatialite uri: " << source.uri().toUtf8().constData() << std::endl;
    mSpatialite.reset( new QgsSpatiaLiteProvider( source.uri() ) );
    mFields = mSpatialite->fields();
        
    // force type of fields, if needed
    QgsFields oFields = mDefinition.overridenFields();
    for ( int i = 0; i < mFields.count(); i++ ) {
        for ( int j = 0; j < oFields.count(); j++ ) {
            if (oFields[j].name() == mFields[i].name()) {
                mFields[i] = oFields[j];
                break;
            }
        }
    }

    // write metadata
    mValid = mSpatialite->isValid();

    // connect to layer removal signals
    for ( auto& layer : mLayers ) {
        if ( layer.layer ) {
            connect( layer.layer, SIGNAL(layerDeleted()), this, SLOT(onLayerDeleted()) );
        }
    }
}


QgsVirtualLayerProvider::~QgsVirtualLayerProvider()
{
    if ( mTempFile ) {
        // if we have been using a temporary file, delete it (the one with the nonce)
        QFile::remove( mPath );
    }
}

void QgsVirtualLayerProvider::resetSqlite()
{
    QStringList toDrop;
    QString sql;

    bool has_mtables = false;
    {
        SqliteQuery q( mSqlite.get(), "SELECT name FROM sqlite_master WHERE name='_tables'" );
        has_mtables = q.step() == SQLITE_ROW;
    }
    if ( has_mtables ) {
        SqliteQuery q( mSqlite.get(), "SELECT name FROM _tables WHERE id>0" );
        while ( q.step() == SQLITE_ROW ) {
            sql += "DROP TABLE " + QString((const char*)sqlite3_column_text( q.stmt(), 0 )) + ";";
        }
        sql += "DELETE FROM _tables;";
        sql += "DELETE FROM _columns;";
        sql += "DROP TABLE _meta;";
    }
    bool has_spatialrefsys = false;
    {
        SqliteQuery q( mSqlite.get(), "SELECT name FROM sqlite_master WHERE name='spatial_ref_sys'" );
        has_spatialrefsys = q.step() == SQLITE_ROW;
    }
    if (!has_spatialrefsys) {
        sql += "SELECT InitSpatialMetadata(1);";
    }
    SqliteQuery::exec( mSqlite.get(), sql );
}

void QgsVirtualLayerProvider::onLayerDeleted()
{
    std::cout << "layer deleted " << sender() << std::endl;
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(sender());
    for ( auto& layer : mLayers ) {
        if (layer.layer && layer.layer->id() == vl->id() ) {
            // must drop the corresponding virtual table
            std::cout << "layer removed, drop virtual table " << layer.name.toUtf8().constData() << std::endl;
            SqliteQuery::exec( mSqlite.get(), QString("DROP TABLE %1").arg(layer.name) );
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
