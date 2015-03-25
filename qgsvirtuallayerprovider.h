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

#ifndef QGSVIRTUAL_LAYER_PROVIDER_H
#define QGSVIRTUAL_LAYER_PROVIDER_H

#include <QTemporaryFile>

#include <qgsvectordataprovider.h>

#include "qgsvirtuallayerdefinition.h"

#include "sqlite_helper.h"

class QgsVirtualLayerFeatureIterator;

class QgsVirtualLayerProvider: public QgsVectorDataProvider
{
  Q_OBJECT public:

    /**
     * Constructor of the vector provider
     * @param uri  uniform resource locator (URI) for a dataset
     */
    QgsVirtualLayerProvider( QString const &uri = "" );

    //! Destructor
    virtual ~ QgsVirtualLayerProvider();

    virtual QgsAbstractFeatureSource* featureSource() const override;

    /**
        *   Returns the permanent storage type for this layer as a friendly name.
        */
    virtual QString storageType() const override;

    /*! Get the QgsCoordinateReferenceSystem for this layer
     * @note Must be reimplemented by each provider.
     * If the provider isn't capable of returning
     * its projection an empty srs will be return, ti will return 0
     */
    virtual QgsCoordinateReferenceSystem crs() override;

    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest& request ) override;

    /** Accessor for sql where clause used to limit dataset */
    virtual QString subsetString() override;

    /** mutator for sql where clause used to limit dataset size */
    virtual bool setSubsetString( QString theSQL, bool updateFeatureCount = true ) override;

    virtual bool supportsSubsetString() override { return true; }

    /** Get the feature type. This corresponds to
     * WKBPoint,
     * WKBLineString,
     * WKBPolygon,
     * WKBMultiPoint,
     * WKBMultiLineString or
     * WKBMultiPolygon
     * as defined in qgis.h
     */
    QGis::WkbType geometryType() const override;

    /** return the number of layers for the current data source

    @note

    Should this be subLayerCount() instead?
    */
    size_t layerCount() const;

    /**
     * Get the number of features in the layer
     */
    long featureCount() const override;

    /** Return the extent for this data layer
    */
    virtual QgsRectangle extent() override;

    /** Update the extent for this data layer
    */
    virtual void updateExtents() override;

    /**
      * Get the field information for the layer
      * @return vector of QgsField objects
      */
    const QgsFields & fields() const override;

    /** Returns the minimum value of an attribute
     *  @param index the index of the attribute */
    QVariant minimumValue( int index ) override;

    /** Returns the maximum value of an attribute
     *  @param index the index of the attribute */
    QVariant maximumValue( int index ) override;

    /** Return the unique values of an attribute
     *  @param index the index of the attribute
     *  @param values reference to the list of unique values
     *  @param limit maximum number of values */
    virtual void uniqueValues( int index, QList < QVariant > &uniqueValues, int limit = -1 ) override;

    /**Returns true if layer is valid
    */
    bool isValid() override;

    /**Describes if provider has save and load style support
       @return true in case saving style to db is supported by this provider*/
    virtual bool isSaveAndLoadStyleToDBSupported() override { return false; }

    /**Returns a bitmask containing the supported capabilities*/
    int capabilities() const override;

    /** The SpatiaLite provider does its own transforms so we return
     * true for the following three functions to indicate that transforms
     * should not be handled by the QgsCoordinateTransform object. See the
     * documentation on QgsVectorDataProvider for details on these functions.
     */
    // XXX For now we have disabled native transforms in the SpatiaLite
    //   (following the PostgreSQL provider example)
    bool supportsNativeTransform()
    {
      return false;
    }

    /** return a provider name

    Essentially just returns the provider key.  Should be used to build file
    dialogs so that providers can be shown with their supported types. Thus
    if more than one provider supports a given format, the user is able to
    select a specific provider to open that file.

    @note

    Instead of being pure virtual, might be better to generalize this
    behavior and presume that none of the sub-classes are going to do
    anything strange with regards to their name or description?

    */
    QString name() const override;

    /** return description

    Return a terse string describing what the provider is.

    @note

    Instead of being pure virtual, might be better to generalize this
    behavior and presume that none of the sub-classes are going to do
    anything strange with regards to their name or description?

    */
    QString description() const override;

    /**
     * Return list of indexes of fields that make up the primary key
     */
    QgsAttributeList pkAttributeIndexes() override;

 private:

    // file on disk
    QString mPath;

    QgsScopedSqlite mSqlite;

    // underlying vector layers
    struct SourceLayer
    {
        SourceLayer(): layer(0) {}
        SourceLayer( QgsVectorLayer *l, const QString& n = "" ) : layer(l), name(n) {}
        SourceLayer( const QString& p, const QString& s, const QString& n ) : layer(0), name(n), source(s), provider(p) {}
        QgsVectorLayer* layer;
        QString name;
        QString source;
        QString provider;
    };
    struct SourceLayers : public QVector<SourceLayer>
    {
        SourceLayers() : QVector<SourceLayer>() {}
    };
    SourceLayers mLayers;

    bool mValid;

    // temporary file used for temporary virtual layer
    QScopedPointer<QTemporaryFile> mTempFile;

    QString mTableName;

    QgsFields mFields;

    QgsCoordinateReferenceSystem mCrs;

    // nonce used for temporary file
    static int mNonce;

    QgsVirtualLayerDefinition mDefinition;

    QString mSubset;

    void resetSqlite();

    mutable bool mCachedStatistics;
    mutable qint64 mFeatureCount;
    mutable QgsRectangle mExtent;

    void updateStatistics() const;

    bool openIt_();
    bool createIt_();
    bool loadSourceLayers_();

    friend class QgsVirtualLayerFeatureIterator;

private slots:
    void onLayerDeleted();
};

#endif
