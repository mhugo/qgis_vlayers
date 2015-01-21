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

const QString VIRTUAL_LAYER_KEY = "virtual";
const QString VIRTUAL_LAYER_DESCRIPTION = "Virtual layer data provider";

QgsVirtualLayerProvider::QgsVirtualLayerProvider( QString const &uri )
    : QgsVectorDataProvider( uri ),
      mValid( true ),
      mSpatialite( 0 )
{
    QUrl url = QUrl::fromEncoded( uri.toUtf8() );
    if ( !url.isValid() ) {
        mValid = false;
        mError = QgsError("Malformed URL", VIRTUAL_LAYER_KEY);
        return;
    }

    // xxxxx = open a virtual layer
    // xxxxx?key=value&key=value = create a virtual layer

    mPath = url.path();

    QList<QPair<QString, QString> > items = url.queryItems();
    for ( int i = 0; i < items.size(); i++ ) {
        QString key = items.at(i).first;
        QString value = items.at(i).second;
        if ( key == "layer_id" ) {
            QgsMapLayer *l = QgsMapLayerRegistry::instance()->mapLayer( value );
            if ( l == 0 ) {
                mValid = false;
                mError = QgsError( QString("Cannot find layer %1").arg(value), VIRTUAL_LAYER_KEY );
                return;
            }
            if ( l->type() != QgsMapLayer::VectorLayer ) {
                mValid = false;
                mError = QgsError( QString("Layer %1 is not a vector layer").arg(value), VIRTUAL_LAYER_KEY );
                return;
            }
            // add the layer to the list
            mLayers << static_cast<QgsVectorLayer*>(l);
        }
        else if ( key == "query" ) {
            mQuery = value;
        }
    }

    // consistency check
    if ( mLayers.size() > 1 && mQuery.isEmpty() ) {
        mValid = false;
        mError = QgsError( QString("Don't know how to join layers, please specify a query"), VIRTUAL_LAYER_KEY );
        return;
    }

    spatialite_init(0);
    sqlite3* db;
    // open and create if it does not exist
    int r = sqlite3_open( mPath.toUtf8().constData(), &db );
    if ( r ) {
        mValid = false;
        mError = QgsError( QString( sqlite3_errmsg(db) ), VIRTUAL_LAYER_KEY );
        return;
    }
    mSqlite.reset(db);

    // now create virtual tables based on layers
    int layer_idx = 0;
    for ( int i = 0; i < mLayers.size(); i++ ) {
        layer_idx++;
        QgsVectorLayer* vlayer = mLayers.at(i);

        QString geometry_type_str;
        int geometry_dim;
        int geometry_wkb_type;
        {
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
        long srid = vlayer->crs().postgisSrid();

        QString createStr = QString("SELECT InitSpatialMetadata(1); DROP TABLE IF EXISTS vtab%1; CREATE VIRTUAL TABLE vtab%1 USING QgsVLayer(%2);").arg(layer_idx).arg(vlayer->id());
        createStr += QString( "INSERT OR REPLACE INTO virts_geometry_columns (virt_name, virt_geometry, geometry_type, coord_dimension, srid) "
                              "VALUES ('vtab%1', 'geometry', %2, %3, %4 );" ).arg(layer_idx).arg(geometry_wkb_type).arg(geometry_dim).arg(srid);

        // manually set column statistics (needed for QGIS spatialite provider)
        QgsRectangle extent = vlayer->extent();
        createStr += QString("INSERT OR REPLACE INTO virts_geometry_columns_statistics (virt_name, virt_geometry, last_verified, row_count, extent_min_x, extent_min_y, extent_max_x, extent_max_y) "
                             "VALUES ('vtab%1', 'geometry', datetime('now'), %2, %3, %4, %5, %6);")
            .arg(layer_idx)
            .arg(vlayer->featureCount())
            .arg(extent.xMinimum())
            .arg(extent.yMinimum())
            .arg(extent.xMaximum())
            .arg(extent.yMaximum());

        char *errMsg;
        int r = sqlite3_exec( mSqlite.data(), createStr.toUtf8().constData(), NULL, NULL, &errMsg );
        if (r) {
            mValid = false;
            mError = QgsError( errMsg, VIRTUAL_LAYER_KEY );
            std::cout << "err: " << errMsg << std::endl;
            return;
        }
    }

    QgsDataSourceURI source;
    source.setDatabase( mPath );
    source.setDataSource( "", "vtab1", "geometry" );
    std::cout << "Spatialite uri: " << source.uri().toUtf8().constData() << std::endl;
    mSpatialite = new QgsSpatiaLiteProvider( source.uri() );
    mValid = mSpatialite->isValid();
}

QgsVirtualLayerProvider::~QgsVirtualLayerProvider()
{
    if (mSpatialite) {
        delete mSpatialite;
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
    return "";
}

bool QgsVirtualLayerProvider::setSubsetString( QString theSQL, bool updateFeatureCount )
{
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
    return mSpatialite->fields();
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
    return SelectAtId | SelectGeometryAtId;
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
}
