# Hunspell.cmake — compila o Hunspell v1.7.2 como biblioteca estática.
#
# O Hunspell upstream usa autotools. Em vez de depender disso, montamos um
# alvo estático aqui mesmo a partir dos arquivos .cxx em src/hunspell/.
#
# A pasta third_party/hunspell/ não é commitada — clonar com:
#   git clone --depth 1 --branch v1.7.2 \
#     https://github.com/hunspell/hunspell.git mira-cpp/third_party/hunspell

set(HUNSPELL_ROOT "${CMAKE_SOURCE_DIR}/third_party/hunspell")
set(HUNSPELL_SRC "${HUNSPELL_ROOT}/src/hunspell")

if(NOT EXISTS "${HUNSPELL_SRC}/hunspell.cxx")
    message(FATAL_ERROR
        "Hunspell sources not found at ${HUNSPELL_SRC}.\n"
        "Clone with: git clone --depth 1 --branch v1.7.2 "
        "https://github.com/hunspell/hunspell.git third_party/hunspell")
endif()

# hunvisapi.h é gerado pelo configure no upstream. Como compilamos estático
# (HUNSPELL_STATIC), o conteúdo do header acaba trivial — só geramos um stub.
set(HUNSPELL_GEN_INC "${CMAKE_BINARY_DIR}/hunspell-gen/hunspell")
file(MAKE_DIRECTORY "${HUNSPELL_GEN_INC}")
file(WRITE "${HUNSPELL_GEN_INC}/hunvisapi.h"
"#ifndef HUNSPELL_VISIBILITY_H_\n"
"#define HUNSPELL_VISIBILITY_H_\n"
"#define LIBHUNSPELL_DLL_EXPORTED\n"
"#endif\n")

add_library(hunspell STATIC
    "${HUNSPELL_SRC}/affentry.cxx"
    "${HUNSPELL_SRC}/affixmgr.cxx"
    "${HUNSPELL_SRC}/csutil.cxx"
    "${HUNSPELL_SRC}/filemgr.cxx"
    "${HUNSPELL_SRC}/hashmgr.cxx"
    "${HUNSPELL_SRC}/hunspell.cxx"
    "${HUNSPELL_SRC}/hunzip.cxx"
    "${HUNSPELL_SRC}/phonet.cxx"
    "${HUNSPELL_SRC}/replist.cxx"
    "${HUNSPELL_SRC}/suggestmgr.cxx"
)

target_compile_definitions(hunspell PUBLIC
    HUNSPELL_STATIC
)
target_compile_definitions(hunspell PRIVATE
    BUILDING_LIBHUNSPELL
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)

# Os fontes incluem "hunvisapi.h" sem o prefixo "hunspell/", então a pasta
# src/hunspell/ precisa estar visível diretamente. A gen-inc traz só o stub.
target_include_directories(hunspell PUBLIC
    "${HUNSPELL_SRC}"
    "${HUNSPELL_GEN_INC}"
)

# MSVC: silenciar warnings barulhentos do hunspell (não vamos consertar
# código upstream). Não suprimimos no Mira em si.
if(MSVC)
    target_compile_options(hunspell PRIVATE
        /wd4267  # size_t -> int conversions
        /wd4244  # type narrowing
        /wd4018  # signed/unsigned mismatch
        /wd4146  # unary minus on unsigned
        /wd4101  # unreferenced local
        /wd4996  # deprecated POSIX names
    )
endif()
