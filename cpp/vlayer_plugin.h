#define CORE_EXPORT
#define GUI_EXPORT
#include <QAction>

#include <qgis.h>
#include <qgisinterface.h>
#include <qgisplugin.h>

class VLayerPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

public:
    VLayerPlugin( QgisInterface *iface );
    void initGui();
    void run();
    void unload();
private:
    QgisInterface* iface_;
    QAction* action_;
};
