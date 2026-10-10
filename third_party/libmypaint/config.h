/* config.h do libmypaint para o Qenna Writer (compilado pelo CMake, sem autotools).
   Sem GLib, sem gettext, sem json-c (o .myb é lido pelo app). */
#ifndef QENNA_LIBMYPAINT_CONFIG_H
#define QENNA_LIBMYPAINT_CONFIG_H
#define MYPAINT_CONFIG_USE_GLIB 0
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define PACKAGE_STRING "libmypaint 1.6.1"
#define PACKAGE_VERSION "1.6.1"
#define GETTEXT_PACKAGE "libmypaint"
#endif
