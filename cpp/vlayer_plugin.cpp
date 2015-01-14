#define CORE_EXPORT
#define GUI_EXPORT
#include <qgis.h>
#include <qgisinterface.h>
#include <qgisplugin.h>

#include <iostream>

class VLayerPlugin : public QgisPlugin
{
public:
    VLayerPlugin( QgisInterface *iface ) : iface_(iface) {
    }

    void initGui()
    {
        std::cout << "VLayer initGui" << std::endl;
    }

    void unload()
    {
        std::cout << "VLayer unload" << std::endl;
    }
private:
    QgisInterface* iface_;
};

QGISEXTERN QgisPlugin * classFactory( QgisInterface * iface )
{
  return new VLayerPlugin( iface );
}
// Return the name of the plugin - note that we do not user class members as
// the class may not yet be insantiated when this method is called.
QGISEXTERN QString name()
{
  return "Vitual layer plugin";
}

// Return the description
QGISEXTERN QString description()
{
  return "This is a POC virtual layer plugin";
}

// Return the category
QGISEXTERN QString category()
{
  return "Plugins";
}

// Return the type (either UI or MapLayer plugin)
QGISEXTERN int type()
{
    return QgisPlugin::UI;
}

// Return the version number for the plugin
QGISEXTERN QString version()
{
  return "Version 0.1";
}

// Return the icon
QGISEXTERN QString icon()
{
  return ":/vlayer/vlayer.png";
}

// Return the experimental status for the plugin
QGISEXTERN QString experimental()
{
  return "true";
}

// Delete ourself
QGISEXTERN void unload( QgisPlugin * thePluginPointer )
{
  delete thePluginPointer;
}
