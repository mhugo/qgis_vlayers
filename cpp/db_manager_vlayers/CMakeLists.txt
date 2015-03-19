
FILE(GLOB PY_FILES *.py)

PYQT4_ADD_RESOURCES(PYRC_FILES resources.qrc)

INSTALL(FILES ${PY_FILES} ${PYRC_FILES} DESTINATION ${QGIS_DATA_DIR}/python/plugins/db_manager/db_plugins/vlayers)

