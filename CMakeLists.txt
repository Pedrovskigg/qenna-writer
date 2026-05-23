cmake_minimum_required(VERSION 3.16)
project(mira-writing VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg LinguistTools)

qt_standard_project_setup()

# Hunspell (corretor ortográfico) — compilado a partir de third_party/hunspell.
include(cmake/Hunspell.cmake)

qt_add_executable(mira-writing
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/TopToolbar.cpp
    src/TopToolbar.h
    src/IconUtils.cpp
    src/IconUtils.h
    src/FontPickerPopup.cpp
    src/FontPickerPopup.h
    src/ImageInsertDialog.cpp
    src/ImageInsertDialog.h
    src/ImageOverlay.cpp
    src/ImageOverlay.h
    src/ProjectModel.cpp
    src/ProjectModel.h
    src/ProjectStorage.cpp
    src/ProjectStorage.h
    src/ProjectSaver.cpp
    src/ProjectSaver.h
    src/WordCounter.cpp
    src/WordCounter.h
    src/WordCountPanel.cpp
    src/WordCountPanel.h
    src/WordCounterCalendar.cpp
    src/WordCounterCalendar.h
    src/RefMenuPanel.cpp
    src/RefMenuPanel.h
    src/ElementCreateDialog.cpp
    src/ElementCreateDialog.h
    src/ElementsStore.cpp
    src/ElementsStore.h
    src/DocCache.cpp
    src/DocCache.h
    src/EditorHost.cpp
    src/EditorHost.h
    src/LeftBar.cpp
    src/LeftBar.h
    src/DrawerListPanel.cpp
    src/DrawerListPanel.h
    src/DrawerCreateDialog.cpp
    src/DrawerCreateDialog.h
    src/ManuscriptPanel.cpp
    src/ManuscriptPanel.h
    src/SceneUtils.cpp
    src/SceneUtils.h
    src/VariationBar.cpp
    src/VariationBar.h
    src/SpellChecker.cpp
    src/SpellChecker.h
    src/SpellHighlighter.cpp
    src/SpellHighlighter.h
    src/SpellEditor.cpp
    src/SpellEditor.h
    src/SettingsPanel.cpp
    src/SettingsPanel.h
    src/SelectionPopup.cpp
    src/SelectionPopup.h
    src/Theme.cpp
    src/Theme.h
    src/EditorLayout.cpp
    src/EditorLayout.h
    src/ThemesPanel.cpp
    src/ThemesPanel.h
    resources/icons.qrc
)

qt_add_translations(mira-writing
    TS_FILES
        translations/mira_pt_BR.ts
        translations/mira_en.ts
    SOURCE_TARGETS mira-writing
)

target_link_libraries(mira-writing PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    hunspell
)

target_compile_definitions(mira-writing PRIVATE
    DEV_ASSETS_DIR="${CMAKE_SOURCE_DIR}/src/assets"
)

set_target_properties(mira-writing PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
