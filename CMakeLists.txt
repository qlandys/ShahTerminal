cmake_minimum_required(VERSION 3.16)
project(ShahTerminal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(orderbook_backend
    backend/src/main.cpp
    backend/src/OrderBook.cpp
)

target_include_directories(orderbook_backend
    PRIVATE
        backend/include
        external/nlohmann
)

if (MSVC)
    target_compile_options(orderbook_backend PRIVATE /W4 /permissive- /MP)
    target_link_libraries(orderbook_backend PRIVATE winhttp)
else ()
    target_compile_options(orderbook_backend PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(orderbook_backend PRIVATE winhttp)
endif ()

# Optional native GUI library for high‑performance DOM widget.
# This requires Qt development libraries; if they are not available,
# the core backend target above still builds as before.
find_package(Qt6 COMPONENTS Widgets Gui Network WebSockets Multimedia QUIET) # Добавляем Gui для Qt6
message(STATUS "Qt6_FOUND: ${Qt6_FOUND}")
if (Qt6_FOUND)
    add_executable(shah_gui
        gui_native/main.cpp
        gui_native/MainWindow.cpp
        gui_native/MainWindow.h
        gui_native/LadderClient.cpp
        gui_native/LadderClient.h
        gui_native/ConnectionStore.cpp
        gui_native/ConnectionStore.h
        gui_native/TradeManager.cpp
        gui_native/TradeManager.h
        gui_native/TradeTypes.h
        gui_native/ConnectionsWindow.cpp
        gui_native/ConnectionsWindow.h
        gui_native/PluginsWindow.cpp
        gui_native/PluginsWindow.h
        gui_native/SettingsWindow.cpp
        gui_native/SettingsWindow.h
        gui_native/PrintsWidget.cpp
        gui_native/PrintsWidget.h
        gui_native/DomWidget.cpp
        gui_native/DomWidget.h
        gui_native/SymbolPickerDialog.cpp
        gui_native/SymbolPickerDialog.h
    )
    target_link_libraries(shah_gui PRIVATE Qt6::Widgets Qt6::Gui Qt6::Network Qt6::WebSockets
                                          $<$<TARGET_EXISTS:Qt6::Multimedia>:Qt6::Multimedia>)
    target_include_directories(shah_gui PRIVATE external/nlohmann)
elseif (NOT WIN32)
    # Try Qt5 as a fallback on non‑Windows platforms.
    find_package(Qt5 COMPONENTS Widgets Gui Network WebSockets Multimedia QUIET) # Добавляем Gui для Qt5
    if (Qt5Widgets_FOUND)
        add_library(dom_widget STATIC
            gui_native/DomWidget.cpp
            gui_native/DomWidget.h
        )
        target_link_libraries(dom_widget PRIVATE Qt5::Widgets)
        target_compile_features(dom_widget PRIVATE cxx_std_20)

        add_executable(shah_gui
            gui_native/main.cpp
            gui_native/MainWindow.cpp
            gui_native/MainWindow.h
            gui_native/LadderClient.cpp
            gui_native/LadderClient.h
            gui_native/ConnectionStore.cpp
            gui_native/ConnectionStore.h
            gui_native/TradeManager.cpp
            gui_native/TradeManager.h
            gui_native/TradeTypes.h
            gui_native/ConnectionsWindow.cpp
            gui_native/ConnectionsWindow.h
        )
    target_link_libraries(shah_gui PRIVATE Qt5::Widgets Qt5::Gui Qt5::Network Qt5::WebSockets
                                          $<$<TARGET_EXISTS:Qt5::Multimedia>:Qt5::Multimedia>
                                          dom_widget) # Добавляем Qt5::Gui
    target_include_directories(shah_gui PRIVATE external/nlohmann)
    endif ()
message(STATUS "Qt5Widgets_FOUND: ${Qt5Widgets_FOUND}")
endif ()
