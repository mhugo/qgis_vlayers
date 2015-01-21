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
    void unload();
public slots:
    void run();
private:
    QgisInterface* iface_;
    QAction* action_;
};
