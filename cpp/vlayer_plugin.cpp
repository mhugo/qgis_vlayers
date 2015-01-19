#include "vlayer_plugin.h"

#include <sqlite3.h>
#include <spatialite.h>
//#include <sqlite3ext.h>

#include <QIcon>
#include <QAction>
#include <QDomNode>

#include <qgsmaplayer.h>
#include <qgsvectorlayer.h>
#include <qgsmaplayerregistry.h>

#include <unistd.h>
#include <iostream>

static const QString sName = "Virtual layer plugin";
static const QString sDescription = "This is a POC virtual layer plugin";
static const QString sCategory = "Plugins";
static const QgisPlugin::PLUGINTYPE sType = QgisPlugin::UI;
static const QString sVersion = "Version 0.1";
static const QString sIcon = ":/vlayer/vlayer.png";
static const QString sExperimental = "true";

extern "C" {
    int qgsvlayer_module_init();
}


VLayerPlugin::VLayerPlugin( QgisInterface *iface ) :
    QgisPlugin( sName, sDescription, sCategory, sVersion, sType ),
    iface_(iface),
    action_(0)
{
}

void VLayerPlugin::initGui()
{
    action_ = new QAction( QIcon( ":/vlayer/vlayer.png" ), tr( "Virtual layer" ), this );
    action_->setObjectName( "action_" );

    connect( action_, SIGNAL( triggered() ), this, SLOT( run() ) );

    iface_->addPluginToMenu( "&Virtual layer", action_ );

    // register sqlite extension
    sqlite3_auto_extension( (void(*)())qgsvlayer_module_init );
}

void VLayerPlugin::run()
{
    std::cout << "run" << std::endl;

    QgsMapLayer* layer = iface_->activeLayer();

    if ( !layer || layer->type() != QgsMapLayer::VectorLayer ) {
        std::cout << "return" << std::endl;
        return;
    }
    QgsVectorLayer *vlayer = static_cast<QgsVectorLayer*>(layer);

    unlink( "/tmp/test_vtable.sqlite" );
    spatialite_init(1);
    sqlite3* db;
    int r = sqlite3_open( "/tmp/test_vtable.sqlite", &db );
    std::cout << "open: " << r << std::endl;
    QString createStr = QString("SELECT InitSpatialMetadata(1); DROP TABLE IF EXISTS vtab; CREATE VIRTUAL TABLE vtab USING QgsVLayer(%1,%2);").arg(vlayer->providerType(), layer->source());
    char *errMsg;
    r = sqlite3_exec( db, createStr.toUtf8().constData(), NULL, NULL, &errMsg );
    if (r) {
        std::cout << "err: " << errMsg << std::endl;
        sqlite3_close( db );
        return;
    }

    sqlite3_close( db );

    QgsVectorLayer* vl = new QgsVectorLayer( "dbname='/tmp/test_vtable.sqlite' table=\"vtab\" sql=", "vtab", "spatialite" );
    std::cout << "vl= " << vl << std::endl;
    std::cout << "isValid = " << vl->isValid() << std::endl;
    QgsMapLayer* new_vl = QgsMapLayerRegistry::instance()->addMapLayer( vl );
    std::cout << "new_vl = " << new_vl << std::endl;

}

void VLayerPlugin::unload()
{
    if (action_) {
        iface_->removePluginMenu( "&Virtual layer", action_ );
        delete action_;
    }
    std::cout << "VLayer unload" << std::endl;

    // unregister sqlite extension
    sqlite3_cancel_auto_extension( (void(*)())qgsvlayer_module_init );
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
