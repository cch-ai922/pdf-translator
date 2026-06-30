cmake_minimum_required(VERSION 3.5)
project(PDFTranView)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.5 REQUIRED COMPONENTS Core Gui Widgets)

# ── MuPDF ──────────────────────────────────────────────────────────────────
# Set MUPDF_DIR to the root of your MuPDF installation, e.g.:
#   cmake -DMUPDF_DIR=C:/mupdf ..
# The tree expected:
#   $MUPDF_DIR/include/mupdf/fitz.h
#   $MUPDF_DIR/lib/libmupdf.a  (or mupdf.lib on MSVC)
if(NOT MUPDF_DIR)
    # Auto-detect: MSYS2/MinGW64 install (C:/msys64/mingw64)
    find_path(MUPDF_INCLUDE mupdf/fitz.h
        HINTS
            "C:/msys64/mingw64/include"
            "C:/msys2/mingw64/include"
            /usr/include
            /usr/local/include
    )
    find_library(MUPDF_LIB NAMES mupdf libmupdf
        HINTS
            "C:/msys64/mingw64/lib"
            "C:/msys2/mingw64/lib"
            /usr/lib
            /usr/local/lib
    )
else()
    set(MUPDF_INCLUDE "${MUPDF_DIR}/include")
    if(WIN32)
        find_library(MUPDF_LIB NAMES mupdf libmupdf
            HINTS "${MUPDF_DIR}/lib" "${MUPDF_DIR}/Release" "${MUPDF_DIR}/Debug"
        )
    else()
        find_library(MUPDF_LIB NAMES mupdf
            HINTS "${MUPDF_DIR}/lib"
        )
    endif()
endif()

if(NOT MUPDF_INCLUDE OR NOT MUPDF_LIB)
    message(FATAL_ERROR
        "MuPDF not found. Pass -DMUPDF_DIR=<path> pointing to a MuPDF install "
        "(with include/ and lib/ sub-directories).")
endif()
message(STATUS "MuPDF include : ${MUPDF_INCLUDE}")
message(STATUS "MuPDF lib     : ${MUPDF_LIB}")

# ── Sources ────────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    mainwindow.cpp
    pdfdocument.cpp
    pdfview.cpp
)

set(HEADERS
    mainwindow.h
    pdfdocument.h
    pdfview.h
)

if(WIN32)
    set(APP_TYPE WIN32)
endif()

add_executable(PDFTranView ${APP_TYPE} ${SOURCES} ${HEADERS})

target_include_directories(PDFTranView PRIVATE ${MUPDF_INCLUDE})

target_link_libraries(PDFTranView PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
    ${MUPDF_LIB}
)

# MuPDF 1.27 static build pulls in many sub-libraries.
# When using MSYS2/MinGW64 packages, pkg-config gives us the full list.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MUPDF_PC QUIET mupdf)
    if(MUPDF_PC_FOUND)
        target_link_libraries(PDFTranView PRIVATE ${MUPDF_PC_STATIC_LIBRARIES})
    endif()
endif()

if(WIN32)
    target_link_libraries(PDFTranView PRIVATE
        gdi32 user32 comdlg32 shell32 ole32 uuid winspool
        # MinGW static runtime (avoids MSVC CRT dependency)
        -static-libgcc -static-libstdc++
    )
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(PDFTranView PRIVATE m pthread dl)
endif()

# Copy PDFTranEngine next to the executable if it exists (non-fatal)
set(ENGINE_SRC "${CMAKE_SOURCE_DIR}/../PDFTranEngine/dist/PDFTranEngine.exe")
add_custom_command(TARGET PDFTranView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DSRC=${ENGINE_SRC}
        -DDST=$<TARGET_FILE_DIR:PDFTranView>/PDFTranEngine.exe
        -P "${CMAKE_CURRENT_LIST_DIR}/copy_if_exists.cmake"
    COMMENT "Copying PDFTranEngine (skipped if not yet built)"
    VERBATIM
)
