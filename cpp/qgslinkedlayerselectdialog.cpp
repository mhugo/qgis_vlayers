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

#include "qgslinkedlayerselectdialog.h"

#include <QMainWindow>

#include <qgsvectorlayer.h>
#include <qgslayertreeview.h>
#include <qgslayertreemodel.h>
#include <qgslayertreegroup.h>
#include <qgslayertreelayer.h>

QgsLinkedLayerSelectDialog::QgsLinkedLayerSelectDialog( QWidget* parent, QMainWindow* mainApp )
    : QDialog( parent )
{
    setupUi( this );

    if ( mainApp ) {
        // look for layers in the layer tree view
        QgsLayerTreeView *tv = mainApp->findChild<QgsLayerTreeView*>( "theLayerTreeView" );
        if ( tv ) {
            auto layers = tv->layerTreeModel()->rootGroup()->findLayers();
            for ( auto& l : layers ) {
                if ( l->layer() && l->layer()->type() == QgsMapLayer::VectorLayer ) {
                    auto item = new QListWidgetItem(l->layer()->name());
                    item->setData( Qt::UserRole, QVariant::fromValue(static_cast<void*>(l->layer())) );
                    mLayerList->addItem( item );
                }
            }
        }
    }

    QObject::connect( mLayerList, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
                      this, SLOT(onItemChanged(QListWidgetItem*,QListWidgetItem*)));
}

void QgsLinkedLayerSelectDialog::onItemChanged( QListWidgetItem* current, QListWidgetItem* previous )
{
    if (current) {
        mLayerName->setText( current->text() );
    }
}

QgsLinkedLayerSelectDialog::~QgsLinkedLayerSelectDialog()
{
}

QgsVectorLayer* QgsLinkedLayerSelectDialog::getLayer() const
{
    QListWidgetItem* item = mLayerList->currentItem();
    if ( item ) {
        return static_cast<QgsVectorLayer*>(item->data(Qt::UserRole).value<void*>());
    }
    return 0;
}

QString QgsLinkedLayerSelectDialog::getLocalName() const
{
    if (mLayerName->text().isEmpty()) {
        auto l = getLayer();
        return l ? l->name() : "vtab";
    }
    return mLayerName->text();
}
