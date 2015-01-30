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

    def testCsvNoGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext")
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_id=" + l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def testDynamicGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/testextpt.txt") + "?type=csv&delimiter=%7C&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext")
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_id=%s&query=select *,makepoint(x,y) as geom from vtab1&geometry=geom:1:4326&uid=id" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def testShapefileWithGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        # use a temporary file
        l2 = QgsVectorLayer( "?layer_id=" + l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def testQuery(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        ref_sum = sum(f.attributes()[0] for f in l1.getFeatures())

        l2 = QgsVectorLayer( "?layer_id=%s&geometry=geometry:3:4326&query=SELECT * FROM vtab1&uid=OBJECTID" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # the same, without geometry
        l2 = QgsVectorLayer( "?layer_id=%s&query=SELECT * FROM vtab1&uid=ObJeCtId" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        self.assertEqual( ref_sum, ref_sum2 )
        self.assertEqual( ref_sum, ref_sum3 )

        # check that it fails when a query and no uid are specified
        l2 = QgsVectorLayer( "?layer_id=%s&query=SELECT * FROM vtab1" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), False )

    def testQueryTableName(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_id=%s:vt&query=SELECT * FROM vt&uid=ObJeCtId" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry

    def testJoin(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "points.shp"), "points", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "points_relations.shp"), "points_relations", "ogr" )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)
        ref_sum = sum(f.attributes()[1] for f in l2.getFeatures())

        # use a temporary file
        l3 = QgsVectorLayer( "?layer_id=%s&layer_id=%s&uid=id&query=select id,Pilots,vtab1.geometry from vtab1,vtab2 where intersects(vtab1.geometry,vtab2.geometry)&geometry=geometry:1:4326" % (l1.id(), l2.id()), "vtab", "virtual" )
        self.assertEqual( l3.isValid(), True )
        self.assertEqual( l3.dataProvider().geometryType(), 1 )
        self.assertEqual( l3.dataProvider().fields().count(), 2 )
        ref_sum2 = sum(f.id() for f in l3.getFeatures())
        self.assertEqual(ref_sum, ref_sum2)


if __name__ == '__main__':
    unittest.main()
