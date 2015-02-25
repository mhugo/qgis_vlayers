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

    // type for parameter storage
    typedef QList<QPair<QString, QString> > ParameterPairs;
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
    // return the virtual layer parameters for a set of vector layers
    ParameterPairs createVirtualLayer( const QList<QgsVectorLayer*>& layers );

    // create a virtual layer out of the passed layer insert it in the legend
    void duplicateLayerToVirtual( QgsVectorLayer* vl );

    void creationDialog( const ParameterPairs&  );

    QgisInterface* iface_;
    QAction* createAction_;
    QAction* convertAction_;
    QAction* origFilterAction_;
    QAction* origLayerMenuAction_;

    QgsLayerTreeViewMenuProvider* origMenuProvider_;
};
