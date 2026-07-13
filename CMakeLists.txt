cmake_minimum_required(VERSION 3.16)

# Versão única do app — também lida pelo installer (qenna-writer.iss) via ISPP,
# pra não precisar bumpar em dois lugares.
# file(STRINGS) só é lido no configure — sem esse CONFIGURE_DEPENDS, um bump no
# VERSION não força reconfigure e "cmake --build" sozinho continua embutindo a
# versão antiga (cache velho), fazendo o UpdateChecker achar pra sempre que há
# atualização nova mesmo já estando na última.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" APP_VERSION_STRING)
string(STRIP "${APP_VERSION_STRING}" APP_VERSION_STRING)

project(qenna-writer VERSION ${APP_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Svg Multimedia Network LinguistTools)

qt_standard_project_setup()

# Hunspell (corretor ortográfico) — compilado a partir de third_party/hunspell.
include(cmake/Hunspell.cmake)

qt_add_executable(qenna-writer
    src/main.cpp
    src/CrashLogger.cpp
    src/CrashLogger.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/ExportPanel.cpp
    src/ExportPanel.h
    src/Exporter.cpp
    src/Exporter.h
    src/ZipWriter.cpp
    src/ZipWriter.h
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
    src/WritingStatsDialog.cpp
    src/WritingStatsDialog.h
    src/RefMenuPanel.cpp
    src/RefMenuPanel.h
    src/PensarioPanel.cpp
    src/PensarioPanel.h
    src/NotesStore.cpp
    src/NotesStore.h
    src/NoteEditPopup.cpp
    src/NoteEditPopup.h
    src/NameGenerator.cpp
    src/NameGenerator.h
    src/GeoData.cpp
    src/GeoData.h
    src/MapView.cpp
    src/MapView.h
    src/MapPanel.cpp
    src/MapPanel.h
    src/MapPinsStore.cpp
    src/MapPinsStore.h
    src/FindBar.cpp
    src/FindBar.h
    src/GlobalSearchPanel.cpp
    src/GlobalSearchPanel.h
    src/ElementCreateDialog.cpp
    src/ElementCreateDialog.h
    src/ElementsPresentDialog.cpp
    src/ElementsPresentDialog.h
    src/ElementsStore.cpp
    src/ElementsStore.h
    src/CharacterDetector.cpp
    src/CharacterDetector.h
    src/DialogueVerbs.h
    src/DialogueDetector.cpp
    src/DialogueDetector.h
    src/DialogueStore.cpp
    src/DialogueStore.h
    src/PresencePopup.cpp
    src/PresencePopup.h
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
    src/ScreenplayFormat.cpp
    src/ScreenplayFormat.h
    src/SettingsPanel.cpp
    src/SettingsPanel.h
    src/AboutDialog.cpp
    src/AboutDialog.h
    src/SelectionPopup.cpp
    src/SelectionPopup.h
    src/MarkerStore.cpp
    src/MarkerStore.h
    src/MarkerPickPopup.cpp
    src/MarkerPickPopup.h
    src/MarkerHoverPopup.cpp
    src/MarkerHoverPopup.h
    src/Theme.cpp
    src/Theme.h
    src/EditorLayout.cpp
    src/EditorLayout.h
    src/ThemesPanel.cpp
    src/ThemesPanel.h
    src/BackgroundWidget.cpp
    src/BackgroundWidget.h
    src/ThemeEditorDialog.cpp
    src/ThemeEditorDialog.h
    src/ThemePreviewWidget.cpp
    src/ThemePreviewWidget.h
    src/ProjectInfoPanel.cpp
    src/ProjectInfoPanel.h
    src/ProjectInfoHover.cpp
    src/ProjectInfoHover.h
    src/BondTypes.cpp
    src/BondTypes.h
    src/BondsLayer.cpp
    src/BondsLayer.h
    src/BondPopup.cpp
    src/BondPopup.h
    src/BondViewPanel.cpp
    src/BondViewPanel.h
    src/AmbienceManager.cpp
    src/AmbienceManager.h
    src/AmbiencePanel.cpp
    src/AmbiencePanel.h
    src/GlossaryStore.cpp
    src/GlossaryStore.h
    src/GlossaryPanel.cpp
    src/GlossaryPanel.h
    src/GlossaryAddPopup.cpp
    src/GlossaryAddPopup.h
    src/ConstrutorStore.cpp
    src/ConstrutorStore.h
    src/ConstrutorWindow.cpp
    src/ConstrutorWindow.h
    src/MemoriesStore.cpp
    src/MemoriesStore.h
    src/MemoryAddPopup.cpp
    src/MemoryAddPopup.h
    src/RemindersStore.cpp
    src/RemindersStore.h
    src/RemindersPanel.cpp
    src/RemindersPanel.h
    src/GroupsPanel.cpp
    src/GroupsPanel.h
    src/LousaScene.cpp
    src/LousaScene.h
    src/LousaView.cpp
    src/LousaView.h
    src/LousaPanel.cpp
    src/LousaPanel.h
    src/LousaTypes.h
    src/CharacterSheetPanel.cpp
    src/CharacterSheetPanel.h
    src/MentionPopup.cpp
    src/MentionPopup.h
    src/TimelineTypes.h
    src/TimelineScene.cpp
    src/TimelineScene.h
    src/TimelineView.cpp
    src/TimelineView.h
    src/TimelineEventItem.cpp
    src/TimelineEventItem.h
    src/TimelineConnItem.cpp
    src/TimelineConnItem.h
    src/TimelineEventPopup.cpp
    src/TimelineEventPopup.h
    src/TimelinePanel.cpp
    src/TimelinePanel.h
    src/CardItem.cpp
    src/CardItem.h
    src/ConnectionItem.cpp
    src/ConnectionItem.h
    src/ZoneItem.cpp
    src/ZoneItem.h
    src/Quotes.cpp
    src/Quotes.h
    src/MainMenuDialog.cpp
    src/MainMenuDialog.h
    src/NewProjectFlow.cpp
    src/NewProjectFlow.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    resources/icons.qrc
    resources/geo.qrc
    resources/flags.qrc
    $<$<PLATFORM_ID:Windows>:resources/mira.rc>
)

qt_add_translations(qenna-writer
    TS_FILES
        translations/qenna_pt_BR.ts
        translations/qenna_en.ts
    SOURCE_TARGETS qenna-writer
)

target_link_libraries(qenna-writer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Multimedia
    Qt6::Network
    hunspell
)

if(WIN32)
    target_link_libraries(qenna-writer PRIVATE dwmapi)
endif()

target_compile_definitions(qenna-writer PRIVATE
    DEV_ASSETS_DIR="${CMAKE_SOURCE_DIR}/src/assets"
    APP_VERSION="${APP_VERSION_STRING}"
)

set_target_properties(qenna-writer PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
