/***************************************************************************
      Virtual layer data provider selection widget

begin                : Feb, 2015
copyright            : (C) 2015 Hugo Mercier, Oslandia
email                : hugo dot mercier at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsembeddedlayerselectdialog.h"

#include <QMainWindow>

#include <qgsvectorlayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>
#include <qgsproviderregistry.h>

QgsEmbeddedLayerSelectDialog::QgsEmbeddedLayerSelectDialog( QWidget* parent, QMainWindow* mainApp )
    : QDialog( parent )
{
    setupUi( this );

    // populate combos
    if ( mainApp ) {
        // look for layers in the layer tree view
        QgsLayerTreeView *tv = mainApp->findChild<QgsLayerTreeView*>( "theLayerTreeView" );
        if ( tv ) {
            auto layers = tv->layerTreeModel()->rootGroup()->findLayers();
            foreach ( const auto& l, layers ) {
                if ( l->layer() && l->layer()->type() == QgsMapLayer::VectorLayer ) {
                    mLayers->addItem( l->layer()->name(), QVariant::fromValue(static_cast<void*>(l->layer())) );
                }
            }
        }
    }

    // providers
    foreach ( auto pk, QgsProviderRegistry::instance()->providerList() ) {
        // we cannot know before trying to actually load a dataset
        // if the provider is raster or vector
        // so we manually exclude well-known raster providers
        if ( pk != "gdal" && pk != "ows" && pk != "wcs" && pk != "wms" ) {
            mProviders->addItem( pk );
        }
    }

    QObject::connect( mImportBtn, SIGNAL(clicked()), this, SLOT(onImport()) );
}

QgsEmbeddedLayerSelectDialog::~QgsEmbeddedLayerSelectDialog()
{
}

QString QgsEmbeddedLayerSelectDialog::getLocalName() const
{
    return mLayerName->text();
}

QString QgsEmbeddedLayerSelectDialog::getProvider() const
{
    return mProviders->currentText();
}

QString QgsEmbeddedLayerSelectDialog::getSource() const
{
    return mSource->toPlainText();
}

void QgsEmbeddedLayerSelectDialog::onImport()
{
    int n = mLayers->currentIndex();
    if ( n != -1) {
        QgsVectorLayer* l = static_cast<QgsVectorLayer*>(mLayers->itemData(n).value<void*>());
        int pi = mProviders->findText( l->providerType() );
        if ( pi != -1 ) {
            mProviders->setCurrentIndex( pi );
        }
        mSource->setPlainText( l->source() );
        mLayerName->setText( l->name() );
    }
}
