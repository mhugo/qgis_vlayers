#!/bin/sh
SCRIPTPATH=`dirname $0`
PYTHONPATH=/usr/local/share/qgis/python:${PYTHONPATH} QGIS_PREFIX_PATH=/usr/local python $SCRIPTPATH/test_qgsvirtuallayerprovider.py $*
