#!/bin/sh
SCRIPTPATH=`dirname $0`
QGIS_PREFIX_PATH=/usr/local python $SCRIPTPATH/test_qgsvirtuallayerprovider.py $*
