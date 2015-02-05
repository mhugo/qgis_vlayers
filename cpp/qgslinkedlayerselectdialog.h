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

#ifndef QGSVIRTUAL_LINKED_LAYER_SELECT_DIALOG_H
#define QGSVIRTUAL_LINKED_LAYER_SELECT_DIALOG_H

#include "ui_qgslinkedlayerselect.h"

#include <QDialog>

class QgsVectorLayer;
class QMainWindow;

class QgsLinkedLayerSelectDialog : public QDialog, private Ui::QgsLinkedLayerSelectDialog
{
    Q_OBJECT

  public:
    QgsLinkedLayerSelectDialog( QWidget * parent, QMainWindow* mainApp );
    ~QgsLinkedLayerSelectDialog();

    QgsVectorLayer* getLayer() const;
    QString getLocalName() const;

private slots:
    void onItemChanged( QListWidgetItem*, QListWidgetItem * );
};

#endif
