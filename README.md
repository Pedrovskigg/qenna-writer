<img width="1600" height="533" alt="Qenna Writer: the writing app for people who write whole worlds. Manuscript, maps, galaxies, timeline, board, free." src="docs/img/hero-banner.webp" />

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
writer, for other writers. It runs on **Windows 10/11** and, since 2.0,
on **macOS 12 or later** (Apple silicon and Intel).

## New in 2.0: the Galaxy Map

<img width="1500" height="938" alt="Qenna Writer's Galaxy Map: a spiral galaxy seen from above, with dozens of named star systems along its arms" src="docs/img/galaxy-full.webp" />

The Map, for stories set among the stars. Territories, groups, connections
and documents work just like on the fantasy map, now on a galactic scale:
systems, planets, moons and whatever gets built around them. Start from the
Milky Way (Solar System included), Andromeda, M87 or the Sombrero, or
generate your own.

- **Two modes.** Navigator is the 3D map: fly between systems, follow
  orbits and build in plain sight. Astronomer works through writing: worlds
  grouped by territory, the document in the middle, the system on the side.
- **Real physics.** Mass, radius, gravity, density, temperature, day and
  year length are generated for every body, with a habitability class from
  I to VI. Keep them, change them or ignore them.
- **Structures.** Orbital rings, fleets, orbital cities, stations, up to
  Dyson spheres. Each one is a place you can name and write about.
- Full, Atlas, Sky, Infrared and Ink views, organic seeding as territories
  grow, documents saved to a drawer or the World Builder, and PNG export.

<img width="1500" height="938" alt="Astronomer mode: worlds grouped by territory, a document about a planet, and its system with a 3D view of the planet and its orbital ring" src="docs/img/galaxy-astronomer.webp" />

## The Map

<img width="1500" height="880" alt="Qenna Writer's Map: a generated continent with mountains, forests, roads, cities and kingdoms" src="docs/img/map-full.webp" />

Since 1.5, Qenna Writer draws maps. Generate a continent from a seed or paint one
by hand; the terrain is real relief (a mountain is height, not a picture of
a mountain), and altitude, latitude and humidity decide the climate and the
biome of every patch of ground.

- **Build** on it: castles in a dozen styles, landmarks, legends, ships and
  sea monsters, and brushes that paint whole cities and forests.
- **Explore** it: every territory is a group in your project. Click it and
  its documents open beside the map, readable and editable. Name a castle
  and it becomes a place of its own.
- **See it** in the afternoon or at night, with the windows lit, or in the
  Ink, Parchment, Nautical chart, Topographic, Watercolor and War plan views.
- A ruler with travel time, several maps per project, and PNG export at 1×,
  2× or 3× for the front of the book.

<img width="1500" height="880" alt="Explore mode: a kingdom outlined on the map, the tree of groups and a document open beside it" src="docs/img/map-explore.webp" />

## The Board

<img width="2000" height="1200" alt="The Board: three acts as areas, notes and index cards, a chapter card, red string between the clues, the cast with their portraits and a row of reference images" src="docs/img/board-full.webp" />

Your plot, pinned to the wall. The Board is a blank canvas for the parts
that aren't prose yet: post-its, index cards, images and linked documents
connected however the plot actually connects, not a mind-map with fixed
nodes.

- **Your cast, with their faces.** Drag a character in from the drawer and
  the photo from their sheet comes along. Pull red string from pin to pin,
  name it, give it a colour.
- **Chapters on the board.** Drop one in and click **open →** to go
  straight back to the text.
- Areas for each act, comments, stamps, a pen, sketch sheets, and any card
  can become a timeline event later.

<img width="49%" alt="Character cards with painted portraits, linked by coloured strings" src="docs/img/board-cast.webp" /> <img width="49%" alt="Image cards with captions: paintings and a landscape photo" src="docs/img/board-mood.webp" />

Or take it as far as you want: a whole act as a wall of images.

<img width="1600" height="762" alt="A whole act laid out on the Board as a wall of images, with the synopsis written on top" src="docs/img/board-wild.webp" />

## The editor

Fully themeable and built for more than the basics. Leave comments and
annotations, create documents straight from the text, add images and tag
other documents with `@`. Focus Mode hides every bar and leaves only the
page.

<img width="1300" height="682" alt="The editor in a dark theme with a photographic background" src="docs/img/shot-neon.webp" />
<img width="49%" alt="The editor in a light theme" src="docs/img/shot-light.webp" /> <img width="49%" alt="Focus Mode: every bar hidden, only the page left" src="docs/img/shot-focus.webp" />

The **Reference panel** opens beside the page: any chapter, scene or sheet
held open next to what you're writing, without switching windows.

<img width="1300" height="680" alt="Reference panel holding another chapter open beside the page" src="docs/img/shot-reference.webp" />

## Drawers and multiple manuscripts

Drawers for characters, settings, objects, lore, anything, with folders and
subfolders and no limit on how many. **Several manuscripts in one project**:
write a whole saga in the same place and look up a detail from book two
while you're writing book six.

<img width="300" alt="Drawers for characters, settings, objects and lore" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/panel-drawer.webp" /> <img width="66%" alt="Character drawer open beside the editor" src="docs/img/shot-characters.webp" />

## Characters

Sheets with the fields you want and a layout you pick (Tabs, Magazine, Case
file, Infobox or Dossier). Relationships between characters, presence
detected across chapters and scenes, and every line of dialogue logged
under whoever said it.

<img width="1500" height="938" alt="A character sheet in the Case file layout: photo, short fields and long sections with an index" src="docs/img/sheet-casefile.webp" />
<img width="32%" alt="Character relationships and bonds" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/bonds.webp" /> <img width="32%" alt="Character statistics: scenes, lines and who they share the page with" src="docs/img/panel-stats2.webp" /> <img width="32%" alt="Dialogue log, each line under whoever said it" src="docs/img/dialogo.webp" />

## The Pensarium

A floating panel (F4) for what you notice along the way: save a passage as
a memory, comment on a highlighted line, jot down loose notes, and find the
book's dialogue sorted by who said it.

<img width="49%" alt="Pensarium memories: passages saved for later, with their chapter" src="docs/img/panel-memories.webp" /> <img width="49%" alt="Pensarium notes" src="docs/img/panel-note.webp" />
<img width="806" height="319" alt="A comment on a highlighted passage" src="docs/img/panel-comment.webp" />

## Outline Mode and scenes

**Outline Mode** opens the whole manuscript as bands of cards: how each
chapter starts, its word count, lines of dialogue, the dialogue/narration
split and who's on stage. None of it is a field you fill in; it's read
from what you already wrote. Chapters split into scenes, nested under each
chapter in the sidebar.

<img width="1140" height="677" alt="Outline Mode: chapter cards with word counts, dialogue lines and the dialogue-to-narration split" src="docs/img/shot-outline.webp" />
<img width="1500" height="788" alt="Chapter list with scenes nested under each chapter" src="docs/img/shot-chapters.webp" />

## Timeline

Built from the dates and summaries already sitting in your chapters, with
flashback lines, branches and markers. Several views, spiral included.

<img width="1300" height="684" alt="Timeline built from the dates in the chapters, with branching lines" src="docs/img/panel-timeline.webp" />
<img width="1100" height="673" alt="Timeline, spiral view" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/timeline-espiral.webp" />

## Builder and Glossary

The **Builder** (beta) is for systems: religions, magic, politics,
technology, with rules and sections that keep the story consistent. The
**Glossary** keeps every invented word with its definition.

<img width="258" height="404" alt="The Builder, a worldbuilding and magic system tool" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/builder.webp" />

## Goals and statistics

Word or time goals, streaks, stars you can trade for days off, Pomodoro
sprints, and a Statistics panel that reads the manuscript on its own:
character participation, words per chapter, a summary of the project.

<img width="45%" alt="Project statistics: character participation, words per chapter, project summary" src="docs/img/panel-stats.webp" /> <img width="45%" alt="Writing goals calendar and streaks" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/panel-streak.webp" />

## Export, sound and themes

Export to DOCX, ODT, PDF and EPUB. Immersive Sound for ambient tracks and
focus music. And more than 500 built-in themes, or one built from scratch
in the Theme Creator: background image, editor, panel and text colours.

<img width="1198" height="759" alt="The themes panel, with search, categories and a preview of each one" src="docs/img/panel-themes.webp" />
<img width="49%" alt="The Theme Creator: background image, gradient, vignette and grain" src="docs/img/panel-theme-creator.webp" /> <img width="49%" alt="Export to DOCX, ODT, PDF or EPUB" src="https://raw.githubusercontent.com/Pedrovskigg/Qenna-Writer-releases/main/img/panel-export.webp" />

## Tech stack

- C++17, Qt 6.8 (Core, Gui, Widgets, Svg, Multimedia, Network, TextToSpeech)
- CMake + Ninja, MinGW-w64 (GCC) on Windows
- [Hunspell](https://github.com/hunspell/hunspell) for spell-checking
- zlib (from the MinGW toolchain) for reading theme packages
- LibreOffice Hunspell dictionaries and MyThes thesauri, plus OpenWordnet-PT,
  for spell-checking and synonyms
- Natural Earth, GeoNames and flag-icons for the offline world map

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

Qenna Writer is currently available in PT-BR, EN-US, ES, IT, and FR, via Qt
Linguist (`translations/*.ts`). Language switches at runtime through the
Main Menu.

## License

Qenna Writer is open for reading, modification, and redistribution.
However, any and all redistribution of the code must remain free of
charge and open. See the full license here: [LICENSE](LICENSE).

(GPL-3.0, supplemented by a Commons Clause condition and a mandatory
author-attribution term.)

Third-party components (Qt, Hunspell, FFmpeg, bundled fonts, etc.)
keep their own licenses — see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Related projects

- [Cover Creator](https://github.com/Pedrovskigg/qenna-cover-creator) —
  the old standalone cover tool, discontinued in 1.5 (covers are now made
  inside Qenna, with the Quick cover)
- [Mira's Maestro](https://github.com/Pedrovskigg/Maestro) — free musical
  theory plugin

## Contact

To report bugs, suggest new features or just scream about your plot at
3:00 AM: `qennawriter@gmail.com`
I can take a few hours or days to answer, but I will! I promise!
Or you can reach me on GitHub as well, on Qenna's repository.

— P.H. Lobato
