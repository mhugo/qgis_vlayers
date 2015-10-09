/***************************************************************************
           qgsvirtuallayerprovider.cpp Virtual layer data provider
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

extern "C" {
#include <sqlite3.h>
#include <spatialite.h>
}

#include <QUrl>

#include <qgsvirtuallayerprovider.h>
#include <qgsvirtuallayerdefinition.h>
#include <qgsvirtuallayerfeatureiterator.h>
#include <qgssql.h>
#include <qgsvectorlayer.h>
#include <qgsmaplayerregistry.h>
#include <qgsdatasourceuri.h>
#include "sqlite_helper.h"
#include "vlayer_module.h"

const QString VIRTUAL_LAYER_KEY = "virtual";
const QString VIRTUAL_LAYER_DESCRIPTION = "Virtual layer data provider";

static QString quotedColumn( QString name )
{
    return "\"" + name.replace("\"", "\"\"") + "\"";
}

// declaration of the spatialite module
extern "C" {
    int qgsvlayer_module_init();
}

#define PROVIDER_ERROR( msg ) do { mError = QgsError( msg, VIRTUAL_LAYER_KEY ); QgsDebugMsg( msg ); } while(0)


int QgsVirtualLayerProvider::mNonce = 0;

QgsVirtualLayerProvider::QgsVirtualLayerProvider( QString const &uri )
    : QgsVectorDataProvider( uri ),
      mCachedStatistics( false ),
      mValid( true )
{
    mError.clear();

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
    try {
        mDefinition = QgsVirtualLayerDefinition( url );

        if ( mDefinition.sourceLayers().empty() && !mDefinition.uri().isEmpty() && mDefinition.query().isEmpty() ) {
            // open the file
            mValid = openIt();
        }
        else {
            // create the file
            mValid = createIt();
        }
    }
    catch (std::runtime_error& e) {
        mValid = false;
        PROVIDER_ERROR( e.what() );
        return;
    }

    // connect to layer removal signals
    foreach ( const SourceLayer& layer, mLayers ) {
        if ( layer.layer ) {
            connect( layer.layer, SIGNAL(layerDeleted()), this, SLOT(onLayerDeleted()) );
        }
    }

    if (mDefinition.geometrySrid() != -1 ) {
        mCrs = QgsCoordinateReferenceSystem( mDefinition.geometrySrid() );
    }
}

bool QgsVirtualLayerProvider::loadSourceLayers()
{
    foreach ( const QgsVirtualLayerDefinition::SourceLayer& layer, mDefinition.sourceLayers() ) {
        if ( layer.isReferenced() ) {
            QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( layer.reference() );
            if ( l == 0 ) {
                PROVIDER_ERROR( QString("Cannot find layer %1").arg(layer.reference()) );
                return false;
            }
            if ( l->type() != QgsMapLayer::VectorLayer ) {
                PROVIDER_ERROR( QString("Layer %1 is not a vector layer").arg(layer.reference()) );
                return false;
            }
            // add the layer to the list
            mLayers << SourceLayer(static_cast<QgsVectorLayer*>(l), layer.name());
        }
        else {
            mLayers << SourceLayer(layer.provider(), layer.source(), layer.name(), layer.encoding() );
        }
    }
    return true;
}

bool QgsVirtualLayerProvider::openIt()
{
    spatialite_init(0);

    mPath = mDefinition.uri();

    // fill mDefinition with information from the sqlite file
    mDefinition = virtualLayerDefinitionFromSqlite( mPath );

    sqlite3* db;
    // open the file
    int r = sqlite3_open( mPath.toUtf8().constData(), &db );
    if ( r ) {
        PROVIDER_ERROR( QString( sqlite3_errmsg(db) ) );
        return false;
    }
    mSqlite.reset(db);

    // load source layers
    if (!loadSourceLayers()) {
        return false;
    }

    mFields = mDefinition.overridenFields();

    /* only one table */
    if ( mDefinition.query().isEmpty() ) {
        mTableName = mLayers[0].name;
        if ( mDefinition.geometryField() != "*no*" ) {
            Sqlite::Query q( mSqlite.get(), "SELECT type FROM _columns WHERE table_id=(SELECT id FROM _tables WHERE name=?) AND name='*geometry*'" );
            q.bind(mTableName);
            if ( q.step() == SQLITE_ROW ) {
                QString geom = q.column_text(0);
                QStringList sl = geom.split(":");
                mDefinition.setGeometryWkbType( QGis::WkbType(sl[0].toInt()) );
                if (sl.size() == 3 ) {
                    mDefinition.setGeometrySrid( sl[2].toLong() );
                }
            }
        }
    }
    else {
        mTableName = "_view";
    }

    return true;
}

bool QgsVirtualLayerProvider::createIt()
{
    // consistency check
    if ( mDefinition.sourceLayers().size() > 1 && mDefinition.query().isEmpty() ) {
        PROVIDER_ERROR( QString("Don't know how to join layers, please specify a query") );
        return false;
    }

    if ( mDefinition.sourceLayers().empty() && mDefinition.uri().isEmpty() && mDefinition.query().isEmpty() ) {
        PROVIDER_ERROR( QString("Please specify at least one source layer or a query") );
        return false;
    }

    if ( !mDefinition.uri().isEmpty() && mDefinition.hasReferencedLayers() ) {
        PROVIDER_ERROR( QString("Cannot store referenced layers") );
        return false;        
    }

    QScopedPointer<QgsSql::Node> queryTree;
    QList<QgsSql::ColumnType> fields, gFields;
    QgsSql::TableDefs refTables;
    if ( !mDefinition.query().isEmpty() ) {
        // deduce sources from the query
        QString error;
        queryTree.reset( QgsSql::parseSql( mDefinition.query(), error ) );
        if ( !queryTree ) {
            PROVIDER_ERROR( "SQL parsing error: " + error );
            return false;
        }

        // look for layers
        QList<QString> tables = referencedTables( *queryTree );
        foreach ( const QString& tname, tables ) {
            // is it in source layers ?
            if ( mDefinition.hasSourceLayer( tname ) ) {
                continue;
            }
            // is it in loaded layers ?
            bool found = false;
            foreach ( const QgsMapLayer* l, QgsMapLayerRegistry::instance()->mapLayers() ) {
                if ( l->type() != QgsMapLayer::VectorLayer ) {
                    PROVIDER_ERROR( QString( "Referenced table %1 is not a vector layer!" ).arg(tname) );
                    return false;
                }
                const auto vl = static_cast<const QgsVectorLayer*>(l);
                if ((vl->name() == tname) || (vl->id() == tname)) {
                    mDefinition.addSource( tname, vl->source(), vl->providerType(), vl->dataProvider()->encoding() );
                    found = true;
                    break;
                }
            }
            if (!found) {
                PROVIDER_ERROR( QString( "Referenced table %1 in query not found!" ).arg(tname) );
                return false;
            }
        }
    }

    mPath = mDefinition.uri();
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
        PROVIDER_ERROR( QString( sqlite3_errmsg(db) ) );
        return false;
    }
    mSqlite.reset(db);
    resetSqlite();
    initMetadata( mSqlite.get() );

    bool noGeometry = false;

    // geometry field to consider (if more than one or if it cannot be detected)
    QgsSql::ColumnType reqGeometryField;

    noGeometry = mDefinition.geometryField() == "*no*";
    if (!noGeometry) {
        reqGeometryField.setName( mDefinition.geometryField() );
        reqGeometryField.setGeometry( mDefinition.geometryWkbType() );
        reqGeometryField.setSrid( mDefinition.geometrySrid() );
    }

    if (!loadSourceLayers()) {
        return false;
    }

    // now create virtual tables based on layers
    for ( int i = 0; i < mLayers.size(); i++ ) {
        QgsVectorLayer* vlayer = mLayers.at(i).layer;
        QString vname = mLayers.at(i).name;
        if ( vlayer ) {
            QString createStr = QString("DROP TABLE IF EXISTS \"%1\"; CREATE VIRTUAL TABLE \"%1\" USING QgsVLayer(%2);").arg(vname).arg(vlayer->id());
            Sqlite::Query::exec( mSqlite.get(), createStr );
        }
        else {
            QString provider = mLayers.at(i).provider;
            QString source = mLayers.at(i).source;
            QString encoding = mLayers.at(i).encoding;
            QString createStr = QString( "DROP TABLE IF EXISTS \"%1\"; CREATE VIRTUAL TABLE \"%1\" USING QgsVLayer(%2,'%4',%3)")
                .arg(vname)
                .arg(provider)
                .arg(encoding)
                .arg(source); // source must be the last argument here, since it can contains '%x' strings that would be replaced
            Sqlite::Query::exec( mSqlite.get(), createStr );
        }
    }

    // add columns of virtual tables to the context
    if ( mLayers.size() > 0 ) {
        Sqlite::Query q( mSqlite.get(), "SELECT t.name, c.name, type FROM _columns as c, _tables as t WHERE c.table_id = t.id ORDER BY c.table_id" );
        while (q.step() == SQLITE_ROW) {
            QString tableName = q.column_text(0);
            QString columnName = q.column_text(1);
            QString columnType = q.column_text(2);
            if ( columnName != "*geometry*" ) {
                QVariant::Type t = QVariant::nameToType( columnType.toUtf8().constData() );
                refTables[tableName] << QgsSql::ColumnType( columnName, t );
            }
            else {
                QStringList ls = columnType.split(':');
                QgsSql::ColumnType c;
                if (ls.size() == 3) {
                    c.setName( "geometry" );
                    c.setGeometry( QGis::WkbType( ls[0].toLong() ) );
                    c.setSrid( ls[2].toLong() );
                    refTables[tableName] << c;
                }
            }
        }
    }

    QList<QString> geometryFields;
    if ( !mDefinition.query().isEmpty() ) {
        // look for column types of the query
        {
            QString err;
            QList<QgsSql::ColumnType> columns = QgsSql::columnTypes( *queryTree, err, &refTables );
            if ( !err.isEmpty() ) {
                PROVIDER_ERROR( err );
                return false;
            }

            int i = 1;
            foreach ( QgsSql::ColumnType c, columns ) {
                if ( c.isGeometry() ) {
                    gFields << c;
                }
                else {
                    if ( c.name().isEmpty() ) {
                        PROVIDER_ERROR( QString("Result column #%1 has no name !").arg(i) );
                        return false;
                    }
                    if ( c.scalarType() == QVariant::Invalid ) {
                        c.setScalarType( QVariant::String );
                    }
                    // if the geometry field is detected with a scalar type, move it to the geometry fields
                    if ( c.name() == reqGeometryField.name() ) {
                      if ( (reqGeometryField.wkbType() != QGis::WKBNoGeometry) && (reqGeometryField.wkbType() != QGis::WKBUnknown) ) {
                        gFields << reqGeometryField;
                      }
                      else {
                        PROVIDER_ERROR( "Can't deduce the geometry type of the geometry field !" );
                        return false;
                      }
                    }
                    else {
                        mFields.append( QgsField( c.name(), c.scalarType() ) );
                    }
                }
                i++;
            }
        }

        // process geometry field
        if ( !reqGeometryField.name().isEmpty() ) {
            bool found = false;
            for ( int i = 0; i < gFields.size(); i++ ) {
                if ( gFields[i].name() == reqGeometryField.name() ) {
                    reqGeometryField = gFields[i];
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                PROVIDER_ERROR( "Cannot find the specified geometry field !" );
                return false;
            }
        }
        if ( reqGeometryField.name().isEmpty() && !noGeometry && gFields.size() > 0 ) {
            // take the first
            reqGeometryField = gFields[0];
        }

        // save fields
        Sqlite::Query::exec( mSqlite.get(), "BEGIN" );
        Sqlite::Query qq( mSqlite.get(), "INSERT INTO _columns (table_id, name, type) VALUES (0, ?, ?)" );
        for ( int i = 0; i < mFields.size(); i++ ) {
            qq.reset();
            qq.bind( mFields.at(i).name() );
            qq.bind( QVariant::typeToName(mFields.at(i).type()) );
            qq.step();
        }

        if ( !noGeometry ) {
            Sqlite::Query::exec( mSqlite.get(), QString("INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, '%1', '%2:%3')" )
                                 .arg(reqGeometryField.name())
                                 .arg(reqGeometryField.wkbType())
                                 .arg(reqGeometryField.srid()) );
        }
        else {
            Sqlite::Query::exec( mSqlite.get(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', 'no::')" );
        }

        Sqlite::Query q(mSqlite.get(), "INSERT OR REPLACE INTO _tables (id, name, source) VALUES (0, ?, ?)");
        q.bind(mDefinition.uid()).bind(mDefinition.query());
        q.step();
        mTableName = "_view";

        // create a view
        QString viewStr = "DROP VIEW IF EXISTS _view; CREATE VIEW _view AS " + mDefinition.query();
        Sqlite::Query::exec( mSqlite.get(), viewStr );

        // end transaction
        Sqlite::Query::exec( mSqlite.get(), "COMMIT" );
    }
    else {
        // no query => implies we must only have one virtual table
        mTableName = mLayers[0].name;

        // attributes have already been loaded
        QgsSql::TableDef td = refTables[mTableName];
        bool has_geometry = false;
        foreach ( const QgsSql::ColumnType& c, td ) {
            if ( !c.isGeometry() ) {
                mFields.append( QgsField( c.name(), c.scalarType() ) );
            }
            else if ( !noGeometry && !has_geometry ) {
                reqGeometryField.setName("geometry");
                reqGeometryField.setGeometry( c.wkbType() );
                reqGeometryField.setSrid( c.srid() );
                Sqlite::Query::exec( mSqlite.get(), QString("INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', '%1:%2')" )
                                     .arg(reqGeometryField.wkbType())
                                     .arg(reqGeometryField.srid()) );
                has_geometry = true;
            }
        }
        if (noGeometry) {
            Sqlite::Query::exec( mSqlite.get(), "INSERT OR REPLACE INTO _columns (table_id, name, type) VALUES (0, 'geometry', 'no::')" );
        }
    }

    // save the geometry type back
    if (!noGeometry) {
        mDefinition.setGeometryField( reqGeometryField.name() );
        mDefinition.setGeometryWkbType( reqGeometryField.wkbType() );
        mDefinition.setGeometrySrid( reqGeometryField.srid() );
    }
    else {
        mDefinition.setGeometryField( "*no*" );
        mDefinition.setGeometryWkbType( QGis::WKBNoGeometry );
    }

    return true;
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
        Sqlite::Query q( mSqlite.get(), "SELECT name FROM sqlite_master WHERE name='_tables'" );
        has_mtables = q.step() == SQLITE_ROW;
    }
    if ( has_mtables ) {
        Sqlite::Query q( mSqlite.get(), "SELECT name FROM _tables WHERE id>0" );
        while ( q.step() == SQLITE_ROW ) {
            sql += "DROP TABLE \"" + q.column_text(0) + "\";";
        }
        sql += "DELETE FROM _tables;";
        sql += "DELETE FROM _columns;";
        sql += "DROP TABLE IF EXISTS _meta;";
    }
    bool has_spatialrefsys = false;
    {
        Sqlite::Query q( mSqlite.get(), "SELECT name FROM sqlite_master WHERE name='spatial_ref_sys'" );
        has_spatialrefsys = q.step() == SQLITE_ROW;
    }
    if (!has_spatialrefsys) {
        sql += "SELECT InitSpatialMetadata(1);";
    }
    Sqlite::Query::exec( mSqlite.get(), sql );
}

void QgsVirtualLayerProvider::onLayerDeleted()
{
    QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(sender());
    foreach ( const SourceLayer& layer, mLayers ) {
        if (layer.layer && layer.layer->id() == vl->id() ) {
            // must drop the corresponding virtual table
            Sqlite::Query::exec( mSqlite.get(), QString("DROP TABLE \"%1\"").arg(layer.name) );
        }
    }    
}

QgsAbstractFeatureSource* QgsVirtualLayerProvider::featureSource() const
{
    return new QgsVirtualLayerFeatureSource( this );
}

QString QgsVirtualLayerProvider::storageType() const
{
    return "No storage per se, view data from other data sources";
}

QgsCoordinateReferenceSystem QgsVirtualLayerProvider::crs()
{
    return mCrs;
}

QgsFeatureIterator QgsVirtualLayerProvider::getFeatures( const QgsFeatureRequest& request )
{
    return QgsFeatureIterator( new QgsVirtualLayerFeatureIterator( new QgsVirtualLayerFeatureSource( this ), false, request ) );
}

QString QgsVirtualLayerProvider::subsetString()
{
    return mSubset;
}

bool QgsVirtualLayerProvider::setSubsetString( QString theSQL, bool updateFeatureCount )
{
    mSubset = theSQL;
    return true;
}


QGis::WkbType QgsVirtualLayerProvider::geometryType() const
{
    return mDefinition.geometryWkbType();
}

size_t QgsVirtualLayerProvider::layerCount() const
{
    return mLayers.size();
}

long QgsVirtualLayerProvider::featureCount() const
{
    if ( !mCachedStatistics ) {
        updateStatistics();
    }
    return mFeatureCount;
}

QgsRectangle QgsVirtualLayerProvider::extent()
{
    if ( !mCachedStatistics ) {
        updateStatistics();
    }
    return mExtent;
}

void QgsVirtualLayerProvider::updateStatistics() const
{
    bool has_geometry = !mDefinition.geometryField().isEmpty() && mDefinition.geometryField() != "*no*";
    QString sql = QString( "SELECT Count(*)%1 FROM %2" )
        .arg( has_geometry ? QString( ",Min(MbrMinX(%1)),Min(MbrMinY(%1)),Max(MbrMaxX(%1)),Max(MbrMaxY(%1))" ).arg( quotedColumn( mDefinition.geometryField()) ) : "" )
        .arg( mTableName );
    Sqlite::Query q(mSqlite.get(), sql );
    if ( q.step() == SQLITE_ROW ) {
        mFeatureCount = q.column_int64(0);
        if (has_geometry) {
            double x1, y1, x2, y2;
            x1 = q.column_double(1);
            y1 = q.column_double(2);
            x2 = q.column_double(3);
            y2 = q.column_double(4);
            mExtent = QgsRectangle( q.column_double(1),
                                    q.column_double(2),
                                    q.column_double(3),
                                    q.column_double(4) );
        }
        mCachedStatistics = true;
    }
}

void QgsVirtualLayerProvider::updateExtents()
{
    //    mSpatialite->updateExtents();
}

const QgsFields & QgsVirtualLayerProvider::fields() const
{
    return mFields;
}

QVariant QgsVirtualLayerProvider::minimumValue( int index )
{
    return 0;
}

QVariant QgsVirtualLayerProvider::maximumValue( int index )
{
    return 0;
}

void QgsVirtualLayerProvider::uniqueValues( int index, QList < QVariant > &uniqueValues, int limit )
{
    //    return mSpatialite->uniqueValues( index, uniqueValues, limit );
}

bool QgsVirtualLayerProvider::isValid()
{
    return mValid;
}

int QgsVirtualLayerProvider::capabilities() const
{
    if ( !mDefinition.uid().isNull() ) {
        return SelectAtId | SelectGeometryAtId;
    }
    return 0;
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
    if ( !mDefinition.uid().isNull() ) {
        for ( int i = 0; i < mFields.size(); i++ ) {
            if ( mFields.at(i).name().toLower() == mDefinition.uid().toLower() ) {
                QgsAttributeList l;
                l << i;
                return l;
            }
        }
    }
    return QgsAttributeList();
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
    //sqlite3_cancel_auto_extension( (void(*)())qgsvlayer_module_init );
}
