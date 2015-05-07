//
// This plugin is here to demonstrate the integration of virtual layers into the QGIS GUI
// It should be considered temporary. The idea is to include virtual layers into the core

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
private slots:
    void addVectorLayer( const QString& source, const QString& name, const QString& provider );
    void onContextMenu( const QPoint& );
    void onLayerFilter();
    void onLayerFilterFromMenu();
    void onLayerFilterFromContextMenu();
    void onLayerSettings();
    void onAddLayer();
private:
    // return the virtual layer parameters for a set of vector layers
    ParameterPairs createVirtualLayer( const QList<QgsVectorLayer*>& layers );

    // create a virtual layer out of the passed layer insert it in the legend
    void duplicateLayerToVirtual( QgsVectorLayer* vl );

    void creationDialog( const ParameterPairs&, bool replaceMode = false );

    QgisInterface* iface_;
    QAction* createAction_;
    QAction* addAction_;
    QAction* origFilterAction_;
    QAction* origLayerMenuAction_;
    QAction* settingsAction_;

    QgsLayerTreeViewMenuProvider* origMenuProvider_;

    bool mReplaceMode;
};
