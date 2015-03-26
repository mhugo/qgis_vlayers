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
To fully benefit the QGIS integration, the following pull request must be merged (https://github.com/qgis/QGIS/pull/1964).
In the meantime, you should use my branch (https://github.com/mhugo/QGIS/tree/fix_querybuilder_joins2)

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


Usage in  QGIS
--------------

- You can create a new virtual layer through the "New layer" menu
- You can open a saved virtual layer through the "Add layer" menu
- A virtual layer can be created out of a layer selection by right click in the context menu
- If you want to filter a layer that has "joins", it will propose to copy it to a virtual layer (this is where the pull request is required)
- In DBManager, there is a new entry "virtual layers" where you can use SQL query to setup a virtual layer

Standalone Spatialite extension
-------------------------------

Virtual layers can also be created and queried directly using Spatialite (in command line or through other clients). The extension must be loaded with spatialite. The extension may need proper environment variables to be set. Under Linux, the variable `QGIS_PREFIX_PATH` must be set.

Session example :
```
# load spatialite with QGIS_PREFIX_PATH set
QGIS_PREFIX_PATH=/usr/local spatialite
# load the extension
select load_extension('/usr/local/lib/qgis/plugins/libvirtuallayerprovider.so', 'qgsvlayer_module_init');
# now, you can use the QgsVLayer module to create virtual tables
CREATE VIRTUAL TABLE "Table1" USING QgsVLayer(ogr,'poi.ods');
# ... and query them
SELECT * FROM Table1
```
