cmake_minimum_required(VERSION 2.6)
PROJECT( vlayer_plugin )

FIND_PACKAGE(Qt4 4.7.0 REQUIRED)

QT4_ADD_RESOURCES(vlayer_plugin_RCC_SRCS vlayer_plugin.qrc)

ADD_LIBRARY(vlayer_plugin MODULE vlayer_plugin.cpp ${vlayer_plugin_RCC_SRCS})

INCLUDE_DIRECTORIES(
  ${QT_INCLUDE_DIR}
  ${QT_INCLUDE_DIR}/QtCore
  ${QT_INCLUDE_DIR}/QtGui
  /usr/local/include/qgis
)

TARGET_LINK_LIBRARIES( vlayer_plugin
  qgis_core
  qgis_gui
)
