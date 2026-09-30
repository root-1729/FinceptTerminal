# Autotrade integration (root-1729 fork of FinceptTerminal).
#
# All autotrade code lives in src/autotrade/. Upstream files carry only these
# hooks, each marked "autotrade:" so they're easy to find when merging upstream:
#   CMakeLists.txt                  include() of this file after add_executable(FinceptTerminal)
#   src/app/WindowFrame_Setup.cpp   #include + register_factory("autotrade", ...)
#   src/app/DockScreenRouter.cpp    "autotrade" screen title
#   src/ui/navigation/ToolBar.cpp   menu entry under Trading & Portfolio
#   src/ui/navigation/CommandBar.cpp command palette entry
#   src/ui/navigation/FKeyBar.cpp   AUTOTRADE tab in the top tab row
#
# The api-gateway URL is set with FINCEPT_AUTOTRADE_URL (default http://localhost:8000).

target_sources(FinceptTerminal PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/AutotradeApi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/AutotradeScreen.cpp
)
