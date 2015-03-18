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
                       QgsMapLayerRegistry,
                       QgsRectangle,
                       QgsErrorMessage,
                       QgsProviderRegistry
                      )

from utilities import (getQgisTestApp,
                       TestCase,
                       unittest,
                       unitTestDataPath
                       )
QGISAPP, CANVAS, IFACE, PARENT = getQgisTestApp()

import os
import tempfile

class TestQgsVirtualLayerProvider(TestCase):

    @classmethod
    def setUpClass(cls):
        #        cls.testDataDir_ = unitTestDataPath()
        cls.testDataDir_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../testdata"))

    def setUp(self):
        print "****************************************************"
        print "In method", self._testMethodName
        print "****************************************************"

    def test_CsvNoGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=" + l1.id(), "vtab", "virtual", False )
        self.assertEqual(l2.isValid(), True)
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())        

    def test_source_escaping(self):
        # the source contains ':'
        source = QUrl.toPercentEncoding("file:///" + os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no")
        l = QgsVectorLayer( "?layer=delimitedtext:%s:t" % source, "vtab", "virtual", False )
        self.assertEqual(l.isValid(), True)
        print sum([f.id() for f in l.getFeatures()])

    def test_DynamicGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/testextpt.txt") + "?type=csv&delimiter=%7C&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        query = QUrl.toPercentEncoding("select *,makepoint(x,y) as geom from vtab1")
        l2 = QgsVectorLayer( "?layer_ref=%s&query=%s&geometry=geom&uid=id" % (l1.id(),query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())        

    def test_FieldTypes(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/testextpt.txt") + "?type=csv&delimiter=%7C&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=%s&field=x:double" % l1.id(), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        for f in l2.dataProvider().fields():
            if f.name() == "x":
                self.assertEqual( f.type(), QVariant.Double )
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())        

    def test_ShapefileWithGeometry(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        # use a temporary file
        l2 = QgsVectorLayer( "?layer_ref=" + l1.id(), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        print sum([f.id() for f in l2.getFeatures()])

        l2 = QgsVectorLayer( "?layer_ref=%s:nn" % l1.id(), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        print sum([f.id() for f in l2.getFeatures()])

        print "**remove layer ", l1.id()
        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())        
        print "**after remove layer "

    def test_Query(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        ref_sum = sum(f.attributes()[0] for f in l1.getFeatures())

        query = QUrl.toPercentEncoding("SELECT * FROM vtab1")
        l2 = QgsVectorLayer( "?layer_ref=%s&geometry=geometry:3:4326&query=%s&uid=OBJECTID" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        for f in l2.getFeatures():
            print f.id(), f.attributes()
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # the same, without specifying the geometry column name
        l2 = QgsVectorLayer( "?layer_ref=%s&query=%s&uid=OBJECTID" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # with two geometry columns
        query = QUrl.toPercentEncoding("SELECT *,geometry as geom FROM vtab1")
        l2 = QgsVectorLayer( "?layer_ref=%s&query=%s&uid=OBJECTID&geometry=geom:3:4326" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # with two geometry columns, but no geometry column specified (will take the first)
        l2 = QgsVectorLayer( "?layer_ref=%s&query=%s&uid=OBJECTID" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        # check we have the same rows
        self.assertEqual( ref_sum, ref_sum2 )
        # check the id is ok
        self.assertEqual( ref_sum, ref_sum3 )

        # the same, without geometry
        query = QUrl.toPercentEncoding("SELECT * FROM ww")
        l2 = QgsVectorLayer( "?layer_ref=%s:ww&query=%s&uid=ObJeCtId&nogeometry" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry
        ref_sum2 = sum(f.attributes()[0] for f in l2.getFeatures())
        ref_sum3 = sum(f.id() for f in l2.getFeatures())
        self.assertEqual( ref_sum, ref_sum2 )
        self.assertEqual( ref_sum, ref_sum3 )

        # check that it fails when a query has a wrong geometry column
        l2 = QgsVectorLayer( "?layer_ref=%s&query=%s&geometry=geo" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), False )

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())

    def test_QueryUrlEncoding( self ):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        query = str(QUrl.toPercentEncoding("SELECT * FROM vtab1"))
        l2 = QgsVectorLayer("?layer_ref=%s&query=%s&uid=ObjectId&nogeometry" % (l1.id(), query), "vtab", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())

    def test_QueryTableName(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        query = str(QUrl.toPercentEncoding("SELECT * FROM vt"))
        l2 = QgsVectorLayer( "?layer_ref=%s:vt&query=%s&uid=ObJeCtId&nogeometry" % (l1.id(), query), "vtab", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())

    def test_Join(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "points.shp"), "points", "ogr", False )
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "points_relations.shp"), "points_relations", "ogr", False )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)
        ref_sum = sum(f.attributes()[1] for f in l2.getFeatures())

        # use a temporary file
        query = QUrl.toPercentEncoding("select id,Pilots,vtab1.geometry from vtab1,vtab2 where intersects(vtab1.geometry,vtab2.geometry)")
        l3 = QgsVectorLayer( "?layer_ref=%s&layer_ref=%s&uid=id&query=%s&geometry=geometry:1:4326" % (l1.id(), l2.id(), query), "vtab", "virtual", False )
        self.assertEqual( l3.isValid(), True )
        self.assertEqual( l3.dataProvider().geometryType(), 1 )
        self.assertEqual( l3.dataProvider().fields().count(), 2 )
        ref_sum2 = sum(f.id() for f in l3.getFeatures())
        self.assertEqual(ref_sum, ref_sum2)

        QgsMapLayerRegistry.instance().removeMapLayer(l1.id())
        QgsMapLayerRegistry.instance().removeMapLayer(l2.id())

    def test_geometryTypes( self ):

        geo = [ (1, "POINT", "(0 0)" ),
                (2, "LINESTRING", "(0 0,1 0)"),
                (3, "POLYGON", "((0 0,1 0,1 1,0 0))"),
                (4, "MULTIPOINT", "(1 1)"),
                (5, "MULTILINESTRING", "((0 0,1 0),(0 1,1 1))"),
                (6, "MULTIPOLYGON", "(((0 0,1 0,1 1,0 0)),((2 2,3 0,3 3,2 2)))") ]
        for wkb_type, wkt_type, wkt in geo:
            l = QgsVectorLayer( "%s?crs=epsg:4326" % wkt_type, "m1", "memory", False )
            self.assertEqual( l.isValid(), True )
            QgsMapLayerRegistry.instance().addMapLayer(l)

            f1 = QgsFeature(1)
            f1.setGeometry(QgsGeometry.fromWkt(wkt_type+wkt))
            l.dataProvider().addFeatures([f1])

            l2 = QgsVectorLayer( "?layer_ref=%s" % l.id(), "vtab", "virtual", False )
            self.assertEqual( l2.isValid(), True )
            self.assertEqual( l2.dataProvider().featureCount(), 1 )
            self.assertEqual( l2.dataProvider().geometryType(), wkb_type )
            print sum([f.id() for f in l2.getFeatures()])

            QgsMapLayerRegistry.instance().removeMapLayer(l.id())

    def test_embeddedLayer( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        l = QgsVectorLayer("?layer=ogr:%s" % source, "vtab", "virtual", False)
        self.assertEqual(l.isValid(), True)

        l = QgsVectorLayer("?layer=ogr:%s:nn" % source, "vtab", "virtual", False)
        self.assertEqual(l.isValid(), True)
        print sum([f.id() for f in l.getFeatures()])

    def test_filter_rect( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))

        query = QUrl.toPercentEncoding("select * from vtab where _search_frame_=BuildMbr(-2.10,49.38,-1.3,49.99,4326)")
        l2 = QgsVectorLayer("?layer=ogr:%s:vtab&query=%s&uid=objectid" % (source,query), "vtab2", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        self.assertEqual(l2.dataProvider().featureCount(), 1)
        a = [fit.attributes()[4] for fit in l2.getFeatures()]
        self.assertEqual(a, [u"Basse-Normandie"])

    def test_recursiveLayer( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        l = QgsVectorLayer("?layer=ogr:%s" % source, "vtab", "virtual", False)
        self.assertEqual(l.isValid(), True)
        QgsMapLayerRegistry.instance().addMapLayer(l)

        l2 = QgsVectorLayer("?layer_ref=" + l.id(), "vtab2", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        print sum([f.id() for f in l2.getFeatures()])

        QgsMapLayerRegistry.instance().removeMapLayer(l.id())

    def test_no_geometry( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        l2 = QgsVectorLayer("?layer=ogr:%s:vtab&nogeometry" % source, "vtab2", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 ) # NoGeometry
        print sum([f.id() for f in l2.getFeatures()])

    def test_reopen( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        tmp = os.path.join(tempfile.gettempdir(), "t.sqlite")
        l = QgsVectorLayer("%s?layer=ogr:%s:vtab" % (tmp, source), "vtab2", "virtual", False)
        self.assertEqual( l.isValid(), True )

        l2 = QgsVectorLayer( tmp, "tt", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        self.assertEqual( l2.dataProvider().featureCount(), 4 )
        print sum([f.id() for f in l2.getFeatures()])

    def test_reopen2( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        tmp = os.path.join(tempfile.gettempdir(), "t.sqlite")
        l = QgsVectorLayer("%s?layer=ogr:%s:vtab&nogeometry" % (tmp,source), "vtab2", "virtual", False)
        self.assertEqual( l.isValid(), True )

        l2 = QgsVectorLayer( tmp, "tt", "virtual", False )
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 )
        self.assertEqual( l2.dataProvider().featureCount(), 4 )
        print sum([f.id() for f in l2.getFeatures()])

    def test_reopen3( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        tmp = os.path.join(tempfile.gettempdir(), "t.sqlite")
        query = QUrl.toPercentEncoding( "SELECT * FROM vtab")
        l = QgsVectorLayer("%s?layer=ogr:%s:vtab&query=%s&uid=objectid&geometry=geometry:3:4326" % (tmp,source,query), "vtab2", "virtual", False)
        self.assertEqual( l.isValid(), True )

        l2 = QgsVectorLayer(tmp, "tt", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 3 )
        self.assertEqual( l2.dataProvider().featureCount(), 4 )
        print sum([f.id() for f in l2.getFeatures()])

    def test_reopen4( self ):
        source = QUrl.toPercentEncoding(os.path.join(self.testDataDir_, "france_parts.shp"))
        tmp = os.path.join(tempfile.gettempdir(), "t.sqlite")
        query = QUrl.toPercentEncoding( "SELECT * FROM vtab")
        l = QgsVectorLayer("%s?layer=ogr:%s:vtab&query=%s&uid=objectid&nogeometry" % (tmp,source,query), "vtab2", "virtual", False)
        self.assertEqual( l.isValid(), True )

        l2 = QgsVectorLayer(tmp, "tt", "virtual", False)
        self.assertEqual( l2.isValid(), True )
        self.assertEqual( l2.dataProvider().geometryType(), 100 )
        self.assertEqual( l2.dataProvider().featureCount(), 4 )
        print sum([f.id() for f in l2.getFeatures()])

    def test_refLayer(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l2 = QgsVectorLayer( "?layer_ref=" + l1.id(), "vtab", "virtual", False )
        self.assertEqual(l2.isValid(), True)
        print sum([f.id() for f in l2.getFeatures()])

        # now delete the layer
        QgsMapLayerRegistry.instance().removeMapLayer( l1.id() )
        # check that it does not crash
        print sum([f.id() for f in l2.getFeatures()])

    def test_refLayers(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        
        # cf qgis bug #12266
        for i in range(10):
            q = QUrl.toPercentEncoding("select * from t" + str(i))
            l2 = QgsVectorLayer( "?layer_ref=%s:t%d&query=%s&uid=id" % (l1.id(),i,q), "vtab", "virtual", False )
            QgsMapLayerRegistry.instance().addMapLayer(l2)
            self.assertEqual(l2.isValid(), True)
            s = sum([f.id() for f in l2.dataProvider().getFeatures()])
            self.assertEqual(sum([f.id() for f in l2.getFeatures()]), 21)
            QgsMapLayerRegistry.instance().removeMapLayer(l2.id())

    def test_refLayers2(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)
        
        # referenced layers cannot be stored !
        tmp = os.path.join(tempfile.gettempdir(), "t.sqlite")
        l2 = QgsVectorLayer( "%s?layer_ref=%s" % (tmp, l1.id()), "tt", "virtual", False )
        self.assertEqual( l2.isValid(), False )
        self.assertEqual( "Cannot store referenced layers" in l2.dataProvider().error().message(), True )

    def test_sql(self):
        l1 = QgsVectorLayer( os.path.join(self.testDataDir_, "delimitedtext/test.csv") + "?type=csv&geomType=none&subsetIndex=no&watchFile=no", "test", "delimitedtext", False)
        self.assertEqual( l1.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l1)

        l3 = QgsVectorLayer( "?query=SELECT * FROM test", "tt", "virtual" )

        self.assertEqual( l3.isValid(), True )
        s = sum(f.id() for f in l3.getFeatures())
        self.assertEqual( s, 15 )

    def test_sql2(self):
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)

        query = QUrl.toPercentEncoding( "SELECT * FROM france_parts")
        l4 = QgsVectorLayer( "?query=%s" % query, "tt", "virtual" )
        self.assertEqual( l4.isValid(), True )

        self.assertEqual(l4.dataProvider().geometryType(), 3)
        self.assertEqual(l4.dataProvider().crs().postgisSrid(), 4326)

        n = 0
        r = QgsFeatureRequest( QgsRectangle(-1.677,49.624, -0.816,49.086) )
        for f in l4.getFeatures(r):
            self.assertEqual(f.geometry() is not None, True)
            self.assertEqual(f.attributes()[0], 2661)
            n += 1
        self.assertEqual(n, 1)

        # use uid
        query = QUrl.toPercentEncoding( "SELECT * FROM france_parts")
        l5 = QgsVectorLayer( "?query=%s&geometry=geometry:polygon:4326&uid=ObjectId" % query, "tt", "virtual" )
        self.assertEqual( l5.isValid(), True )

        idSum = sum(f.id() for f in l5.getFeatures())
        self.assertEqual( idSum, 10659 )

        r = QgsFeatureRequest( 2661 )
        idSum2 = sum(f.id() for f in l5.getFeatures(r))
        self.assertEqual( idSum2, 2661 )

        r = QgsFeatureRequest()
        r.setFilterFids( [ 2661, 2664 ] )
        self.assertEqual( sum(f.id() for f in l5.getFeatures(r)), 2661+2664)

        # test attribute subset
        r = QgsFeatureRequest()
        r.setFlags( QgsFeatureRequest.SubsetOfAttributes )
        r.setSubsetOfAttributes([1])
        s = [(f.id(), f.attributes()[0]) for f in l5.getFeatures(r)]
        self.assertEqual(sum(map(lambda x:x[0], s)), 10659)
        self.assertEqual(sum(map(lambda x:x[1], s)), 3064.0)

        # test NoGeometry
        # by request flag
        r = QgsFeatureRequest()
        r.setFlags( QgsFeatureRequest.NoGeometry )
        self.assertEqual(all([f.geometry() is None for f in l5.getFeatures(r)]), True)

        # test subset
        l5.setSubsetString( "ObjectId = 2661" )
        idSum2 = sum(f.id() for f in l5.getFeatures(r))
        self.assertEqual( idSum2, 2661 )

    def test_sql3(self):
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)

        # unnamed column
        query = QUrl.toPercentEncoding( "SELECT 42" )
        l4 = QgsVectorLayer( "?query=%s" % query, "tt", "virtual", False )
        self.assertEqual( l4.isValid(), False )
        self.assertEqual( "Result column #1 has no name" in l4.dataProvider().error().message(), True )

        query = QUrl.toPercentEncoding( "SELECT 42 as t, 'ok'||'ok' as t2, GeomFromText('') as t3" )
        l4 = QgsVectorLayer( "?query=%s" % query, "tt", "virtual", False )
        self.assertEqual( l4.isValid(), True )
        self.assertEqual( l4.dataProvider().fields().at(0).name(), "t" )
        self.assertEqual( l4.dataProvider().fields().at(0).type(), QVariant.Int )
        self.assertEqual( l4.dataProvider().fields().at(1).name(), "t2" )
        self.assertEqual( l4.dataProvider().fields().at(1).type(), QVariant.String )
        self.assertEqual( l4.dataProvider().fields().at(2).name(), "t3" )
        self.assertEqual( l4.dataProvider().fields().at(2).type(), QVariant.String )

        query = QUrl.toPercentEncoding( "SELECT GeomFromText('POINT(0 0)') as geom")
        l4 = QgsVectorLayer( "?query=%s&geometry=geom" % query, "tt", "virtual", False )
        self.assertEqual( l4.isValid(), False )
        self.assertEqual( "Can't deduce the geometry type of the geometry field" in l4.dataProvider().error().message(), True )

        query = QUrl.toPercentEncoding( "SELECT CastToPoint(GeomFromText('POINT(0 0)')) as geom")
        l4 = QgsVectorLayer( "?query=%s" % query, "tt", "virtual", False )
        self.assertEqual( l4.isValid(), True )
        self.assertEqual( l4.dataProvider().geometryType(), 1 )

    def test_sql4(self):
        l2 = QgsVectorLayer( os.path.join(self.testDataDir_, "france_parts.shp"), "france_parts", "ogr", False )
        self.assertEqual( l2.isValid(), True )
        QgsMapLayerRegistry.instance().addMapLayer(l2)
        print l2.id()

        print "list"
        for k,v in QgsMapLayerRegistry.instance().mapLayers().items():
            print k, v.name()

        query = QUrl.toPercentEncoding( "SELECT OBJECTId from france_parts" )
        l4 = QgsVectorLayer( "?query=%s" % query, "tt", "virtual", False )
        self.assertEqual( l4.isValid(), True )
        s = sum( f.attributes()[0] for f in l4.getFeatures() )
        self.assertEqual( s, 10659 )

if __name__ == '__main__':
    unittest.main()
