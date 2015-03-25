cmake_minimum_required(VERSION 2.6)
PROJECT( vlayer_plugin )

#############################################################

# set path to additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

FIND_PACKAGE(QGIS REQUIRED)

FIND_PACKAGE(Qt4 4.7.0 REQUIRED)
FIND_PACKAGE(Sqlite3 REQUIRED)
FIND_PACKAGE(SPATIALITE REQUIRED)

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
# enable use of c++11 features where available
# full c++11 support in clang 3.3+: http://clang.llvm.org/cxx_status.html
# for Mac, this is probably Apple LLVM 4.2 (based on LLVM 3.2svn, in XCode 4.6+)
#   or definitely Apple LLVM 5.0 (based on LLVM 3.3svn, in Xcode 5+):
#   https://gist.github.com/yamaya/2924292

IF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  EXECUTE_PROCESS(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
  IF (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
    SET(USE_CXX_11 TRUE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  ENDIF()
ELSEIF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  IF ((NOT APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.2")
       OR (APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.1"))
    SET(USE_CXX_11 TRUE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-error=c++11-narrowing")
  ENDIF()
ELSE()
  SET(USE_CXX_11 FALSE)
ENDIF()

#allow override keyword if available
IF (NOT USE_CXX_11)
  ADD_DEFINITIONS("-Doverride=")
ENDIF()

#############################################################
# enable warnings

IF (PEDANTIC)
  MESSAGE (STATUS "Pedantic compiler settings enabled")
  IF(MSVC)
    IF (NOT USING_NMAKE)
      ADD_DEFINITIONS( /W4 )
    ENDIF (NOT USING_NMAKE)

    # disable warnings
    ADD_DEFINITIONS( /wd4100 )  # unused formal parameters
    ADD_DEFINITIONS( /wd4127 )  # constant conditional expressions (used in Qt template classes)
    ADD_DEFINITIONS( /wd4231 )  # nonstandard extension used : 'identifier' before template explicit instantiation (used in Qt template classes)
    ADD_DEFINITIONS( /wd4244 )  # conversion from '...' to '...' possible loss of data
    ADD_DEFINITIONS( /wd4251 )  # needs to have dll-interface to be used by clients of class (occurs in Qt template classes)
    ADD_DEFINITIONS( /wd4275 )  # non dll-interface class '...' used as base for dll-interface class '...'
    ADD_DEFINITIONS( /wd4505 )  # unreferenced local function has been removed (QgsRasterDataProvider::extent)
    ADD_DEFINITIONS( /wd4510 )  # default constructor could not be generated (sqlite3_index_info, QMap)
    ADD_DEFINITIONS( /wd4512 )  # assignment operator could not be generated (sqlite3_index_info)
    ADD_DEFINITIONS( /wd4610 )  # user defined constructor required (sqlite3_index_info)
    ADD_DEFINITIONS( /wd4706 )  # assignment within conditional expression (pal)
    ADD_DEFINITIONS( /wd4800 )  # 'int' : forcing value to bool 'true' or 'false' (performance warning)
  ELSE (MSVC)
    # add warnings via flags (not as definitions as on Mac -Wall can not be overridden per language )
    SET(_warnings "-Wall -Wextra -Wno-long-long -Wformat-security -Wno-strict-aliasing")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_warnings}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_warnings}")

    # Qt produces lots of warnings with strict aliasing (as of Qt 4.4.0 & GCC 4.3)
    # There are redundant declarations in Qt and GDAL
    # ADD_DEFINITIONS( -fstrict-aliasing -Wstrict-aliasing=1 -Wredundant-decls )

    IF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-type-c-linkage -Wno-overloaded-virtual")
    ENDIF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

    # add any extra CXXFLAGS flags set by user. can be -D CXX_EXTRA_FLAGS or environment variable
    # command line -D option overrides environment variable
    # e.g. useful for suppressing transient upstream warnings in dependencies, like Qt
    SET(CXX_EXTRA_FLAGS "" CACHE STRING "Additional appended CXXFLAGS")
    IF ("${CXX_EXTRA_FLAGS}" STREQUAL "" AND DEFINED $ENV{CXX_EXTRA_FLAGS})
      SET(CXX_EXTRA_FLAGS "$ENV{CXX_EXTRA_FLAGS}")
    ENDIF ("${CXX_EXTRA_FLAGS}" STREQUAL "" AND DEFINED $ENV{CXX_EXTRA_FLAGS})
    IF (NOT "${CXX_EXTRA_FLAGS}" STREQUAL "")
      MESSAGE (STATUS "Appending CXX_EXTRA_FLAGS")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_EXTRA_FLAGS}")
    ENDIF (NOT "${CXX_EXTRA_FLAGS}" STREQUAL "")
  ENDIF (MSVC)

ENDIF (PEDANTIC)

IF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Qunused-arguments")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Qunused-arguments")
  SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Qunused-arguments")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Qunused-arguments")
ENDIF ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

IF(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")
  # spatialite crashes on ppc - see bugs.debian.org/603986
  ADD_DEFINITIONS( -fno-strict-aliasing )
ENDIF(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)")

IF (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
  MESSAGE (STATUS "Debug output enabled")
  ADD_DEFINITIONS(-DQGISDEBUG=1)
ENDIF (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

IF(MSVC)
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DQGISDEBUG=1")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DQGISDEBUG=1")
  # disable macros that offend std::numeric_limits<T>::min()/max()
  ADD_DEFINITIONS(-DNOMINMAX)
ENDIF(MSVC)

#############################################################
# platform specific stuff

IF (WIN32)
  SET (DEFAULT_LIB_SUBDIR     lib)
  SET (DEFAULT_LIBEXEC_SUBDIR .)
  SET (DEFAULT_DATA_SUBDIR    .)
  SET (DEFAULT_PLUGIN_SUBDIR  plugins)
  SET (DEFAULT_INCLUDE_SUBDIR include)

  IF (MSVC)
    SET (DEFAULT_BIN_SUBDIR bin)
    SET (DEFAULT_CGIBIN_SUBDIR bin)
    # put all the build products into a single directory
    # under build (doesnt affect install target) to make for
    # easier debugging.

    # Turn on defines for non standard maths stuff
    ADD_DEFINITIONS(-D_USE_MATH_DEFINES)

    # Turn off deprecation warnings
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_WARNINGS)

    IF (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
      IF (NOT USING_NMAKE)
        MESSAGE (STATUS "Generating browse files")
        ADD_DEFINITIONS( /FR )
      ENDIF (NOT USING_NMAKE)
    ENDIF (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

    IF (INSTALL_DEPS)
      INSTALL(DIRECTORY ${INSTALL_DEPS} DESTINATION .)
    ENDIF (INSTALL_DEPS)
  ELSE(MSVC)
    SET (DEFAULT_BIN_SUBDIR .)
    SET (DEFAULT_CGIBIN_SUBDIR .)
  ENDIF(MSVC)
ELSE (WIN32)

  IF (APPLE)
    IF (POLICY CMP0042) # in CMake 3.0.0+
      SET (CMAKE_MACOSX_RPATH OFF) # otherwise ON by default
    ENDIF (POLICY CMP0042)
    # for Mac OS X, everything is put inside an application bundle
    # save the root install prefix for the app later
    SET (QGIS_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
    SET (QGIS_MACAPP_PREFIX ${CMAKE_INSTALL_PREFIX}/${QGIS_APP_NAME}.app/Contents)
    # common prefix for components, let cmake handle it
    SET (CMAKE_INSTALL_PREFIX ${QGIS_MACAPP_PREFIX}/MacOS)
    # 4 bundling levels, each includes previous
    #   0  nothing
    #   1  Qt frameworks
    #   2  non-system libraries, "standard"
    #   3  non-system frameworks, "standalone"
    SET (QGIS_MACAPP_BUNDLE 1 CACHE STRING "What to bundle into app package")
    SET (QGIS_MACAPP_BUNDLE_USER "" CACHE STRING "Path to user bundling script")
    SET (QGIS_MACAPP_INSTALL_DEV FALSE CACHE BOOL "Install developer frameworks")
    SET (QGIS_MACAPP_DEV_PREFIX "/Library/Frameworks" CACHE STRING "Path to install developer frameworks")
    # if we have @loader_path, >=OSX 10.5 (darwin 9+)
    IF (CMAKE_SYSTEM_VERSION VERSION_GREATER 9.0.0 OR CMAKE_SYSTEM_VERSION VERSION_EQUAL 9.0.0)
        SET (OSX_HAVE_LOADERPATH 1)
    ELSE ()
        SET (OSX_HAVE_LOADERPATH 0)
    ENDIF ()
    #this will define ${APP_SERVICES_LIBRARY}
    FIND_LIBRARY(APP_SERVICES_LIBRARY ApplicationServices )

    SET (DEFAULT_BIN_SUBDIR      bin)
    SET (QGIS_BIN_SUBDIR_REV     ..)
    SET (DEFAULT_CGIBIN_SUBDIR   fcgi-bin)
    SET (QGIS_CGIBIN_SUBDIR_REV  ..)
    SET (DEFAULT_LIB_SUBDIR      lib)
    SET (QGIS_LIB_SUBDIR_REV     ..)
    SET (QGIS_FW_SUBDIR          ../Frameworks)
    SET (QGIS_FW_SUBDIR_REV      ../MacOS)
    SET (DEFAULT_DATA_SUBDIR     ../Resources)
    SET (QGIS_DATA_SUBDIR_REV    ../MacOS)
    SET (DEFAULT_LIBEXEC_SUBDIR  lib/qgis)
    SET (QGIS_LIBEXEC_SUBDIR_REV ../..)
    SET (DEFAULT_PLUGIN_SUBDIR   ../PlugIns/qgis)
    SET (QGIS_PLUGIN_SUBDIR_REV  ../../MacOS)
    SET (DEFAULT_INCLUDE_SUBDIR  include/qgis)
    # path for framework references when running from build directory
    # changed later to reference in-app resources upon install
    SET (CMAKE_INSTALL_NAME_DIR ${CMAKE_BINARY_DIR}/output/lib)
    IF (WITH_GLOBE)
      SET (OSG_PLUGINS_PATH "" CACHE PATH "Path to OSG plugins for bundling")
    ENDIF (WITH_GLOBE)
    # recent cmakes force SDKs, recent SDKs don't have user symlinks
    # need to find non-system frameworks
    # cmake bug #0007250 - CMAKE_SHARED_LINKER_FLAGS ignored when creating
    # a framework, so these need to be manually handled with LINK_FLAGS options
    SET (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -F/Library/Frameworks")
    SET (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -F/Library/Frameworks")
    SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -F/Library/Frameworks")

  ELSE (APPLE)
    # UNIX
    SET (DEFAULT_BIN_SUBDIR     bin)
    SET (DEFAULT_CGIBIN_SUBDIR  bin)
    SET (DEFAULT_LIB_SUBDIR     lib${LIB_SUFFIX})
    SET (DEFAULT_DATA_SUBDIR    share/qgis)
    SET (DEFAULT_LIBEXEC_SUBDIR lib${LIB_SUFFIX}/qgis)
    SET (DEFAULT_PLUGIN_SUBDIR  lib${LIB_SUFFIX}/qgis/plugins)
    SET (DEFAULT_INCLUDE_SUBDIR include/qgis)
  ENDIF (APPLE)

ENDIF (WIN32)

#assume we have escaped compiler directives
#eventually we want to change this to new
#since we don't need to jump through so many
#hoops to escape compiler directives then
IF(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0005 OLD)
ENDIF(COMMAND cmake_policy)

IF (WIN32)
  SET(DLLIMPORT "__declspec(dllimport)")
  SET(DLLEXPORT "__declspec(dllexport)")
ELSE (WIN32)
  IF(PEDANTIC AND NOT APPLE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
    SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
  ENDIF(PEDANTIC AND NOT APPLE)

  SET(DLLIMPORT "")
  SET(DLLEXPORT "")
ENDIF(WIN32)

ADD_DEFINITIONS("-DCORE_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DGUI_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DPYTHON_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DANALYSIS_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DAPP_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DCUSTOMWIDGETS_EXPORT=${DLLIMPORT}")
ADD_DEFINITIONS("-DSERVER_EXPORT=${DLLIMPORT}")

######################################

LINK_DIRECTORIES(
  ${QGIS_PLUGIN_DIR}
)

INCLUDE_DIRECTORIES(
  ${QT_INCLUDE_DIR}
  ${QT_INCLUDE_DIR}/QtCore
  ${QT_INCLUDE_DIR}/QtGui
  ${QT_INCLUDE_DIR}/QtXml
  ${QGIS_INCLUDE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${SQLITE3_INCLUDE_DIR}
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
  qgsvirtuallayerfeatureiterator.cpp
  qgsvirtuallayersourceselect.cpp
  qgsembeddedlayerselectdialog.cpp
  vlayer_module.cpp
  qgsvirtuallayerdefinition.cpp
  qgssql.cpp
)

ADD_FLEX_FILES(QGIS_VLAYER_PROVIDER_SRCS qgssqllexer.ll)

ADD_BISON_FILES(QGIS_VLAYER_PROVIDER_SRCS qgssqlparser.yy)

ADD_LIBRARY(virtuallayerprovider MODULE
  ${QGIS_VLAYER_PROVIDER_SRCS}
)

TARGET_LINK_LIBRARIES( virtuallayerprovider
  ${QGIS_CORE_LIBRARY}
  ${QGIS_GUI_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${SQLITE3_LIBRARY}
  ${SPATIALITE_LIBRARY}
)

INSTALL(TARGETS virtuallayerprovider
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
  )

#############################################################
#    Virtual layer plugin
#############################################################

QT4_WRAP_CPP (vlayer_plugin_MOC_SRCS  vlayer_plugin.h)

QT4_ADD_RESOURCES(vlayer_plugin_RCC_SRCS vlayer_plugin.qrc)

ADD_LIBRARY(vlayer_plugin MODULE vlayer_plugin.cpp ${vlayer_plugin_RCC_SRCS} ${vlayer_plugin_MOC_SRCS} )

TARGET_LINK_LIBRARIES( vlayer_plugin
  ${QGIS_CORE_LIBRARY}
  ${QGIS_GUI_LIBRARY}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTGUI_LIBRARY}
  ${QT_QTXML_LIBRARY}
)


INSTALL(TARGETS vlayer_plugin
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR}
  )

############################################################
# Test
############################################################

SET(TEST_PARSER_SRCS
  qgssql.cpp
  test_parser.cpp
)
ADD_FLEX_FILES(TEST_PARSER_SRCS qgssqllexer.ll)
ADD_BISON_FILES(TEST_PARSER_SRCS qgssqlparser.yy)

ADD_EXECUTABLE( test_parser ${TEST_PARSER_SRCS} )
SET_TARGET_PROPERTIES( test_parser PROPERTIES AUTOMOC TRUE)

TARGET_LINK_LIBRARIES( test_parser
  ${QT_QTCORE_LIBRARY}
  ${QT_QTTEST_LIBRARY}
  ${QGIS_CORE_LIBRARY}
  ${QGIS_GUI_LIBRARY}
)

############################################################
# DB manager integration
############################################################

FIND_PACKAGE(PyQt4 REQUIRED)
INCLUDE(PyQt4Macros)

ADD_SUBDIRECTORY(db_manager_vlayers)

