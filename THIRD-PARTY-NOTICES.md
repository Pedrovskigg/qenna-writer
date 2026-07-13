# Third-Party Notices

Qenna Writer (this repository) is licensed under the GNU General Public
License v3.0 — see [LICENSE](LICENSE). It also uses the third-party
components listed below, each under its own license. Full license texts are
in the [`licenses/`](licenses/) folder except where noted.

## Qt 6.8.3

Modules used: Core, Gui, Widgets, Svg, Multimedia, Network.

- **License:** GNU Lesser General Public License v3.0 (LGPL-3.0) — official
  open-source Qt build.
- **Usage:** Unmodified, dynamically linked. The Qt `.dll` files ship
  alongside `qenna-writer.exe` in the installed folder and can be replaced
  by the user with a compatible Qt build, as required by the LGPL.
- **Source:** https://www.qt.io/download-open-source /
  https://code.qt.io/cgit/qt/qt5.git
- **Full text:** [`licenses/LGPL-3.0.txt`](licenses/LGPL-3.0.txt)

## FFmpeg (via Qt Multimedia's FFmpeg backend)

Bundled as `avcodec-61.dll`, `avformat-61.dll`, `avutil-59.dll`,
`swresample-5.dll`, `swscale-8.dll` — Qt Multimedia's official FFmpeg-based
media backend, distributed unmodified as shared libraries by the Qt project.

- **License:** GNU Lesser General Public License v2.1 or later (LGPL-2.1+).
- **Usage:** Unmodified, dynamically linked.
- **Source:** https://ffmpeg.org / https://github.com/FFmpeg/FFmpeg
- **Full text:** see [`licenses/hunspell/COPYING-LGPL-2.1.txt`](licenses/hunspell/COPYING-LGPL-2.1.txt)
  (same license text; Hunspell's copy is reused here to avoid duplicating
  the file twice in this repo).

## Hunspell v1.7.2

Spell-checking engine, compiled from source in `third_party/hunspell/` (not
committed to this repo — see [cmake/Hunspell.cmake](cmake/Hunspell.cmake) for
how to fetch it) and statically linked into `qenna-writer.exe`.

- **License:** Triple-licensed by upstream under GPL-2.0, LGPL-2.1, or
  MPL-1.1 (your choice). Qenna Writer relies on the **LGPL-2.1** option.
- **Usage:** Unmodified.
- **Source:** https://github.com/hunspell/hunspell
- **Full texts:** [`licenses/hunspell/`](licenses/hunspell/) (all three
  upstream options included, as shipped by the Hunspell project itself).

## MinGW-w64 runtime

Bundled as `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` —
required by any executable built with the MinGW-w64 GCC toolchain.

- **`libgcc_s_seh-1.dll` / `libstdc++-6.dll`:** GNU General Public License
  v3.0, with the **GCC Runtime Library Exception 3.1** (which permits
  linking without extending GPL terms to the linking program). Same GPL-3.0
  text as this project's own [LICENSE](LICENSE); exception text at
  https://www.gnu.org/licenses/gcc-exception-3.1.html.
- **`libwinpthread-1.dll`:** MIT-style license.
  Full text: [`licenses/MIT.txt`](licenses/MIT.txt)
- **Source:** https://www.mingw-w64.org

## Bundled fonts

Qenna Writer bundles ~120 open-source font families under
`src/assets/fonts/`, each in its own SIL Open Font License 1.1 (OFL) folder
with the original `OFL.txt`/license file and copyright notices intact, as
required by the OFL. See each font's own folder for its specific copyright
holder(s).

- **License:** SIL Open Font License 1.1.
- **Source:** https://openfontlicense.org
