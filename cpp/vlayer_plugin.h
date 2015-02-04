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
private slots:
    void addVectorLayer( const QString& source, const QString& name, const QString& provider );
private:
    QgisInterface* iface_;
    QAction* action_;
};
