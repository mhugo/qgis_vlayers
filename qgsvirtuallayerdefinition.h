/***************************************************************************
                qgsvirtuallayerdefinition.h
begin                : Feb, 2015
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

#ifndef QGSVIRTUALLAYERDEFINITION_H
#define QGSVIRTUALLAYERDEFINITION_H

#include <qgsfield.h>
#include <qgis.h>

class QgsVirtualLayerDefinition
{
 public:
    class SourceLayer
    {
    public:
        SourceLayer( const QString& name, const QString& ref ) : mName(name), mRef(ref) {}
        SourceLayer( const QString& name, const QString& source, const QString& provider ) : mName(name), mSource(source), mProvider(provider) {}

        bool isReferenced() const { return !mRef.isEmpty(); }

        QString reference() const { return mRef; }
        QString name() const { return mName; }
        QString provider() const { return mProvider; }
        QString source() const { return mSource; }
    private:
        QString mName;
        QString mSource;
        QString mProvider;
        QString mRef;
    };

    QgsVirtualLayerDefinition( const QString& uri = "" ) : mUri(uri) {}
    QgsVirtualLayerDefinition( const QUrl& );

    void fromUrl( const QUrl& );

    void addSource( const QString& name, const QString ref )
    {
        mSourceLayers.append( SourceLayer(name, ref) );
    }
    void addSource( const QString& name, const QString source, const QString& provider )
    {
        mSourceLayers.append( SourceLayer(name, source, provider) );
    }

    const QList<SourceLayer>& sourceLayers() const { return mSourceLayers; }

    bool hasSourceLayer( QString name ) const;

    bool hasReferencedLayers() const;

    QString query() const { return mQuery; }
    void setQuery( const QString& query ) { mQuery = query; }

    QString uri() const { return mUri; }
    void setURI( const QString& uri ) { mUri = uri; }

    QString uid() const { return mUid; }
    void setUid( const QString& uid ) { mUid = uid; }

    QString geometryField() const { return mGeometryField; }
    void setGeometryField( const QString& geometryField ) { mGeometryField = geometryField; }

    QGis::WkbType geometryWkbType() const { return mGeometryWkbType; }
    void setGeometryWkbType( QGis::WkbType t ) { mGeometryWkbType = t; }

    long geometrySrid() const { return mGeometrySrid; }
    void setGeometrySrid( long srid ) { mGeometrySrid = srid; }

    QgsFields overridenFields() const { return mOverridenFields; }
    void setOverridenFields( const QgsFields& fields ) { mOverridenFields = fields; }

private:
    QList<SourceLayer> mSourceLayers;
    QString mQuery;
    QString mUid;
    QString mGeometryField;
    QString mUri;
    QgsFields mOverridenFields;
    QGis::WkbType mGeometryWkbType;
    long mGeometrySrid;
};

QGis::WkbType geometry_type_to_wkb_type( const QString& wkb_str );

QgsVirtualLayerDefinition virtualLayerDefinitionFromSqlite( const QString& path );

#endif
