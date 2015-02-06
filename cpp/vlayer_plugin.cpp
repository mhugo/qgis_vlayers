#include "vlayer_plugin.h"

#include <sqlite3.h>
#include <spatialite.h>
//#include <sqlite3ext.h>

#include <QIcon>
#include <QAction>
#include <QDialog>
#include <QDomNode>
#include <QMenu>
#include <QToolBar>
#include <QUrl>

#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsmaplayerregistry.h>
#include <qgsproviderregistry.h>
#include <qgsmessagebar.h>
#include <qgsrendererv2.h>

#include <unistd.h>
#include <iostream>

static const QString sName = "Virtual layer plugin";
static const QString sDescription = "This is the plugin companion for the virtual layer provider";
static const QString sCategory = "Plugins";
static const QgisPlugin::PLUGINTYPE sType = QgisPlugin::UI;
static const QString sVersion = "Version 0.1";
static const QString sIcon = ":/vlayer/vlayer.png";
static const QString sExperimental = "true";

VLayerPlugin::VLayerPlugin( QgisInterface *iface ) :
    QgisPlugin( sName, sDescription, sCategory, sVersion, sType ),
    iface_(iface),
    action_(0)
{
}

void VLayerPlugin::initGui()
{
    action_ = new QAction( QIcon( ":/vlayer/vlayer.png" ), tr( "New virtual layer" ), this );

    convertAction_ = new QAction( QIcon( ":/vlayer/vlayer.png" ), tr( "Convert to virtual layer" ), this );

    iface_->newLayerMenu()->addAction( action_ );

    iface_->layerToolBar()->addAction( convertAction_ );

    connect( action_, SIGNAL( triggered() ), this, SLOT( run() ) );
    connect( convertAction_, SIGNAL( triggered() ), this, SLOT( onConvert() ) );
}

void VLayerPlugin::unload()
{
    if (action_) {
        iface_->layerToolBar()->removeAction( action_ );
        delete action_;
    }
}

void VLayerPlugin::run()
{
    QDialog* ss = dynamic_cast<QDialog*>(QgsProviderRegistry::instance()->selectWidget( "virtual", iface_->mainWindow() ));

    connect( ss, SIGNAL( addVectorLayer( QString const &, QString const &, QString const & ) ),
             this, SLOT( addVectorLayer( QString const &, QString const &, QString const & ) ) );
    ss->exec();
    delete ss;
}

void VLayerPlugin::onConvert()
{
    QgsMapLayer* l = iface_->activeLayer();
    if ( l && l->type() == QgsMapLayer::VectorLayer ) {
        QgsVectorLayer* vl = static_cast<QgsVectorLayer*>(l);

        QUrl url;

        if ( !vl->vectorJoins().isEmpty() ) {
            QStringList columns, tables, wheres;
            columns << "t.*";
            int join_idx = 0;
            for ( auto& join : vl->vectorJoins() ) {
                // join name for the query
                QString join_name = QString("j%1").arg(++join_idx);

                // real layer name (to prefix joined field names)
                QgsMapLayer* l = QgsMapLayerRegistry::instance()->mapLayer( join.joinLayerId );
                if (!l || l->type() != QgsMapLayer::VectorLayer) {
                    // shoudl not happen
                    iface_->messageBar()->pushMessage( tr( "Layer not found" ),
                                                       tr( "Unable to found the joined layer %1" ).arg(join.joinLayerId),
                                                       QgsMessageBar::CRITICAL );
                    return;
                }
                QgsVectorLayer* joined_layer = static_cast<QgsVectorLayer*>(l);

                tables << join_name;

                // is there a subset of joined fields ?
                if ( join.joinFieldNamesSubset() ) {
                    for ( auto& f : *join.joinFieldNamesSubset() ) {
                        columns << join_name + "." + f + " AS " + joined_layer->name() + "_" + f;
                    }
                }
                else {
                    const QgsFields& joined_fields = joined_layer->dataProvider()->fields();
                    for ( int i = 0; i < joined_fields.count(); i++ ) {
                        const QgsField& f = joined_fields.field(i);
                        columns << join_name + "." + f.name() + " AS " + joined_layer->name() + "_" + f.name();
                    }
                }

                wheres << "t." + join.targetFieldName + "=" + join_name + "." + join.joinFieldName;

                // add reference to the joined layer
                url.addQueryItem( "layer_ref", join.joinLayerId + ":" + join_name );
            }

            // find uid column if any
            QString pk_field;
            const QgsFields& fields = vl->dataProvider()->fields();
            {
                auto pk = vl->dataProvider()->pkAttributeIndexes();
                if ( pk.size() == 1 ) {
                    pk_field = fields.field(pk[0]).name();
                }
                else {
                    // find an uid name
                    pk_field = "uid";
                    {
                        bool uid_exists;
                        do {
                            uid_exists = false;
                            for ( int i = 0; i < fields.count(); i++ ) {
                                if ( fields.field(i).name() == pk_field ) {
                                    pk_field += "_";
                                    uid_exists = true;
                                    break;
                                }
                            }
                        } while (uid_exists);
                    }
                    // add a column
                    columns << "t.rowid AS " + pk_field;
                }
            }

            // QGIS joins are pseudo left joins
            QString left_joins;
            for ( int i = 0; i < tables.size(); i++ ) {
                left_joins += " LEFT JOIN " + tables[i] + " ON " + wheres[i];
            }

            url.addQueryItem( "query", "SELECT " + columns.join(",") + " FROM t" + left_joins );
            url.addQueryItem( "uid", pk_field );
            url.addQueryItem( "geometry", "geometry");
        }

        QString source = QUrl::toPercentEncoding(vl->source(), "", ":%");
        url.addQueryItem( "layer", vl->providerType() + ":" + source + ":t" );

        QScopedPointer<QgsVectorLayer> new_vl( new QgsVectorLayer( url.toString(), vl->name() + tr( " (virtual)" ), "virtual" ) );
        if ( !new_vl || !new_vl->isValid() ) {
            if ( new_vl ) {
                iface_->messageBar()->pushMessage( tr( "Layer is not valid" ), new_vl->dataProvider()->error().message(), QgsMessageBar::CRITICAL );
            }
            return;
        }

        // copy symbology
        new_vl->setRendererV2( vl->rendererV2()->clone() );

        QgsMapLayerRegistry::instance()->addMapLayer(new_vl.take());
    }
}

void VLayerPlugin::addVectorLayer( const QString& source, const QString& name, const QString& provider )
{
    QgsVectorLayer* l = new QgsVectorLayer( source, name, provider, false );
    if ( l && l->isValid() ) {
        QgsMapLayerRegistry::instance()->addMapLayer( l );
    }
    else {
        QString msg = tr( "The layer %1 is not a valid layer and can not be added to the map" ).arg( source );
        iface_->messageBar()->pushMessage( tr( "Layer is not valid" ), msg, QgsMessageBar::CRITICAL );
        if (l) {
            iface_->messageBar()->pushMessage( tr( "Layer is not valid" ), l->dataProvider()->error().message(), QgsMessageBar::CRITICAL );
        }
        delete l;
    }
}

QGISEXTERN QgisPlugin * classFactory( QgisInterface * iface )
{
  return new VLayerPlugin( iface );
}
// Return the name of the plugin - note that we do not user class members as
// the class may not yet be insantiated when this method is called.
QGISEXTERN QString name()
{
    return sName;
}

// Return the description
QGISEXTERN QString description()
{
    return sDescription;
}

// Return the category
QGISEXTERN QString category()
{
    return sCategory;
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
    return sType;
}

// Return the version number for the plugin
QGISEXTERN QString version()
{
    return sVersion;
}

// Return the icon
QGISEXTERN QString icon()
{
    return sIcon;
}

// Return the experimental status for the plugin
QGISEXTERN QString experimental()
{
    return sExperimental;
}

// Delete ourself
QGISEXTERN void unload( QgisPlugin * thePluginPointer )
{
    delete thePluginPointer;
}
