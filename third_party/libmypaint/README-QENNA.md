# libmypaint 1.6.1 (no Qenna Writer)

Motor de pincéis do MyPaint, usado pela folha de Esboço da Lousa. Licença ISC (ver `COPYING`).
Fonte: https://github.com/mypaint/libmypaint/releases/tag/v1.6.1

Só os `.c`/`.h` da raiz e `fastapprox/` do tarball oficial. Mudanças do Qenna:

- `config.h` próprio (sem autotools): sem GLib, sem gettext.
- `mypaint-brush.c`: a leitura de `.myb` via json-c fica atrás de `#ifndef QENNA_NO_JSONC`.
  O app lê o `.myb` com o Qt e passa as configurações com `mypaint_brush_set_base_value`
  e `mypaint_brush_set_mapping_*`. Com `QENNA_NO_JSONC`, `mypaint_brush_from_string`
  devolve FALSE.

Compilado por `cmake/LibMyPaint.cmake`.
