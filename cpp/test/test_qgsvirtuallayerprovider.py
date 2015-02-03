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

    def tearDown(self):
        QgsMapLayerRegistry.instance().removeAllMapLayers()

    def test_CsvNoGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext")
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=" + l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def test_DynamicGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/testextpt.txt") + "?type=csv&delimiter=%7C&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext")
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=%s&query=select *,makepoint(x,y) as geom from vtab1&geometry=geom&uid=id" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def test_FieldTypes(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/testextpt.txt") + "?type=csv&delimiter=%7C&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext")
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=%s&field=x:double" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        for f in l2.dataProvider().fields():
            if f.name() == "x":
                self.assertEqual( f.type(), QVariant.Double )

    def test_ShapefileWithGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        # use a temporary file
        l2 = QgsVectorLayer( "?layer_ref=" + l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

        l2 = QgsVectorLayer( "?layer_ref=%s:nn" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )

    def test_Query(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        ref_sum = sum(f.attributes()[0] for f in l1.getFeatures())

        l2 = QgsVectorLayer( "?layer_ref=%s&geometry=geometry&query=SELECT * FROM vtab1&uid=OBJECTID" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # the same, without specifying the geometry column name
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT * FROM vtab1&uid=OBJECTID" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # with two geometry columns
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT *,geometry as geom FROM vtab1&uid=OBJECTID&geometry=geom" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # with two geometry columns, but no geometry column specified (will take the first)
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT *,geometry as geom FROM vtab1&uid=OBJECTID" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # the same, without geometry
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT * FROM vtab1&uid=ObJeCtId&nogeometry" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        self.assertEqual( ref_sum, ref_sum2 )
        self.assertEqual( ref_sum, ref_sum3 )

        # check that it fails when a query and no uid are specified
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT * FROM vtab1" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), False )

        # check that it fails when a query has a wrong geometry column
        l2 = QgsVectorLayer( "?layer_ref=%s&query=SELECT * FROM vtab1&geometry=geo" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), False )

    def test_QueryUrlEncoding( self ):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        query = str(QUrl.toPercentEncoding("SELECT * FROM vtab1"))
        l2 = QgsVectorLayer("?layer_ref=%s&query=%s&uid=ObjectId&nogeometry" % (l1.id(), query), "vtab", "virtual")
        self.assertEqual( l2.isValid(), True )

    def test_QueryTableName(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=%s:vt&query=SELECT * FROM vt&uid=ObJeCtId&nogeometry" % l1.id(), "vtab", "virtual" )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry

    def test_Join(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "points.shp"), "points", "ogr" )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "points_relations.shp"), "points_relations", "ogr" )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)
        ref_sum = sum(f.attributes()[1] for f in l2.getFeatures())

        # use a temporary file
        l3 = QgsVectorLayer( "?layer_ref=%s&layer_ref=%s&uid=id&query=select id,Pilots,vtab1.geometry from vtab1,vtab2 where intersects(vtab1.geometry,vtab2.geometry)" % (l1.id(), l2.id()), "vtab", "virtual" )
        self.assertEqual( l3.isValid(), True )
        self.assertEqual( l3.dataProvider().geometryType(), 1 )
        self.assertEqual( l3.dataProvider().fields().count(), 2 )
        ref_sum2 = sum(f.id() for f in l3.getFeatures())
        self.assertEqual(ref_sum, ref_sum2)

    def test_geometryTypes( self ):

        geo = [ (1, "POINT", "(0 0)" ),
                (2, "LINESTRING", "(0 0,1 0)"),
                (3, "POLYGON", "((0 0,1 0,1 1,0 0))"),
                (4, "MULTIPOINT", "(1 1)"),
                (5, "MULTILINESTRING", "((0 0,1 0),(0 1,1 1))"),
                (6, "MULTIPOLYGON", "(((0 0,1 0,1 1,0 0)),((2 2,3 0,3 3,2 2)))") ]
        for wkb_type, wkt_type, wkt in geo:
            l = QgsVectorLayer( "%s?crs=epsg:4326" % wkt_type, "m1", "memory" )
            self.assertEqual( l.isValid(), True )
            QgsMapLayerRegistry.instance().addMapLayer(l)

            f1 = QgsFeature(1)
            f1.setGeometry(QgsGeometry.fromWkt(wkt_type+wkt))
            l.dataProvider().addFeatures([f1])

            l2 = QgsVectorLayer( "?layer_ref=%s" % l.id(), "vtab", "virtual" )
            self.assertEqual( l2.isValid(), True )
            self.assertEqual( l2.dataProvider().featureCount(), 1 )
            self.assertEqual( l2.dataProvider().geometryType(), wkb_type )

    def test_embeddedLayer( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        l = QgsVectorLayer("?layer=ogr:%s" % source, "vtab", "virtual")
        self.assertEqual(l.isValid(), True)

        l = QgsVectorLayer("?layer=ogr:%s:nn" % source, "vtab", "virtual")
        self.assertEqual(l.isValid(), True)

if __name__ == '__main__':
    unittest.main()
