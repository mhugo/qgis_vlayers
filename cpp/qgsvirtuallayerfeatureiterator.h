#ifndef QGSVIRTUALLAYER_FEATURE_SOURCE_H
#define QGSVIRTUALLAYER_FEATURE_SOURCE_H


#include <qgsvirtuallayerprovider.h>

class QgsVirtualLayerFeatureSource : public QgsAbstractFeatureSource
{
public:
    QgsVirtualLayerFeatureSource( const QgsVirtualLayerProvider* p );
    ~QgsVirtualLayerFeatureSource();

    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest& request ) override;

    const QgsVirtualLayerProvider* provider() const { return mProvider; }
private:
    const QgsVirtualLayerProvider* mProvider;
};

class QgsVirtualLayerFeatureIterator : public QgsAbstractFeatureIteratorFromSource<QgsVirtualLayerFeatureSource>
{
  public:
    QgsVirtualLayerFeatureIterator( QgsVirtualLayerFeatureSource* source, bool ownSource, const QgsFeatureRequest& request );
    ~QgsVirtualLayerFeatureIterator();

    //! reset the iterator to the starting position
    virtual bool rewind() override;

    //! end of iterating: free the resources / lock
    virtual bool close() override;

  protected:

    //! fetch next feature, return true on success
    virtual bool fetchFeature( QgsFeature& feature ) override;

    QScopedPointer<Sqlite::Query> mQuery;

    QgsFeatureId mFid;

    QString mPath;
    QgsScopedSqlite mSqlite;
    QgsVirtualLayerDefinition mDefinition;
    QgsFields mFields;

    QString mSqlQuery;

    // map query column index to feature attribute index
    // -1 for the geometry column
    QMap<int, int> mColumnMap;
};

#endif
