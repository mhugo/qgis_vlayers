cmake_minimum_required(VERSION 2.6)
PROJECT( vlayer_plugin )

#############################################################

# set path to additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

FIND_PACKAGE(Qt4 4.7.0 REQUIRED)

INCLUDE(Flex)

FIND_FLEX()

IF (NOT FLEX_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Couldn't find Flex")
ENDIF (NOT FLEX_EXECUTABLE)

INCLUDE(Bison)

FIND_BISON()

IF (NOT BISON_EXECUTABLE)
  MESSAGE(FATAL_ERROR "Couldn't find Bison")
ENDIF (NOT BISON_EXECUTABLE)

#############################################################

LINK_DIRECTORIES(
  /usr/local/lib/qgis/plugins
)

ADD_DEFINITIONS( -DCORE_EXPORT= -DGUI_EXPORT= -std=c++11 -DQGISDEBUG=1 -DQGIS_DEBUG=1)

INCLUDE_DIRECTORIES(
  ~/src/QGIS/src/providers
  ${QT_INCLUDE_DIR}
  ${QT_INCLUDE_DIR}/QtCore
  ${QT_INCLUDE_DIR}/QtGui
  ${QT_INCLUDE_DIR}/QtXml
  /usr/local/include/qgis
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
)

#############################################################
#    Virtual layer provider
#############################################################

QT4_WRAP_CPP(vlayer_provider_MOC_SRCS qgsvirtuallayerprovider.h qgsvirtuallayersourceselect.h qgsembeddedlayerselectdialog.h)

QT4_WRAP_UI(vlayer_provider_UI_H qgsvirtuallayersourceselectbase.ui qgsembeddedlayerselect.ui)

SET(QGIS_VLAYER_PROVIDER_SRCS
  ${vlayer_provider_MOC_SRCS}
  ${vlayer_provider_UI_H}
  qgsvirtuallayerprovider.cpp
  qgsvirtuallayersourceselect.cpp
  qgsembeddedlayerselectdialog.cpp
  vlayer_module.cpp
  qgsvirtuallayerdefinition.cpp
)

ADD_FLEX_FILES(QGIS_VLAYER_PROVIDER_SRCS qgssqllexer.ll)

ADD_BISON_FILES(QGIS_VLAYER_PROVIDER_SRCS qgssqlparser.yy)

ADD_LIBRARY(virtuallayerprovider MODULE
  ${QGIS_VLAYER_PROVIDER_SRCS}
)

TARGET_LINK_LIBRARIES( virtuallayerprovider
  spatialiteprovider
)

#############################################################
#    Virtual layer plugin
#############################################################

QT4_WRAP_CPP (vlayer_plugin_MOC_SRCS  vlayer_plugin.h)

QT4_ADD_RESOURCES(vlayer_plugin_RCC_SRCS vlayer_plugin.qrc)

ADD_LIBRARY(vlayer_plugin MODULE vlayer_plugin.cpp ${vlayer_plugin_RCC_SRCS} ${vlayer_plugin_MOC_SRCS} )

TARGET_LINK_LIBRARIES( vlayer_plugin
  qgis_core
  qgis_gui
)


############################################################
# Test
############################################################

SET(TEST_PARSER_SRCS
  test_parser.cpp
)
ADD_FLEX_FILES(TEST_PARSER_SRCS qgssqllexer.ll)
ADD_BISON_FILES(TEST_PARSER_SRCS qgssqlparser.yy)

ADD_EXECUTABLE( test_parser ${TEST_PARSER_SRCS} )
SET_TARGET_PROPERTIES( test_parser PROPERTIES AUTOMOC TRUE)

TARGET_LINK_LIBRARIES( test_parser
  ${QT_QTCORE_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  qgis_core
  qgis_gui
)



