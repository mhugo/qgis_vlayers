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
#include <QSettings>
#include <QFileDialog>

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

#define VLAYER_CHAR_ESCAPING ":%=&?"

VLayerPlugin::VLayerPlugin( QgisInterface *iface ) :
    QgisPlugin( sName, sDescription, sCategory, sVersion, sType ),
    iface_(iface),
    createAction_(0)
{
}

void VLayerPlugin::initGui()
{
    createAction_ = new QAction( QIcon( ":/vlayer/vlayer_new.svg" ), tr( "New virtual layer" ), this );
    addAction_ = new QAction( QIcon( ":/vlayer/vlayer_add.svg" ), tr( "Add a virtual layer" ), this );
    connect( addAction_, SIGNAL(triggered()), this, SLOT(onAddLayer()) );

    settingsAction_ = new QAction(tr( "Virtual layer settings" ), this);
    connect( settingsAction_, SIGNAL(triggered()), this, SLOT(onLayerSettings()) );

    iface_->newLayerMenu()->addAction( createAction_ );
    iface_->addLayerMenu()->addAction( addAction_ );
    iface_->layerMenu()->addAction( settingsAction_ );

    iface_->layerToolBar()->addAction( createAction_ );
    iface_->layerToolBar()->addAction( addAction_ );

    connect( createAction_, SIGNAL( triggered() ), this, SLOT( onCreate() ) );

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

            // add the "create virtual layer" action
            menu->insertAction( action, createAction_ );

            // add the "virtual layer settings" action
            if ( iface_->activeLayer() &&
                 iface_->activeLayer()->type() == QgsMapLayer::VectorLayer &&
                 static_cast<QgsVectorLayer*>(iface_->activeLayer())->providerType() == "virtual" ) {
                menu->insertAction( action, settingsAction_ );
            }
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
                duplicateLayerToVirtual( layer );
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
    QList<QgsVectorLayer*> selection;
    foreach( QgsMapLayer *l, iface_->layerTreeView()->selectedLayers() ) {
        if ( l && l->type() == QgsMapLayer::VectorLayer ) {
            selection.append( static_cast<QgsVectorLayer*>(l) );
        }
    }
    // prepare parameters from selection
    ParameterPairs params( createVirtualLayer( selection ) );
    // show the dialog
    creationDialog( params );
}

void VLayerPlugin::onLayerSettings()
{
    ParameterPairs params;
    if ( iface_->activeLayer() &&
         iface_->activeLayer()->type() == QgsMapLayer::VectorLayer &&
         static_cast<QgsVectorLayer*>(iface_->activeLayer())->providerType() == "virtual" ) {
        QString source = iface_->activeLayer()->source();
        QUrl url( QUrl::fromEncoded( source.toLocal8Bit() ) );
        if (url.path().isEmpty()) {
            // temporary layer
            params.append( qMakePair(QString("fromUrl"), source) );
            creationDialog( params, /*replaceMode*/ true );
        }
        else {
            // layer on disk
            params.append( qMakePair(QString("fromFile"), url.path()) );
            creationDialog( params, /*replaceMode*/ true );
        }
    }
}

typedef QDialog * creationFunction_t( QWidget * parent, Qt::WindowFlags fl, const VLayerPlugin::ParameterPairs& );

void VLayerPlugin::creationDialog( const ParameterPairs& params, bool replaceMode )
{
    creationFunction_t* fun = (creationFunction_t*)(QgsProviderRegistry::instance()->function( "virtual", "createWidget" ));
    QScopedPointer<QDialog> ss( fun( iface_->mainWindow(), QgisGui::ModalDialogFlags, params ) );
    if (!ss) {
        return;
    }

    mReplaceMode = replaceMode;
    connect( ss.data(), SIGNAL( addVectorLayer( QString const &, QString const &, QString const & ) ),
                 this, SLOT( addVectorLayer( QString const &, QString const &, QString const & ) ) );
    ss->exec();
}

void VLayerPlugin::onAddLayer()
{
    QSettings settings;
    QString lastUsedDir = settings.value( "/UI/lastVirtualLayerDir", "." ).toString();
    QString filename = QFileDialog::getOpenFileName( 0, tr( "Open a virtual layer" ),
                                                     lastUsedDir,
                                                     tr( "Virtual layer" ) + " (*.qgl *.sqlite)" );
    if ( filename.isEmpty() ) {
        return;
    }
    QFileInfo info( filename );
    settings.setValue( "/UI/lastVirtualLayerDir", info.path() );

    // add the new layer
    QScopedPointer<QgsVectorLayer> l( new QgsVectorLayer( filename, info.baseName(), "virtual" ) );
    QgsMapLayerRegistry::instance()->addMapLayer( l.take() );
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

VLayerPlugin::ParameterPairs VLayerPlugin::createVirtualLayer( const QList<QgsVectorLayer*>& layers )
{
    ParameterPairs params;

    QList<const QgsVectorLayer*> layersInJoins;

    // first pass on layers with joins
    foreach( const QgsVectorLayer* vl, layers ) {
        if ( vl->vectorJoins().isEmpty() ) {
            // skip non joined layers for now
            continue;
        }
        // else mark it has joined
        layersInJoins.append( vl );

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
                                                   tr( "Unable to find the joined layer %1" ).arg(join.joinLayerId),
                                                   QgsMessageBar::CRITICAL );
                return ParameterPairs();
            }
            QgsVectorLayer* joined_layer = static_cast<QgsVectorLayer*>(l);
            // mark it
            layersInJoins.append( joined_layer );

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
            params.append( qMakePair( QString("layer"), join_name ) );
            params.append( qMakePair( QString("source"), QString(QUrl::toPercentEncoding(joined_layer->source(), VLAYER_CHAR_ESCAPING)) ) );
            params.append( qMakePair( QString("provider"), joined_layer->providerType() ) );
        }

        // find uid column if any
        QString pk_field;
        const QgsFields& fields = vl->dataProvider()->fields();
        {
            auto pk = const_cast<QgsVectorLayer*>(vl)->dataProvider()->pkAttributeIndexes();
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

        params.append( qMakePair( QString("query"), "SELECT " + columns.join(",") + " FROM t" + left_joins ) );
        params.append( qMakePair( QString("uid"), pk_field ) );
        params.append( qMakePair( QString("geometry"), QString("geometry") ) );
        QString source = QUrl::toPercentEncoding(vl->source(), "", VLAYER_CHAR_ESCAPING);
        params.append( qMakePair( QString("layer"), QString("t") ) );
        params.append( qMakePair( QString("source"), source ) );
        params.append( qMakePair( QString("provider"), vl->providerType() ) );
    }

    // second pass on layers without joins
    foreach( const QgsVectorLayer* vl, layers ) {
        if ( layersInJoins.contains( vl ) ) {
            // skip if it has already been added
            continue;
        }
        QString source = QUrl::toPercentEncoding(vl->source(), "", VLAYER_CHAR_ESCAPING);
        params.append( qMakePair( QString("layer"), vl->name() ) );
        params.append( qMakePair( QString("source"), source ) );
        params.append( qMakePair( QString("provider"), vl->providerType() ) );
    }

    return params;
}


void VLayerPlugin::duplicateLayerToVirtual( QgsVectorLayer* vl )
{
    QList<QgsVectorLayer*> layers;
    layers << vl;
    ParameterPairs params( createVirtualLayer( layers ) );
    QUrl url;
    for ( auto& p : params ) {
        url.addQueryItem( p.first, p.second );
    }

    QScopedPointer<QgsVectorLayer> new_vl( new QgsVectorLayer( url.toString(), vl->name() + tr( " (virtual)" ), "virtual" ) );
    if ( !new_vl || !new_vl->isValid() ) {
        if ( new_vl ) {
            iface_->messageBar()->pushMessage( tr( "Layer is not valid" ), new_vl->dataProvider()->error().message(), QgsMessageBar::CRITICAL );
        }
        return;
    }

    // copy symbology
    copy_layer_symbology( vl, new_vl.data() );

    // disable the old layer
    QgsLayerTreeLayer* vl_in_tree = QgsProject::instance()->layerTreeRoot()->findLayer( vl->id() );
    if ( vl_in_tree ) {
        vl_in_tree->setVisible( Qt::Unchecked );
    }
    QgsVectorLayer* nl = static_cast<QgsVectorLayer*>(QgsMapLayerRegistry::instance()->addMapLayer(new_vl.take()));
    iface_->layerTreeView()->setCurrentLayer( nl );
}

void VLayerPlugin::addVectorLayer( const QString& source, const QString& name, const QString& provider )
{
    QgsVectorLayer* l = new QgsVectorLayer( source, name, provider, false );
    if ( l && l->isValid() ) {
        if (!mReplaceMode) {
            QgsMapLayerRegistry::instance()->addMapLayer( l );
        }
        else {
            QgsMapLayerRegistry::instance()->addMapLayer( l, /*addToLegend*/ false, /*takeOwnership*/ false );
            // replace the current layer
            QgsVectorLayer* current = static_cast<QgsVectorLayer*>(iface_->activeLayer());
            // copy symbology
            copy_layer_symbology( current, l );
            // look for the current layer
            QgsLayerTreeLayer* in_tree = QgsProject::instance()->layerTreeRoot()->findLayer( current->id() );
            int idx = 0;
            for ( auto& vl : in_tree->parent()->children() ) {
                if ( vl->nodeType() == QgsLayerTreeNode::NodeLayer && static_cast<QgsLayerTreeLayer*>(vl)->layer() == current ) {
                    break;
                }
                idx++;
            }
            // insert the new layer
            QgsLayerTreeGroup* parent = static_cast<QgsLayerTreeGroup*>(in_tree->parent()) ? static_cast<QgsLayerTreeGroup*>(in_tree->parent()) : QgsProject::instance()->layerTreeRoot();
            parent->insertLayer( idx, l );
            // remove the current layer
            parent->removeLayer( current );
        }
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
