cmake_minimum_required(VERSION 2.6)
PROJECT( vlayer_plugin )

FIND_PACKAGE(Qt4 4.7.0 REQUIRED)

QT4_WRAP_CPP (vlayer_plugin_MOC_SRCS  vlayer_plugin.h)

QT4_WRAP_CPP (vlayer_provider_MOC_SRCS qgsvirtuallayerprovider.h)

QT4_ADD_RESOURCES(vlayer_plugin_RCC_SRCS vlayer_plugin.qrc)

ADD_LIBRARY(vlayer_plugin MODULE vlayer_plugin.cpp ${vlayer_plugin_RCC_SRCS} ${vlayer_plugin_MOC_SRCS} )

#ADD_LIBRARY(vlayer_ext MODULE vtable_ext.c vtable.cpp vtable.h)

LINK_DIRECTORIES(
  /usr/local/lib/qgis/plugins
)

ADD_LIBRARY(virtuallayerprovider MODULE ${vlayer_provider_MOC_SRCS} qgsvirtuallayerprovider.cpp vlayer_module.c vtable.cpp )

TARGET_LINK_LIBRARIES( virtuallayerprovider
  spatialiteprovider
)

ADD_DEFINITIONS( -DCORE_EXPORT= -DGUI_EXPORT= -std=c++11 -DQGIDDEBUG=1 -DQGIS_DEBUG=1)

INCLUDE_DIRECTORIES(
  ~/src/QGIS/src/providers
  ${QT_INCLUDE_DIR}
  ${QT_INCLUDE_DIR}/QtCore
  ${QT_INCLUDE_DIR}/QtGui
  ${QT_INCLUDE_DIR}/QtXml
  /usr/local/include/qgis
)

TARGET_LINK_LIBRARIES( vlayer_plugin
  qgis_core
  qgis_gui
)
