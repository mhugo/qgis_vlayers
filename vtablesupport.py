# -*- coding: utf-8 -*-
"""
/***************************************************************************
 VTableSupport
                                 A QGIS plugin
 Virtual table support experimentation
                              -------------------
        begin                : 2014-03-31
        copyright            : (C) 2014 by Hugo Mercier/Oslandia
        email                : hugo.mercier@oslandia.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
"""
# Import the PyQt and QGIS libraries
from PyQt4.QtCore import *
from PyQt4.QtGui import *
from qgis.core import *
# Initialize Qt resources from file resources.py
import resources_rc
# Import the code for the dialog
from vtablesupportdialog import VTableSupportDialog
import os.path

from qgis_vtable import QgsVectorVTable
import apsw

class VTableSupport:

    def __init__(self, iface):
        # Save reference to the QGIS interface
        self.iface = iface
        # initialize plugin directory
        self.plugin_dir = os.path.dirname(__file__)
        # initialize locale
        locale = QSettings().value("locale/userLocale")[0:2]
        localePath = os.path.join(self.plugin_dir, 'i18n', 'vtablesupport_{}.qm'.format(locale))

        if os.path.exists(localePath):
            self.translator = QTranslator()
            self.translator.load(localePath)

            if qVersion() > '4.3.3':
                QCoreApplication.installTranslator(self.translator)

        # Create the dialog (after translation) and keep reference
        self.dlg = VTableSupportDialog()

    def initGui(self):
        # Create action that will start plugin configuration
        self.action = QAction(
            QIcon(":/plugins/vtablesupport/icon.png"),
            u"VTable", self.iface.mainWindow())
        # connect the action to the run method
        self.action.triggered.connect(self.run)

        # Add toolbar button and menu item
        self.iface.addToolBarIcon(self.action)
        self.iface.addPluginToMenu(u"&qgis_vtable", self.action)

    def unload(self):
        # Remove the plugin menu item and icon
        self.iface.removePluginMenu(u"&qgis_vtable", self.action)
        self.iface.removeToolBarIcon(self.action)

    # run method that performs all the real work
    def run(self):
        layer = self.iface.activeLayer()
        print "layer", layer

        conn = apsw.Connection(":memory:")
        print "conn", conn
        qgisvtable = QgsVectorVTable()
        conn.createmodule( "QgsVectorVTable", qgisvtable )
        print "layer id", layer.id()
        conn.cursor().execute("CREATE VIRTUAL TABLE test USING QgsVectorVTable(%s)" % layer.id())

        cur = conn.cursor()
        r = cur.execute("SELECT * FROM test")
        for row in r:
            print row


