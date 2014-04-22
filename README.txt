Some experiments around SQLITE virtual tables for QGIS.

This is a very basic QGIS plugin that will try to create a SQLITE database where layers from QGIS can be accessed as a virtual table.

It uses apsw as for sqlite3 binding and virtual table support.
Unfortunately, it seems to be no way to write our own Blob class in Python. So, there is no way to have a virtual table with support for geometry. We would have to rely on a C/C++ implementation for that purpose.
