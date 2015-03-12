#include <qgsvirtuallayerfeatureiterator.h>
#include "vlayer_module.h"

QgsVirtualLayerFeatureIterator::QgsVirtualLayerFeatureIterator( QgsVirtualLayerFeatureSource* source, bool ownSource, const QgsFeatureRequest& request )
    : QgsAbstractFeatureIteratorFromSource<QgsVirtualLayerFeatureSource>( source, ownSource, request )
{
    mPath = mSource->provider()->mPath;
    mSqlite = Sqlite::open( mPath );
    mDefinition = mSource->provider()->mDefinition;
    mFields = mSource->provider()->fields();

    // base query
    mSqlQuery = mDefinition.query();

    if ( !mDefinition.geometryField().isNull() && request.filterType() == QgsFeatureRequest::FilterRect ) {
        bool do_exact = request.flags() & QgsFeatureRequest::ExactIntersect;
        QgsRectangle rect( request.filterRect() );
        QString mbr = QString("%1,%2,%3,%4").arg(rect.xMinimum()).arg(rect.yMinimum()).arg(rect.xMaximum()).arg(rect.yMaximum());
        mSqlQuery = QString("SELECT * FROM (%1) WHERE %2Intersects(%3,BuildMbr(%4))")
            .arg(mSqlQuery)
            .arg(do_exact ? "Mbr" : "")
            .arg(mDefinition.geometryField())
            .arg(mbr);
    }

    mQuery.reset( new Sqlite::Query( mSqlite.get(), mSqlQuery ) );

    int n = mQuery->column_count();
    int j = 0;
    for ( int i = 0; i < n; i++ ) {
        if ( mQuery->column_name(i) == mDefinition.geometryField() ) {
            mColumnMap[i] = -1;
        }
        else {
            mColumnMap[i] = j++;
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

    //feature.setFields
    feature.setFields( &mFields, /* init */ true );
    //feature.setFeatureId
    feature.setFeatureId( mFid++ );
    //feature.setAttribute
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
