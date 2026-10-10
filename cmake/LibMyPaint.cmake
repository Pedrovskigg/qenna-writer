# LibMyPaint.cmake — compila o libmypaint 1.6.1 (ISC) como biblioteca estática.
# Motor dos pincéis da folha de Esboço da Lousa. Os fontes ficam em
# third_party/libmypaint (versionados; ver README-QENNA.md lá).

set(LIBMYPAINT_ROOT "${CMAKE_SOURCE_DIR}/third_party/libmypaint")

add_library(mypaint STATIC
    "${LIBMYPAINT_ROOT}/brushmodes.c"
    "${LIBMYPAINT_ROOT}/fifo.c"
    "${LIBMYPAINT_ROOT}/helpers.c"
    "${LIBMYPAINT_ROOT}/mypaint.c"
    "${LIBMYPAINT_ROOT}/mypaint-brush.c"
    "${LIBMYPAINT_ROOT}/mypaint-brush-settings.c"
    "${LIBMYPAINT_ROOT}/mypaint-fixed-tiled-surface.c"
    "${LIBMYPAINT_ROOT}/mypaint-mapping.c"
    "${LIBMYPAINT_ROOT}/mypaint-matrix.c"
    "${LIBMYPAINT_ROOT}/mypaint-rectangle.c"
    "${LIBMYPAINT_ROOT}/mypaint-surface.c"
    "${LIBMYPAINT_ROOT}/mypaint-symmetry.c"
    "${LIBMYPAINT_ROOT}/mypaint-tiled-surface.c"
    "${LIBMYPAINT_ROOT}/operationqueue.c"
    "${LIBMYPAINT_ROOT}/rng-double.c"
    "${LIBMYPAINT_ROOT}/tilemap.c"
)
set_target_properties(mypaint PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(mypaint PRIVATE QENNA_NO_JSONC HAVE_CONFIG_H)
target_include_directories(mypaint PUBLIC "${LIBMYPAINT_ROOT}")
# Código de terceiros: sem consertar os avisos dele.
if(NOT MSVC)
    target_compile_options(mypaint PRIVATE -w)
endif()
if(NOT WIN32)
    target_link_libraries(mypaint PUBLIC m)
endif()
