set(intern_HEADERS
    src/intern/drw_cptable932.h
    src/intern/drw_cptable936.h
    src/intern/drw_cptable949.h
    src/intern/drw_cptable950.h
    src/intern/drw_cptables.h
    src/intern/drw_dbg.h
    src/intern/drw_textcodec.h
    src/intern/dwgbuffer.h
    src/intern/dwgreader15.h
    src/intern/dwgreader18.h
    src/intern/dwgreader21.h
    src/intern/dwgreader24.h
    src/intern/dwgreader27.h
    src/intern/dwgreader.h
    src/intern/dwgutil.h
    src/intern/dxfreader.h
    src/intern/dxfwriter.h
    src/intern/rscodec.h)

set(intern_SOURCES
    src/intern/drw_dbg.cpp
    src/intern/drw_textcodec.cpp
    src/intern/dwgbuffer.cpp
    src/intern/dwgreader15.cpp
    src/intern/dwgreader18.cpp
    src/intern/dwgreader21.cpp
    src/intern/dwgreader24.cpp
    src/intern/dwgreader27.cpp
    src/intern/dwgreader.cpp
    src/intern/dwgutil.cpp
    src/intern/dxfreader.cpp
    src/intern/dxfwriter.cpp
    src/intern/rscodec.cpp)

set(libdxfrw_HEADERS
    src/drw_base.h
    src/drw_interface.h
    src/drw_header.h
    src/drw_classes.h
    src/drw_entities.h
    src/drw_objects.h
    src/libdxfrw.h)

set(libdxfrw_SOURCES
    src/drw_header.cpp
    src/drw_classes.cpp
    src/drw_entities.cpp
    src/drw_objects.cpp
    src/libdxfrw.cpp)

set(libdwgr_HEADERS
    src/libdwgr.h)

set(libdwgr_SOURCES
    src/libdwgr.cpp)

add_library(dxfrw STATIC
    ${intern_HEADERS}
    ${intern_SOURCES}
    ${libdxfrw_HEADERS}
    ${libdxfrw_SOURCES}
    ${libdwgr_HEADERS}
    ${libdwgr_SOURCES})

target_include_directories(dxfrw PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/intern/)

target_include_directories(dxfrw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/)
