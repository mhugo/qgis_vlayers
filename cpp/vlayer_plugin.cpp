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

#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgsmaplayerregistry.h>
#include <qgsproviderregistry.h>
#include <qgsmessagebar.h>

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
    std::cout << "vlayer_plugin" << std::endl;
}

void VLayerPlugin::initGui()
{
    std::cout << "initGui" << std::endl;
    action_ = new QAction( QIcon( ":/vlayer/vlayer.png" ), tr( "Create a virtual layer" ), this );

    //    iface_->newLayerMenu()->addAction( action_ );
    iface_->layerToolBar()->addAction( action_ );

    connect( action_, SIGNAL( triggered() ), this, SLOT( run() ) );
}

void VLayerPlugin::run()
{
    std::cout << "run" << std::endl;

    QDialog* ss = dynamic_cast<QDialog*>(QgsProviderRegistry::instance()->selectWidget( "virtual", iface_->mainWindow() ));

    connect( ss, SIGNAL( addVectorLayer( QString const &, QString const &, QString const & ) ),
             this, SLOT( addVectorLayer( QString const &, QString const &, QString const & ) ) );
    ss->exec();
    delete ss;
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

void VLayerPlugin::unload()
{
    if (action_) {
        iface_->removePluginMenu( "&Virtual layer", action_ );
        delete action_;
    }
    std::cout << "VLayer unload" << std::endl;
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
