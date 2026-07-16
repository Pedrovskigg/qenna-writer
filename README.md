# Qenna Writer

Qenna Writer (formerly Mira Writing, briefly Qiyva Writer) is a complete
C++/Qt rebuild of the original Mira Writing (JSX) — same project, new name
and identity.

This repository holds the **source code** of the application. For
downloads of the releases, go to:
https://github.com/Pedrovskigg/Qenna-Writer-releases

---

Qenna Writer is a text editor and multi-document manager built entirely for
the craft of creative writing — drawers for characters, settings and
objects, multi-manuscript projects, a visual Board for plotting, a Timeline,
a world-building tool (the Builder), writing goals and stats, and more, all
wrapped in a fully themeable editor with Focus Mode.

Qenna Writer is free — no subscriptions, no paywalled features. Made by one
writer, for other writers.

## Tech stack

- C++17, Qt 6.8 (Core, Gui, Widgets, Svg, Multimedia, Network)
- CMake + Ninja, MinGW-w64 (GCC) on Windows
- [Hunspell](https://github.com/hunspell/hunspell) for spell-checking

## Building from source (Windows)

1. Install [Qt 6.8+](https://www.qt.io/download-open-source) (MinGW 64-bit
   kit) and its bundled CMake/Ninja/MinGW under Qt Maintenance Tool, or your
   own CMake 3.16+ / Ninja / MinGW-w64 toolchain.
2. Clone Hunspell v1.7.2 into `third_party/hunspell` (not vendored in this
   repo — see [cmake/Hunspell.cmake](cmake/Hunspell.cmake)):
   ```powershell
   git clone --depth 1 --branch v1.7.2 https://github.com/hunspell/hunspell.git third_party/hunspell
   ```
3. Configure and build:
   ```powershell
   $env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH
   & "C:\Qt\6.8.3\mingw_64\bin\qt-cmake.bat" -G Ninja -S . -B build
   cmake --build build
   ```
4. Run it:
   ```powershell
   $env:PATH = "C:\Qt\6.8.3\mingw_64\bin;" + $env:PATH
   & .\build\qenna-writer.exe
   ```

The "Immersive Sound" ambient tracks (`src/assets/ambience-sounds/`) aren't
included in this repo — the app builds and runs fine without them, just
without that one feature's audio files.

## Translations

Qenna Writer is currently available in PT-BR, EN-US, and ES, via Qt
Linguist (`translations/*.ts`). Language switches at runtime through the
Main Menu.

## License

Qenna Writer is open for reading, modification, and redistribution.
However, any and all redistribution of the code must remain free of
charge and open. See the full license here: [LICENSE](LICENSE).

(GPL-3.0, supplemented by a Commons Clause condition and a mandatory
author-attribution term — releases published before 2026-07-15 remain
under plain GPL-3.0, as published at the time.)

Third-party components (Qt, Hunspell, FFmpeg, bundled fonts, etc.)
keep their own licenses — see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Related projects

- [Cover Creator](https://github.com/Pedrovskigg/qenna-cover-creator) —
  cover design tool bundled with Qenna Writer
- [Mira's Maestro](https://github.com/Pedrovskigg/Maestro) — free musical
  theory plugin

## Contact

To report bugs, suggest new features or just scream about your plot at
3:00 AM: `qennawriter@gmail.com`
I can take a few hours or days to answer, but I will! I promise!
Or you can reach me on GitHub as well, on Qenna's repository.

— P.H. Lobato
