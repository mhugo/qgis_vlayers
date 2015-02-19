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
    void onCreate();
    void onConvert();
private slots:
    void addVectorLayer( const QString& source, const QString& name, const QString& provider );
    void onContextMenu( const QPoint& );
    void onLayerFilter();
    void onLayerFilterFromMenu();
    void onLayerFilterFromContextMenu();
private:
    QgsVectorLayer* createVirtualLayer( QgsVectorLayer* vl );
    void creationDialog( QList<QgsVectorLayer*> sourceLayers );

    QgisInterface* iface_;
    QAction* createAction_;
    QAction* convertAction_;
    QAction* origFilterAction_;
    QAction* origLayerMenuAction_;

    QgsLayerTreeViewMenuProvider* origMenuProvider_;
};
