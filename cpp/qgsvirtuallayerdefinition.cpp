#include <QUrl>
#include <QRegExp>

#include <iostream>

#include "qgsvirtuallayerdefinition.h"

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

QgsVirtualLayerDefinition::QgsVirtualLayerDefinition( const QUrl& url )
{
    fromUrl( url );
}

void QgsVirtualLayerDefinition::fromUrl( const QUrl& url )
{
    mUri = url.path();

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
            // add the layer to the list
            mSourceLayers << SourceLayer(vlayer_name, layer_id);
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

                mSourceLayers << SourceLayer(vlayer_name, source, providerKey);
            }
        }
        else if ( key == "geometry" ) {
            // geometry field definition, optional
            // geometry_column(:wkb_type:srid)?
            QRegExp reGeom( "(\\w+)(?::([a-zA-Z0-9]+):(\\d+))?" );
            int pos = reGeom.indexIn( value );
            if ( pos >= 0 ) {
                mGeometryField = reGeom.cap(1);
                if ( reGeom.captureCount() > 1 ) {
                    // not used by the spatialite provider for now ...
                    mGeometryWkbType = geometry_type_to_wkb_type( reGeom.cap(2) );
                    if (mGeometryWkbType == 0) {
                        mGeometryWkbType = reGeom.cap(2).toLong();
                    }
                    mGeometrySrid = reGeom.cap(3).toLong();
                }
            }
        }
        else if ( key == "nogeometry" ) {
            std::cout << "nogeometry!!!" << std::endl;
            mGeometryField = "*no*";
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
                mOverridenFields.append( QgsField( reGeom.cap(1), type, reGeom.cap(2) ) );
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
}
