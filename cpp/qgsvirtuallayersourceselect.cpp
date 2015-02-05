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
#include <QMainWindow>

#include <qgsmaplayerregistry.h>
#include <qgsvectorlayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>

QgsVirtualLayerSourceSelect::QgsVirtualLayerSourceSelect( QWidget* parent, Qt::WindowFlags fl, bool embedded )
    : QDialog( parent, fl )
{
    setupUi( this );

    QObject::connect( mAddSourceBtn, SIGNAL(clicked()), this, SLOT(onAddSource()) );
    QObject::connect( mRemoveSourceBtn, SIGNAL(clicked()), this, SLOT(onRemoveSource()) );

    if ( sMainApp ) {
        // look for layers in the layer tree view
        QgsLayerTreeView *tv = sMainApp->findChild<QgsLayerTreeView*>( "theLayerTreeView" );
        if ( tv ) {
            auto layers = tv->layerTreeModel()->rootGroup()->findLayers();
            for ( auto& l : layers ) {
                if ( l->layer() && l->layer()->type() == QgsMapLayer::VectorLayer ) {
                    mLayersCombo->addItem( l->layer()->name(), QVariant::fromValue(static_cast<void*>(l->layer())) );
                }
            }
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
    QTableWidgetItem *item;

    item = new QTableWidgetItem( l->id() );
    item->setFlags( item->flags() & ~Qt::ItemIsEditable ); // not editable
    mSourceLayers->setItem(n, 0, item );

    item = new QTableWidgetItem( local_name );
    mSourceLayers->setItem(n, 1, item );

    item = new QTableWidgetItem( l->source() );
    item->setFlags( item->flags() & ~Qt::ItemIsEditable ); // not editable
    mSourceLayers->setItem(n, 2, item );

    item = new QTableWidgetItem( l->providerType() );
    item->setFlags( item->flags() & ~Qt::ItemIsEditable ); // not editable
    mSourceLayers->setItem(n, 3, item );

    item = new QTableWidgetItem();
    if ( l->providerType() == "memory" ) {
        // memory layers are by default "referenced" rather than "embedded"
        item->setCheckState(Qt::Checked);
    }
    else {
        item->setCheckState(Qt::Unchecked);
    }
    mSourceLayers->setItem(n, 4, item );
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
        if ( mSourceLayers->item(i,4)->checkState() != Qt::Checked ) {
            // an embedded layer
            QString encodedSource( QUrl::toPercentEncoding(mSourceLayers->item(i,2)->text(), "", ":%") );
            QString v = QString("%1:%2:%3")
                .arg(mSourceLayers->item(i, 3)->text(), encodedSource, mSourceLayers->item(i,1)->text() );
            QString vv( QUrl::toPercentEncoding(v, "", "%") );
            url.addQueryItem( "layer", vv );
        }
        else {
            // a referenced layer
            QString v = QString("%1:%2").arg(mSourceLayers->item(i, 0)->text(), mSourceLayers->item(i,1)->text() );
            QString vv( QUrl::toPercentEncoding(v, "", "%") );
            url.addQueryItem( "layer_ref", vv );
        }
    }
    QString q = mQueryEdit->toPlainText();
    if ( ! q.isEmpty() ) {
        url.addQueryItem( "query", q );
    }
    if ( ! mUidField->text().isEmpty() ) {
        url.addQueryItem( "uid", mUidField->text() );
    }
    if ( mHasGeometry->checkState() == Qt::Unchecked ) {
        url.addQueryItem( "nogeometry", "" );
    }
    else if ( ! mGeometryField->text().isEmpty() ) {
        url.addQueryItem( "geometry", mGeometryField->text() );
    }
    emit addVectorLayer( url.toString(), layer_name, "virtual" );
}

QGISEXTERN QgsVirtualLayerSourceSelect *selectWidget( QWidget *parent, Qt::WindowFlags fl )
{
  return new QgsVirtualLayerSourceSelect( parent, fl );
}

QMainWindow *QgsVirtualLayerSourceSelect::sMainApp = 0;

QGISEXTERN void registerGui( QMainWindow *mainWindow )
{
    QgsVirtualLayerSourceSelect::sMainApp = mainWindow;
}
