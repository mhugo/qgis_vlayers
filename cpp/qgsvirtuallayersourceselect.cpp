/***************************************************************************
           qgsvirtuallayersourceselect.cpp
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

#include "qgsvirtuallayersourceselect.h"

#include <qgsmaplayerregistry.h>
#include <qgsvectorlayer.h>

QgsVirtualLayerSourceSelect::QgsVirtualLayerSourceSelect( QWidget* parent, Qt::WindowFlags fl, bool embedded )
    : QDialog( parent, fl )
{
    setupUi( this );

    QObject::connect( mAddSourceBtn, SIGNAL(clicked()), this, SLOT(onAddSource()) );
    QObject::connect( mRemoveSourceBtn, SIGNAL(clicked()), this, SLOT(onRemoveSource()) );

    // populate the layer combo box
    auto mapLayers = QgsMapLayerRegistry::instance()->mapLayers();
    for ( auto& m : mapLayers.keys() ) {
        QgsMapLayer* l = mapLayers.value(m);
        if (l->type() == QgsMapLayer::VectorLayer) {
            mLayersCombo->addItem( mapLayers.value(m)->name(), QVariant::fromValue(static_cast<void*>(l)) );
        }
    }
}

QgsVirtualLayerSourceSelect::~QgsVirtualLayerSourceSelect()
{
}

void QgsVirtualLayerSourceSelect::onAddSource()
{
    QString lname = mLayersCombo->currentText();
    QString new_name = mSourceName->text();
    QString local_name = new_name == "" ? lname : new_name;
    QgsVectorLayer* l = static_cast<QgsVectorLayer*>(mLayersCombo->itemData( mLayersCombo->currentIndex() ).value<void*>());
    int n = mSourceLayers->rowCount() ? mSourceLayers->rowCount()-1 : 0;
    mSourceLayers->insertRow(n);
    mSourceLayers->setItem(n, 0, new QTableWidgetItem( l->id() ) );
    mSourceLayers->setItem(n, 1, new QTableWidgetItem( local_name ) );
    mSourceLayers->setItem(n, 2, new QTableWidgetItem( l->source() ) );
}

void QgsVirtualLayerSourceSelect::onRemoveSource()
{
    int n = mSourceLayers->currentRow();
    if ( n == -1 ) {
        return;
    }
    mSourceLayers->removeRow(n);
}

void QgsVirtualLayerSourceSelect::on_buttonBox_accepted()
{
    QString layer_name = "virtual_layer";
    if ( ! mLayerName->text().isEmpty() ) {
        layer_name = mLayerName->text();
    }
    if ( mSourceLayers->rowCount() >= 1 ) {
        emit addVectorLayer( QString("?layer_ref=%1").arg(mSourceLayers->itemAt(0,0)->text()), layer_name, "virtual" );
    }
    QDialog::accepted();
}

QGISEXTERN QgsVirtualLayerSourceSelect *selectWidget( QWidget *parent, Qt::WindowFlags fl )
{
  return new QgsVirtualLayerSourceSelect( parent, fl );
}
