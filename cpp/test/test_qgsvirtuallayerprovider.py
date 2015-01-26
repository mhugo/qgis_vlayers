# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsVirtualLayerProvider

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Hugo Mercier'
__date__ = '26/01/2015'
__copyright__ = 'Copyright 2015, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis

from PyQt4.QtCore import *

from qgis.core import (QGis,
                       QgsVectorLayer,
                       QgsFeature,
                       QgsFeatureRequest,
                       QgsField,
                       QgsGeometry,
                       QgsPoint,
                       QgsMapLayerRegistry
                      )

from utilities import (getQgisTestApp,
                       TestCase,
                       unittest,
                       unitTestDataPath
                       )
QGISAPP, CANVAS, IFACE, PARENT = getQgisTestApp()

import os

class TestQgsVirtualLayerProvider(TestCase):

    @classmethod
    def setUpClass(cls):
        #        cls.testDataDir_ = unitTestDataPath()
        cls.testDataDir_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../testdata"))

    def testPointCtor(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        self.assertEqual( l1.isValid(), True )

        os.unlink("/tmp/test_vtable.sqlite")
        l2 = QgsVectorLayer( "/tmp/test_vtable.sqlite?layer_id=" + l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

if __name__ == '__main__':
    unittest.main()
