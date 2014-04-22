import qgis
import apsw
from qgis.core import QgsMapLayerRegistry, QgsFeature    

class QgsVectorVTable:

    def Create(self, connection, modulename, dbname, tablename, *args):
        print "tablename", tablename
        self._conn = connection
        # args : list of qgis vector layer ids
        self._layerid = args[0]
        registry = QgsMapLayerRegistry.instance()
        self._layer = registry.mapLayers()[self._layerid]

        # returns a list with a "create table" statement and a VTable object
        pr = self._layer.dataProvider()

        type_map = { 'String' : 'TEXT',
                     'Integer' : 'INT',
                     'Real' : 'REAL'}

        fields = pr.fields()
        field_list = [ "%s %s" % (f.name(), type_map[f.typeName()]) for f in fields.toList() ]
#        if self._layer.hasGeometryType():
#            field_list += [ "geometry BLOB" ]
        stmt = "CREATE TABLE %s (%s)" % (tablename, ','.join(field_list) )
        print stmt
        return (stmt, self)

    Connect = Create

    def BestIndex( self, constraints, orderbys ):
        return None

    def Open( self ):
        return QgsVectorVCursor( self._conn, self._layer )

class QgsVectorVCursor:

    def __init__( self, conn, layer ):
        self._conn = conn
        self._layer = layer

        self._iterator = self._layer.dataProvider().getFeatures()
        self._has_geometry = self._layer.hasGeometryType()
        self._n_fields = len(self._layer.dataProvider().fields())
        self._eof = False
        self._feat = None

    def Close( self ):
        self._iterator.close()

    def Column( self, n ):
        # return current value at column #n
        if self._feat is None:
            return None

        if n == -1:
            return self.Rowid()

        # request for last column = geometry
        # does not work !
        #if self._has_geometry and n == self._n_fields:
        #    return QgsGeometryBlob(self._feat.geometry())

        return self._feat.attributes()[n]

    def Eof( self ):
        return self._eof

    def Filter( self, indexnum, indexname, constraintargs ):
        # get first row
        self.Next()

    def Next( self ):
        if self._feat is None:
            self._feat = QgsFeature()
        self._eof = not self._iterator.nextFeature( self._feat )

    def Rowid( self ):
        if self._feat is not None:
            return self._feat.id()
        return None

    
