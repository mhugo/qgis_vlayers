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

#include <QUrl>

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
    mSourceLayers->setItem(n, 3, new QTableWidgetItem( l->providerType() ) );
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
    QUrl url;
    for ( int i = 0; i < mSourceLayers->rowCount(); i++ ) {
        QString encodedSource( QUrl::toPercentEncoding(mSourceLayers->item(i,2)->text(), "", ":%") );
        QString v = QString("%1:%2:%3")
            .arg(mSourceLayers->item(i, 3)->text(), encodedSource, mSourceLayers->item(i,1)->text() );
        QString vv( QUrl::toPercentEncoding(v, "", "%") );
        url.addQueryItem( "layer", vv );
    }
    QString q = mQueryEdit->toPlainText();
    if ( ! q.isEmpty() ) {
        url.addQueryItem( "query", q );
    }
    if ( ! mUidField->text().isEmpty() ) {
        url.addQueryItem( "uid", mUidField->text() );
    }
    if ( ! mGeometryField->text().isEmpty() ) {
        url.addQueryItem( "geometry", mGeometryField->text() );
    }
    emit addVectorLayer( url.toString(), layer_name, "virtual" );
}

QGISEXTERN QgsVirtualLayerSourceSelect *selectWidget( QWidget *parent, Qt::WindowFlags fl )
{
  return new QgsVirtualLayerSourceSelect( parent, fl );
}
