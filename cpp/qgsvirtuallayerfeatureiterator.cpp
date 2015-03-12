#include <qgsvirtuallayerfeatureiterator.h>
#include "vlayer_module.h"

QgsVirtualLayerFeatureIterator::QgsVirtualLayerFeatureIterator( QgsVirtualLayerFeatureSource* source, bool ownSource, const QgsFeatureRequest& request )
    : QgsAbstractFeatureIteratorFromSource<QgsVirtualLayerFeatureSource>( source, ownSource, request )
{
    mPath = mSource->provider()->mPath;
    mSqlite = Sqlite::open( mPath );
    mDefinition = mSource->provider()->mDefinition;
    mFields = mSource->provider()->fields();

    mSqlQuery = mDefinition.query();

    QStringList wheres;
    QString subset = mSource->provider()->mSubset;
    if ( !subset.isNull() ) {
        wheres << subset;
    }

    if ( !mDefinition.geometryField().isNull() && request.filterType() == QgsFeatureRequest::FilterRect ) {
        bool do_exact = request.flags() & QgsFeatureRequest::ExactIntersect;
        QgsRectangle rect( request.filterRect() );
        QString mbr = QString("%1,%2,%3,%4").arg(rect.xMinimum()).arg(rect.yMinimum()).arg(rect.xMaximum()).arg(rect.yMaximum());
        wheres <<  QString("%1Intersects(%2,BuildMbr(%3))")
            .arg(do_exact ? "Mbr" : "")
            .arg(mDefinition.geometryField())
            .arg(mbr);
    }
    else if (!mDefinition.uid().isNull() && request.filterType() == QgsFeatureRequest::FilterFid ) {
        wheres << QString("%1=%2")
            .arg(mDefinition.uid())
            .arg(request.filterFid());
    }
    if ( !wheres.isEmpty() ) {
        mSqlQuery = QString("SELECT * FROM (%1) WHERE %2")
            .arg(mSqlQuery)
            .arg(wheres.join(" AND "));
    }

    mQuery.reset( new Sqlite::Query( mSqlite.get(), mSqlQuery ) );

    mUidColumn = -1;
    int n = mQuery->column_count();
    int j = 0;
    for ( int i = 0; i < n; i++ ) {
        QString colname = mQuery->column_name(i).toLower();
        if ( colname == mDefinition.geometryField().toLower() ) {
            mColumnMap[i] = -1;
        }
        else {
            mColumnMap[i] = j++;
            if (colname == mDefinition.uid().toLower()) {
                mUidColumn = i;
            }
        }
    }

    mFid = 0;
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

    feature.setFields( &mFields, /* init */ true );

    if ( mUidColumn == -1 ) {
        // no id column => autoincrement
        feature.setFeatureId( mFid++ );
    }
    else {
        feature.setFeatureId( mQuery->column_int64( mUidColumn ) );
    }

    int n = mQuery->column_count();
    for ( int i = 0; i < n; i++ ) {
        int j = mColumnMap[i];
        if ( j == -1 ) { 
            // geometry field
            QByteArray blob( mQuery->column_blob(i) );
            std::unique_ptr<QgsGeometry> geom( spatialite_blob_to_qgsgeometry( (const unsigned char*)blob.constData(), blob.size() ) );
            feature.setGeometry( geom.release() );
        }
        else {
            const QgsField& f = mFields.at(j);
            if ( f.type() == QVariant::Int ) {
                feature.setAttribute( j, mQuery->column_int(i) );
            }
            else if ( f.type() == QVariant::Double ) {
                feature.setAttribute( j, mQuery->column_double(i) );
            }
            else if ( f.type() == QVariant::String ) {
                feature.setAttribute( j, mQuery->column_text(i) );
            }
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
