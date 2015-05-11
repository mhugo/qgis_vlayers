Virtual layers for QGIS
=======================

You can find here sources of an extension to QGIS that introduces "virtual layers" which are database views over existing QGIS (vector) layers.
These layers can be built by the use of the powerful SQL language and then allow for advanced uses in a consistent manner :
joins between different layers, including spatial joins, subset selection, type conversion, on-the-fly attribute and geometry generation, etc.

The current implementation is a functional prototype, designed as plugins for QGIS :

- an extension to Spatialite
- a new QGIS vector data provider
- a (C++) plugin for integration with GUI parts
- an extension to QGIS DBManager for virtual layers

Note :

These plugins have been designed for QGIS 2.8.
Support for filtering a layer that has a (QGIS) join defined needs a recent 2.9 version (at least from 02-18-2015 - commit number 4a52750a - nightly build 2.9.0-33)

Installation under Linux
------------------------

Use CMake to configure the project. Make QGIS_* variables point to your QGIS installation directories.
Example of configuration:
```
 QGIS_ANALYSIS_LIBRARY            /usr/local/lib/libqgis_analysis.so                                                 
 QGIS_APP_DIR                     /usr/local/share/qgis                                                              
 QGIS_CORE_LIBRARY                /usr/local/lib/libqgis_core.so                                                     
 QGIS_GUI_LIBRARY                 /usr/local/lib/libqgis_gui.so                                                      
 QGIS_INCLUDE_DIR                 /usr/local/include/qgis                                                            
 QGIS_PLUGIN_DIR                  /usr/local/lib/qgis/plugins                                                        
```

Then type `make` and `sudo make install`.

Installation under Windows
--------------------------

The compilation has been tested under VS2010.

Use CMake to configure the project with "NMake makefiles".

Pay attention to the CMAKE_BUILD_TYPE you use (Debug/Release or RelWithDebInfo). You cannot mix debug and release libraries under Windows. It has to be the same as the one used to compile QGIS.
Usually, if QGIS is installed with the osgeo installer, then set it to "RelWithDebInfo".

Make QGIS_* variables point to QGIS installation (or build) directories.

You will need Flex/Bison for the SQL parser. Make FLEX_EXECUTABLE and BISON_EXECUTABLE point to the executables used by the QGIS compilation (Flex coming for cygwin and bison from GnuWin32 in my case)

Type `nmake` to compile and `nmake install` to install.

Usage in QGIS
-------------

- You can create a new virtual layer through the "New layer" menu
- You can open a saved virtual layer through the "Add layer" menu
- A virtual layer can be created out of a layer selection by right click in the context menu
- If you want to filter a layer that has "joins", it will propose to copy it to a virtual layer (this is where the pull request is required)
- In DBManager, there is a new entry "virtual layers" where you can use an SQL query to setup a virtual layer

Videos :

* https://vimeo.com/123287075
* https://vimeo.com/123287077
* https://vimeo.com/123287076

Standalone Spatialite extension
-------------------------------

Virtual layers can also be created and queried directly using Spatialite (in command line or through other clients). The extension must be loaded with spatialite. The extension may need proper environment variables to be set. Under Linux, the variable `QGIS_PREFIX_PATH` must be set.

Session example :
```
# load spatialite with QGIS_PREFIX_PATH set
QGIS_PREFIX_PATH=/usr/local spatialite
```

And then, in spatialite
```SQL
-- load the extension
select load_extension('/usr/local/lib/qgis/plugins/libvirtuallayerprovider.so', 'qgsvlayer_module_init');
-- now, you can use the QgsVLayer module to create virtual tables
CREATE VIRTUAL TABLE "Table1" USING QgsVLayer(ogr,'poi.ods');
-- ... and query them
SELECT * FROM Table1
```

Provider syntax
===============

A new virtual layer can be created out of existing layers, possibly with a query. Every parameters are passed to the QgsVectorLayer constructor "source" argument as a string representing an URL with
a set of key-value pairs.

It can then be used directly either in Python or in C++.

To reference a layer, use the key `layer`. The value is a string with three fields separated by a colon : `provider:source[:name]`.

* The first field is the name of the QGIS provider to use
* The second field is the source string. If it contains special characters, and especially ":", "&", "?" or "=" it must be escaped with the percentage sign (see QUrl.toPercentEncoding)
* The third field is optional and is here to name the layer for reference in an SQL query. If no name is specified, each referenced layer will be named "vtabN" with an incrementing N integer.

A "live" layer already loaded by QGIS can also be referenced by using the key `layer_ref`. The corresponding value is `layer_id[:name]`.
Currently, layers referenced this way are not available from the creation UI. Internally, features are accessed by the provider, rather directly by the layer. This is a known limitation.

The `geometry` key is used to set the geometry column name, type and SRID. Its syntax is `column_name[:type:srid]`. The type field can be a string (point, linestring, etc.) or a QGis::WkbType.
SRID is an EPSG code.

A parameter allows to ignore any geometry column, resulting in an attribute-only layer : `nogeometry`

The `uid` key allows to specifiy which column must be used as an identifier for each feature. This is not mandatory. If no uid is specified, the underlying provider will autoincrement an integer for
each feature.

The `query` key allows to use an SQL query to setup the layer. It should also be escaped. Layer references are not strictly necessary. If the query uses names of existing QGIS layers (or their ID),
they will be automatically referenced. Name and type of the geometry column will also be detected.

Serialization
-------------

Virtual layers can be written on disk to reopen them later on.

When the source parameter contains a path and no other parameters, it is opened from disk.

When the source parameter contains a path and other parameters are set, the virtual layer is created as usual and saved into the given path.

SQL syntax
----------

The supported SQL syntax is the SQL of SQLite. The version shipped with QGIS. No recursive CTE for now.

SQLite and Spatialite functions are supported.

The internal SQL parser tries to detect types of columns (and especially the geometry column). "CAST" and geometry casts expressions are supported.

Examples
--------

Minimal syntax to set an SQL query out of an existing layer (Python syntax)

```python
l = QgsVectorLayer( "?query=SELECT a,b FROM tab", "myvlayer", "virtual" )
```

A join between two shapefiles where the uid and the geometry column are set :
```python
q = QUrl.toPercentEncoding("SELECT a.* FROM shp1 AS a, shp2 AS b WHERE Intersects(a.geometry,b.geometry)")
l = QgsVectorLayer( "?layer=ogr:/data/myshape.shp:shp1&layer=ogr:/data/myshape2.shp:shp2&query=%s&uid=id&geometry=geometry:2:4326" % q, "myjoin", "virtual" )
```

Minimal syntax to create a virtual layer on a single layer (equivalent to SELECT * FROM)
```python
l = QgsVectorLayer( "?layer=ogr:/data/myfile.shp", "myvlayer", "virtual" )
```

Creation of a virtual layer and storage in a file :
```python
l = QgsVectorLayer( "/myvlayer.sqlite?query=SELECT * FROM ta, tb", "myvlayer", "virtual" )
```

Opening of an existing virtual layer:
```python
l = QgsVectorLayer( "/myvlayer.sqlite", "myvlayer", "virtual" )
```

Index support
-------------

When the query uses a field from one of the referenced layer declared as a primary key, features are accessed via a QgsFeatureRequest with a "filterFid". It's then up to the underlying provider to support fast
access by ID.

For spatial indexes, a 'hidden' field named '_search_frame_' is created for each virtual table (i.e. each referenced layer of the virtual layer). The bounding box of the given geometry will be used
to restrain the query to a particular region of space. This bounding box is passed to the underlyng provider by a QgsFeatureRequest with a "filterRect".

So that use of spatial indexes must be explicit.

For example :
```SQL
SELECT * FROM pt, poly WHERE pt._search_frame_ = poly.geometry AND Intersects(pt.geometry, poly.geometry)
```

Known limitations / Future developments
---------------------------------------

* Virtual layers are read-only for now. For the simple case (only one layer referenced), implementing feature update and addition should not be complicated. For the more general case where a virtual layer
can be a join between layers, the concept of "updateable" views found in databases could be ported. It means it would require the user to specify triggers on UPDATE and INSERT.
* Creating a virtual layer with the `layer_ref` key allows to directly access already loaded QGIS layers (including memory layers). But internally, QgsDataProvider is used to access features, where QgsVectorLayer could be used, allowing to expose joins (and edit buffers) ?
* Spatial indexes must be set explicitly. The SQL parser code may be used in the future as a base for simple SQL transformations where use of geometry predicates (or bounding boxes operators like && in PostGIS) could be detected and spatial indexes added.
* QgsExpression functions are not supported in an SQL query. Addition of such a feature would be possible (since SQLite virtual table mechanism allows to add/overload functions).
