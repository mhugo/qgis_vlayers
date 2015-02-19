#include <QAction>

#include <qgis.h>
#include <qgisinterface.h>
#include <qgisplugin.h>

class QgsLayerTreeViewMenuProvider;

class VLayerPlugin : public QObject, public QgisPlugin
{
    Q_OBJECT

public:
    VLayerPlugin( QgisInterface *iface );
    void initGui();
    void unload();
public slots:
    void run();
    void onConvert();
private slots:
    void addVectorLayer( const QString& source, const QString& name, const QString& provider );
    void onContextMenu( const QPoint& );
    void onLayerFilter();
private:
    QgsVectorLayer* createVirtualLayer( QgsVectorLayer* vl );
    QgisInterface* iface_;
    QAction* action_;
    QAction* convertAction_;
    QAction* origFilterAction_;

    QgsLayerTreeViewMenuProvider* origMenuProvider_;
};
