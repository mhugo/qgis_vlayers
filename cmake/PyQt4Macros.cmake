IF(PYUIC4BINARY AND PYRCC4BINARY)
  # Already in cache, be silent
  set(PyQt4_FIND_QUIETLY TRUE)
ENDIF(PYUIC4BINARY AND PYRCC4BINARY)

FIND_PROGRAM(PYUIC4BINARY pyuic4)
FIND_PROGRAM(PYRCC4BINARY pyrcc4)

MACRO(PYQT4_WRAP_UI outfiles)
  FOREACH(it ${ARGN})
    GET_FILENAME_COMPONENT(outfile ${it} NAME_WE)
    GET_FILENAME_COMPONENT(infile ${it} ABSOLUTE)
    SET(outfile2 ${CMAKE_CURRENT_BINARY_DIR}/ui_${outfile}.py)
    STRING(REGEX REPLACE "[:/]" "_" tname ${outfile2})
    ADD_CUSTOM_TARGET(${tname} ALL
      DEPENDS ${outfile2}
    )
    ADD_CUSTOM_COMMAND(OUTPUT ${outfile2}
      COMMAND ${PYUIC4BINARY} ${infile} -o ${outfile2}
      MAIN_DEPENDENCY ${infile}
    )
    SET(${outfiles} ${${outfiles}} ${outfile2})
  ENDFOREACH(it)
ENDMACRO (PYQT4_WRAP_UI)

MACRO(PYQT4_ADD_RESOURCES outfiles)
  FOREACH(it ${ARGN})
    GET_FILENAME_COMPONENT(outfile ${it} NAME_WE)
    GET_FILENAME_COMPONENT(infile ${it} ABSOLUTE)
    SET(outfile2 ${CMAKE_CURRENT_BINARY_DIR}/${outfile}_rc.py)
    STRING(REGEX REPLACE "[:/]" "_" tname ${outfile2})
    ADD_CUSTOM_TARGET(${tname} ALL
      DEPENDS ${outfile2}
    )
    ADD_CUSTOM_COMMAND(OUTPUT ${outfile2}
      COMMAND ${PYRCC4BINARY} ${infile} -o ${outfile2}
      MAIN_DEPENDENCY ${infile}
    )
    SET(${outfiles} ${${outfiles}} ${outfile2})
  ENDFOREACH(it)
ENDMACRO (PYQT4_ADD_RESOURCES)

IF(EXISTS ${PYUIC4BINARY} AND EXISTS ${PYRCC4BINARY})
   set(PyQt4_FOUND TRUE)
ENDIF(EXISTS ${PYUIC4BINARY} AND EXISTS ${PYRCC4BINARY})

if(PyQt4_FOUND)
  if(NOT PyQt4_FIND_QUIETLY)
    message(STATUS "Found PyQt4: ${PYUIC4BINARY}, ${PYRCC4BINARY}")
  endif(NOT PyQt4_FIND_QUIETLY)
endif(PyQt4_FOUND)
