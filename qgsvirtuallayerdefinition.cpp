#include <QUrl>
#include <QRegExp>
#include <QStringList>

#include <iostream>

#include "qgsvirtuallayerdefinition.h"
#include "common.h"

QGis::WkbType geometry_type_to_wkb_type( const QString& wkb_str )
{
    QString w = wkb_str.toLower();
    if ( w == "point" ) {
        return QGis::WKBPoint;
    }
    else if ( w == "linestring" ) {
        return QGis::WKBLineString;
    }
    else if ( w == "polygon" ) {
        return QGis::WKBPolygon;
    }
    else if ( w == "multipoint" ) {
        return QGis::WKBMultiPoint;
    }
    else if ( w == "multilinestring" ) {
        return QGis::WKBMultiLineString;
    }
    else if ( w == "multipolygon" ) {
        return QGis::WKBMultiPolygon;
    }
    return QGis::WKBUnknown;
}

QgsVirtualLayerDefinition::QgsVirtualLayerDefinition( const QUrl& url )
{
    fromUrl( url );
}

void QgsVirtualLayerDefinition::fromUrl( const QUrl& url )
{
    mUri = url.path();

    mGeometrySrid = -1;
    mGeometryWkbType = QGis::WKBNoGeometry;

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
                    if (mGeometryWkbType == QGis::WKBUnknown) {
                        mGeometryWkbType = QGis::WkbType(reGeom.cap(2).toLong());
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

QgsVirtualLayerDefinition virtualLayerDefinitionFromSqlite( const QString& path )
{
    QgsVirtualLayerDefinition def;
    QgsScopedSqlite sqlite = Sqlite::open( path );

    def.setURI( path );

    QgsFields forcedFields;
    {
        Sqlite::Query q( sqlite.get(), "SELECT name FROM sqlite_master WHERE name='_meta' OR name='_tables' OR name='_columns'" );
        int cnt = 0;
        while ( q.step() == SQLITE_ROW ) {
            cnt++;
        }
        if ( cnt != 3 ) {
            throw std::runtime_error( "No metadata tables !" );
        }
    }
    // look for the correct version
    {
        Sqlite::Query q( sqlite.get(), "SELECT version FROM _meta" );
        int version = 0;
        if (q.step() == SQLITE_ROW) {
            version = sqlite3_column_int( q.stmt(), 0 );
        }
        if (version != VIRTUAL_LAYER_VERSION) {
            throw std::runtime_error( "Bad virtual layer version !" );
        }
    }
    // look for a query, if any
    {
        Sqlite::Query q( sqlite.get(), "SELECT id, name, source, provider FROM _tables" );
        while ( q.step() == SQLITE_ROW ) {
            int id = sqlite3_column_int( q.stmt(), 0 );
            if ( id == 0 ) { // query
                def.setQuery( (const char*)sqlite3_column_text( q.stmt(), 2 ) );
                // name stores the UID field, if any
                def.setUid( (const char*)sqlite3_column_text( q.stmt(), 1 ) );
            }
            else {
                def.addSource( (const char*)sqlite3_column_text( q.stmt(), 1 ),
                                       (const char*)sqlite3_column_text( q.stmt(), 2 ),
                                       (const char*)sqlite3_column_text( q.stmt(), 3 ) );
            }
        }
    }
    // look for field overloaded types and geometry
    {
        Sqlite::Query q( sqlite.get(), "SELECT name, type FROM _columns WHERE table_id=0" );
        while ( q.step() == SQLITE_ROW ) {
            QString colname( (const char*)sqlite3_column_text( q.stmt(), 0 ) );
            QString coltype( (const char*)sqlite3_column_text( q.stmt(), 1 ) );
            if ( coltype.contains(':') ) {
                // geometry field
                if ( coltype.left(2) == "no" ) {
                    def.setGeometryField( "*no*" );
                    def.setGeometryWkbType( QGis::WKBNoGeometry );
                    def.setGeometrySrid(0);
                }
                else {
                    def.setGeometryField( colname );
                    QStringList sl = coltype.split(':');
                    def.setGeometryWkbType( QGis::WkbType(sl[0].toLong()) );
                    if ( sl.size() > 1 ) {
                        def.setGeometrySrid( sl[1].toLong() );
                    }
                }
            }
            else {
                QVariant::Type t = QVariant::nameToType( coltype.toLocal8Bit().constData() );
                forcedFields.append( QgsField( colname, t, coltype ) );
            }
        }
    }
    def.setOverridenFields( forcedFields );
    return def;
}

bool QgsVirtualLayerDefinition::hasSourceLayer( QString name ) const
{
    foreach ( const auto& l, mSourceLayers ) {
        if ( l.name() == name ) {
            return true;
        }
    }
    return false;
}

bool QgsVirtualLayerDefinition::hasReferencedLayers() const
{
    foreach ( const auto& l, mSourceLayers ) {
        if ( l.isReferenced() ) {
            return true;
        }
    }
    return false;
}
