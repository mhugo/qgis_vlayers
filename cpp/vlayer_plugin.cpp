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
#include <QCoreApplication>
#include <QMessageBox>

#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsmaplayerregistry.h>
#include <qgsproviderregistry.h>
#include <qgsmessagebar.h>
#include <qgsrendererv2.h>
#include <qgslayertreeview.h>
#include <qgsproject.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>

#include "qgsvirtuallayersourceselect.h"

#include <unistd.h>
#include <iostream>

static const QString sName = "Virtual layer plugin";
static const QString sDescription = "This is the plugin companion for the virtual layer provider";
static const QString sCategory = "Plugins";
static const QgisPlugin::PLUGINTYPE sType = QgisPlugin::UI;
static const QString sVersion = "Version 0.1";
static const QString sIcon = ":/vlayer/vlayer.svg";
static const QString sExperimental = "true";

VLayerPlugin::VLayerPlugin( QgisInterface *iface ) :
    QgisPlugin( sName, sDescription, sCategory, sVersion, sType ),
    iface_(iface),
    createAction_(0)
{
}

void VLayerPlugin::initGui()
{
    createAction_ = new QAction( QIcon( ":/vlayer/vlayer.svg" ), tr( "New virtual layer" ), this );
    convertAction_ = new QAction( QIcon( ":/vlayer/vlayer.svg" ), tr( "Convert to virtual layer" ), this );

    iface_->newLayerMenu()->addAction( createAction_ );
    iface_->layerToolBar()->addAction( convertAction_ );

    connect( createAction_, SIGNAL( triggered() ), this, SLOT( onCreate() ) );
    connect( convertAction_, SIGNAL( triggered() ), this, SLOT( onConvert() ) );

    // override context menu
    iface_->layerTreeView()->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( iface_->layerTreeView(), SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( onContextMenu( const QPoint& ) ) );
    origMenuProvider_ = iface_->layerTreeView()->menuProvider();

    // override entry in layer menu
    QMenu* layerMenu = iface_->layerMenu();
    for ( auto action: layerMenu->actions() ) {
        if (action->objectName() == "mActionLayerSubsetString" ) {
            origLayerMenuAction_ = action;
            QAction* myAction = new QAction( action->text(), layerMenu );
            myAction->setShortcut( action->shortcut() );
            connect( myAction, SIGNAL(triggered()), this, SLOT(onLayerFilterFromMenu()) );
            layerMenu->insertAction( action, myAction );
            action->setVisible( false );
            break;
        }
    }
}

void VLayerPlugin::onContextMenu( const QPoint& pos )
{
    QgsLayerTreeView* v = iface_->layerTreeView();

    // from qgsapplayertreeviewmenuprovider ...
    QModelIndex idx = v->indexAt( pos );
    if ( !idx.isValid() )
        v->setCurrentIndex( QModelIndex() );

    // call the original context menu
    // and add a new custom action in place of the old one
    QMenu* menu = origMenuProvider_->createContextMenu();
    QString actionText = QCoreApplication::translate("QgsAppLayerTreeViewMenuProvider", "&Filter...");

    QAction* myAction = new QAction(actionText, menu);
    connect( myAction, SIGNAL(triggered()), this, SLOT(onLayerFilterFromContextMenu()) );

    for ( auto action : menu->actions() ) {
        // look for the action by its text (!)
        if ( action->text() == actionText ) {
            origFilterAction_ = action;
            // insert the replacement action
            menu->insertAction( action, myAction );
            // make the old one invisible (do not remove it, we still need it)
            origFilterAction_->setVisible(false);
            break;
        }
    }
    menu->exec( v->mapToGlobal( pos ) );
    delete menu;
}

void VLayerPlugin::onLayerFilterFromMenu()
{
    onLayerFilter();

    // forward to the original action
    origLayerMenuAction_->trigger();
}

void VLayerPlugin::onLayerFilterFromContextMenu()
{
    onLayerFilter();
    
    // forward to the original action
    origFilterAction_->trigger();
}

void VLayerPlugin::onLayerFilter()
{
    QgsMapLayer* lyr = iface_->layerTreeView()->currentLayer();
    if ( lyr == 0 ) {
        return;
    }
    if ( lyr->type() == QgsMapLayer::VectorLayer ) {
        QgsVectorLayer* layer = static_cast<QgsVectorLayer*>(lyr);
        if ( layer->vectorJoins().size() > 0 ) {
            if ( QMessageBox::question( NULL, tr( "Filter on joined fields" ),
                                        tr( "You are about to set a subset filter on a layer that has joined fields. "
                                            "Joined fields cannot be filtered, unless you convert the layer to a virtual layer first. "
                                            "Would you like to create a virtual layer out of this layer first ?" ),
                                        QMessageBox::Yes | QMessageBox::No ) == QMessageBox::Yes ) {
                // do it
                QgsVectorLayer* new_lyr = createVirtualLayer( layer );
                if (new_lyr) {
                    // set it as the active layer
                    iface_->layerTreeView()->setCurrentLayer( new_lyr );
                }
            }
        }
    }
}

void VLayerPlugin::unload()
{
    // restore context menu policy
    iface_->layerTreeView()->setContextMenuPolicy( Qt::DefaultContextMenu );

    if (createAction_) {
        iface_->layerToolBar()->removeAction( createAction_ );
        delete createAction_;
    }
}

void VLayerPlugin::onCreate()
{
    creationDialog( QList<QgsVectorLayer*>() );
}

typedef QDialog * creationFunction_t( QWidget * parent, Qt::WindowFlags fl, QList<QPair<QString,QString> > );

void VLayerPlugin::creationDialog( QList<QgsVectorLayer*> sourceLayers )
{
    QList<QPair<QString, QString> > parameters;
    foreach ( QgsVectorLayer* l, sourceLayers ) {
        parameters.append( qMakePair( QString("name"), l->name() ) );
        parameters.append( qMakePair( QString("source"), l->source() ) );
        parameters.append( qMakePair( QString("provider"), l->providerType() ) );
    }

    creationFunction_t* fun = (creationFunction_t*)(QgsProviderRegistry::instance()->function( "virtual", "createWidget" ));
    QScopedPointer<QDialog> ss( fun( iface_->mainWindow(), QgisGui::ModalDialogFlags, parameters ) );
    if (!ss) {
        return;
    }

    connect( ss.data(), SIGNAL( addVectorLayer( QString const &, QString const &, QString const & ) ),
             this, SLOT( addVectorLayer( QString const &, QString const &, QString const & ) ) );
    ss->exec();
}

void copy_layer_symbology( const QgsVectorLayer* source, QgsVectorLayer* dest )
{
    QDomImplementation DomImplementation;
    QDomDocumentType documentType =
      DomImplementation.createDocumentType(
        "qgis", "http://mrcc.com/qgis.dtd", "SYSTEM" );
    QDomDocument doc( documentType );
    QDomElement rootNode = doc.createElement( "qgis" );
    rootNode.setAttribute( "version", QString( "%1" ).arg( QGis::QGIS_VERSION ) );
    doc.appendChild( rootNode );
    QString errorMsg;
    source->writeSymbology( rootNode, doc, errorMsg );
    dest->readSymbology( rootNode, errorMsg );
}

QgsVectorLayer* VLayerPlugin::createVirtualLayer( QgsVectorLayer* vl )
{
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
                return 0;
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
        return 0;
    }

    // copy symbology
    copy_layer_symbology( vl, new_vl.data() );

    // disable the old layer
    QgsLayerTreeLayer* vl_in_tree = QgsProject::instance()->layerTreeRoot()->findLayer( vl->id() );
    if ( vl_in_tree ) {
        vl_in_tree->setVisible( Qt::Unchecked );
    }

    return static_cast<QgsVectorLayer*>(QgsMapLayerRegistry::instance()->addMapLayer(new_vl.take()));
}

void VLayerPlugin::onConvert()
{
    QList<QgsVectorLayer*> selection;
    foreach( QgsMapLayer *l, iface_->layerTreeView()->selectedLayers() ) {
        if ( l && l->type() == QgsMapLayer::VectorLayer ) {
            selection.append( static_cast<QgsVectorLayer*>(l) );
        }
    }
    creationDialog( selection );
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
