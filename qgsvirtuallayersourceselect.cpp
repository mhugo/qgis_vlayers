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
#include "qgsembeddedlayerselectdialog.h"
#include "qgsvirtuallayerdefinition.h"

#include <QUrl>
#include <QMainWindow>
#include <QSettings>
#include <QFileInfo>
#include <QFileDialog>

#include <qgsmaplayerregistry.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>

QgsVirtualLayerSourceSelect::QgsVirtualLayerSourceSelect( QWidget* parent, Qt::WindowFlags fl )
    : QDialog( parent, fl )
{
    setupUi( this );

    QObject::connect( mAddSourceBtn, SIGNAL(clicked()), this, SLOT(onAddSource()) );
    QObject::connect( mRemoveSourceBtn, SIGNAL(clicked()), this, SLOT(onRemoveSource()) );
    QObject::connect( mBrowseBtn, SIGNAL(clicked()), this, SLOT(onBrowse()) );
}

QgsVirtualLayerSourceSelect::~QgsVirtualLayerSourceSelect()
{
}

void QgsVirtualLayerSourceSelect::addSource( const QString& name, const QString& source, const QString& provider, const QString& encoding )
{
    int n = mSourceLayers->rowCount() ? mSourceLayers->rowCount()-1 : 0;
    mSourceLayers->insertRow(n);
    QTableWidgetItem *item;

    item = new QTableWidgetItem( name );
    mSourceLayers->setItem(n, 0, item );

    item = new QTableWidgetItem( provider );
    item->setFlags( item->flags() & ~Qt::ItemIsEditable ); // not editable
    mSourceLayers->setItem(n, 1, item );

    item = new QTableWidgetItem();
    QComboBox* encodingCmb = new QComboBox( mSourceLayers );
    encodingCmb->addItems( QgsVectorDataProvider::availableEncodings() );
    encodingCmb->setCurrentIndex( encodingCmb->findText(encoding) );

    mSourceLayers->setCellWidget( n, 2, encodingCmb );

    item = new QTableWidgetItem( source );
    item->setFlags( item->flags() & ~Qt::ItemIsEditable ); // not editable
    mSourceLayers->setItem(n, 3, item );
}

void QgsVirtualLayerSourceSelect::setQuery( const QString& query )
{
    mQueryEdit->setPlainText( query );
}

void QgsVirtualLayerSourceSelect::setUid( const QString& uid )
{
    mUidField->setText( uid );
}

void QgsVirtualLayerSourceSelect::setLayerName( const QString& name )
{
    mLayerName->setText( name );
}

void QgsVirtualLayerSourceSelect::setGeometryColumn( const QString& geom )
{
    if ( geom == "*no*" ) {
        mHasGeometry->setChecked( Qt::Unchecked );
    }
    else {
        mHasGeometry->setChecked( Qt::Checked );
        mGeometryField->setText( geom );
    }
}

void QgsVirtualLayerSourceSelect::setFilename( const QString& filename )
{
    mFilename->setText( filename );
}

void QgsVirtualLayerSourceSelect::onAddSource()
{
    QScopedPointer<QgsEmbeddedLayerSelectDialog> dlg( new QgsEmbeddedLayerSelectDialog( this, sMainApp ) );
    int r = dlg->exec();
    if ( r == QDialog::Rejected ) {
        return;
    }

    addSource( dlg->getLocalName(), dlg->getSource(), dlg->getProvider(), dlg->getEncoding() );
}

void QgsVirtualLayerSourceSelect::onRemoveSource()
{
    int n = mSourceLayers->currentRow();
    if ( n == -1 ) {
        return;
    }
    mSourceLayers->removeRow(n);
}

void QgsVirtualLayerSourceSelect::onBrowse()
{
    QSettings settings;
    QString lastUsedDir = settings.value( "/UI/lastVirtualLayerDir", "." ).toString();
    QString filename = QFileDialog::getSaveFileName( 0, tr( "Open a virtual layer" ),
                                                     lastUsedDir,
                                                     tr( "Virtual layer" ) + " (*.qgl *.sqlite)" );
    if ( filename.isEmpty() ) {
        return;
    }

    QFileInfo info( filename );
    settings.setValue( "/UI/lastVirtualLayerDir", info.path() );

    mFilename->setText( filename );
}

void QgsVirtualLayerSourceSelect::on_buttonBox_accepted()
{
    QString layer_name = "virtual_layer";
    if ( ! mLayerName->text().isEmpty() ) {
        layer_name = mLayerName->text();
    }
    QUrl url;

    // embedded layers
    for ( int i = 0; i < mSourceLayers->rowCount(); i++ ) {
            QString encodedSource( QUrl::toPercentEncoding(mSourceLayers->item(i,3)->text(), "", ":%") );
            QComboBox* cmb = qobject_cast<QComboBox*>(mSourceLayers->cellWidget(i,2));
            QString encoding = cmb->currentText();
            QString v = QString("%1:%2:%3:%4")
                .arg(mSourceLayers->item(i, 1)->text(), encodedSource, mSourceLayers->item(i,0)->text(), encoding );
            url.addQueryItem( "layer", v );
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
    if ( ! mFilename->text().isEmpty() ) {
        url.setPath( mFilename->text() );
    }
    emit addVectorLayer( url.toString(), layer_name, "virtual" );
}

QGISEXTERN QgsVirtualLayerSourceSelect *createWidget( QWidget *parent, Qt::WindowFlags fl, const QList<QPair<QString, QString> >& parameters )
{
    QgsVirtualLayerSourceSelect *w = new QgsVirtualLayerSourceSelect( parent, fl );
    QString name, source, encoding;
    foreach ( const auto& p, parameters ) {
        if ((p.first == "fromUrl") || (p.first == "fromFile")) {
            QgsVirtualLayerDefinition def;
            if (p.first == "fromUrl") {
                QUrl url( QUrl::fromEncoded( p.second.toLocal8Bit() ) );
                def.fromUrl( url );
            }
            else {
                def = virtualLayerDefinitionFromSqlite( p.second );
            }
            w->setQuery( def.query() );
            w->setUid( def.uid() );
            w->setGeometryColumn( def.geometryField() );
            foreach ( const auto& l, def.sourceLayers() ) {
                w->addSource( l.name(), l.source(), l.provider(), l.encoding() );
            }
            w->setFilename( def.uri() );
            break;
        }
        else if (p.first == "layer") {
            name = p.second;
        }
        else if (p.first == "source") {
            source = QUrl::fromPercentEncoding(p.second.toLocal8Bit());
        }
        else if (p.first == "encoding") {
            encoding = p.second;
        }
        else if (p.first == "provider") {
            w->addSource( name, source, p.second, encoding );
        }
        else if (p.first == "query") {
            w->setQuery( QUrl::fromPercentEncoding(p.second.toLocal8Bit()) );
        }
        else if (p.first == "uid") {
            w->setUid( p.second );
        }
        else if (p.first == "geometry") {
            w->setGeometryColumn( p.second );
        }
        else if (p.first == "file") {
            w->setFilename( p.second );
        }
        else if (p.first == "name") {
            w->setLayerName( p.second );
        }
    }
    return w;
}

QMainWindow *QgsVirtualLayerSourceSelect::sMainApp = 0;

QGISEXTERN void registerGui( QMainWindow *mainWindow )
{
    QgsVirtualLayerSourceSelect::sMainApp = mainWindow;
}
