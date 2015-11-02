/***************************************************************************
                   qgsvirtuallayerfeatureiterator.cpp
            Feature iterator for the virtual layer provider
begin                : Feb 2015
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

#include <qgsvirtuallayerfeatureiterator.h>
#include <qgsmessagelog.h>
#include "vlayer_module.h"

static QString quotedColumn( QString name )
{
    return "\"" + name.replace("\"", "\"\"") + "\"";
}

QgsVirtualLayerFeatureIterator::QgsVirtualLayerFeatureIterator( QgsVirtualLayerFeatureSource* source, bool ownSource, const QgsFeatureRequest& request )
    : QgsAbstractFeatureIteratorFromSource<QgsVirtualLayerFeatureSource>( source, ownSource, request )
{
    try {
        mPath = mSource->provider()->mPath;
        mSqlite = Sqlite::open( mPath );
        mDefinition = mSource->provider()->mDefinition;

        QString tableName = mSource->provider()->mTableName;

        QStringList wheres;
        QString subset = mSource->provider()->mSubset;
        if ( !subset.isNull() ) {
            wheres << subset;
        }

        if ( !mDefinition.geometryField().isNull() && mDefinition.geometryField() != "*no*" && request.filterType() == QgsFeatureRequest::FilterRect ) {
            bool do_exact = request.flags() & QgsFeatureRequest::ExactIntersect;
            QgsRectangle rect( request.filterRect() );
            QString mbr = QString("%1,%2,%3,%4").arg(rect.xMinimum()).arg(rect.yMinimum()).arg(rect.xMaximum()).arg(rect.yMaximum());
            wheres <<  QString("%1Intersects(%2,BuildMbr(%3))")
                .arg(do_exact ? "Mbr" : "")
                .arg(quotedColumn(mDefinition.geometryField()))
                .arg(mbr);
        }
        else if (!mDefinition.uid().isNull() && request.filterType() == QgsFeatureRequest::FilterFid ) {
            wheres << QString("%1=%2")
                .arg(quotedColumn(mDefinition.uid()))
                .arg(request.filterFid());
        }
        else if (!mDefinition.uid().isNull() && request.filterType() == QgsFeatureRequest::FilterFids ) {
            QString values = quotedColumn(mDefinition.uid()) + " IN (";
            bool first = true;
            foreach ( auto& v, request.filterFids() ) {
                if (!first) {
                    values += ",";
                }
                first = false;
                values += QString::number(v);
            }
            values += ")";
            wheres << values;
        }

        mFields = mSource->provider()->fields();
        if ( request.flags() & QgsFeatureRequest::SubsetOfAttributes ) {

            // copy only selected fields
            foreach ( int idx, request.subsetOfAttributes() ) {
                mAttributes << idx;
            }
        }
        else {
            mAttributes = mFields.allAttributesList();
        }

        QString columns;
        {
            // the first column is always the uid (or 0)
            if ( !mDefinition.uid().isNull() ) {
                columns = quotedColumn(mDefinition.uid());
            }
            else {
                columns = "0";
            }
            foreach( int i, mAttributes ) {
                columns += ",";
                QString cname = mFields.at(i).name().toLower();
                columns += quotedColumn(cname);
            }
        }
        // the last column is the geometry, if any
        if ( !(request.flags() & QgsFeatureRequest::NoGeometry) && !mDefinition.geometryField().isNull() && mDefinition.geometryField() != "*no*" ) {
            columns += "," + quotedColumn(mDefinition.geometryField());
        }

        mSqlQuery = "SELECT " + columns + " FROM " + tableName;
        if ( !wheres.isEmpty() ) {
            mSqlQuery += " WHERE " + wheres.join(" AND ");
        }

        mQuery.reset( new Sqlite::Query( mSqlite.get(), mSqlQuery ) );

        mFid = 0;
    }
    catch (std::runtime_error& e)
    {
        QgsMessageLog::logMessage( e.what(), QObject::tr( "VLayer" ) );
        close();
    }
}

QgsVirtualLayerFeatureIterator::~QgsVirtualLayerFeatureIterator()
{
    close();
}

bool QgsVirtualLayerFeatureIterator::rewind()
{
    if (mClosed) {
        return false;
    }

    mQuery->reset();

    return true;
}

bool QgsVirtualLayerFeatureIterator::close()
{
    if (mClosed) {
        return false;
    }

    // this call is absolutely needed
    iteratorClosed();

    mClosed = true;
    return true;
}

bool QgsVirtualLayerFeatureIterator::fetchFeature( QgsFeature& feature )
{
    if (mClosed) {
        return false;
    }
    if (mQuery->step() != SQLITE_ROW) {
        return false;
    }

#if VERSION_INT <= 20900
    feature.setFields( &mFields, /* init */ true );
#else
    feature.setFields( mFields, /* init */ true );
#endif

    if ( mDefinition.uid().isNull() ) {
        // no id column => autoincrement
        feature.setFeatureId( mFid++ );
    }
    else {
        // first column: uid
        feature.setFeatureId( mQuery->column_int64( 0 ) );
    }

    int n = mQuery->column_count();
    int i = 0;
    foreach( int idx, mAttributes ) {
        const QgsField& f = mFields.at(idx);
        if ( f.type() == QVariant::Int ) {
            feature.setAttribute( idx, mQuery->column_int(i+1) );
        }
        else if ( f.type() == QVariant::LongLong ) {
            feature.setAttribute( idx, mQuery->column_int64(i+1) );
        }
        else if ( f.type() == QVariant::Double ) {
            feature.setAttribute( idx, mQuery->column_double(i+1) );
        }
        else if ( f.type() == QVariant::String ) {
            feature.setAttribute( idx, mQuery->column_text(i+1) );
        }
        i++;
    }
    if (n > mAttributes.size()+1) {
        // geometry field
        QByteArray blob( mQuery->column_blob(n-1) );
        if ( blob.size() > 0 ) {
            std::unique_ptr<QgsGeometry> geom( spatialite_blob_to_qgsgeometry( (const unsigned char*)blob.constData(), blob.size() ) );
            feature.setGeometry( geom.release() );
        }
    }

    return true;
}

QgsVirtualLayerFeatureSource::QgsVirtualLayerFeatureSource( const QgsVirtualLayerProvider* p ) :
    mProvider(p)
{
}

QgsVirtualLayerFeatureSource::~QgsVirtualLayerFeatureSource()
{
}

QgsFeatureIterator QgsVirtualLayerFeatureSource::getFeatures( const QgsFeatureRequest& request )
{
  return QgsFeatureIterator( new QgsVirtualLayerFeatureIterator( this, false, request ) );
}
