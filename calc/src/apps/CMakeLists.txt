add_executable(calc calc_cli.cc)
target_link_libraries(calc PRIVATE calculator)

include(FetchContent)
FetchContent_Declare(webview
    GIT_REPOSITORY https://github.com/webview/webview
    GIT_TAG 9a608c2ce21521f2e1006df2003970cf02871ede)
FetchContent_MakeAvailable(webview)
add_executable(calc-gui calc_gui.cc)
target_link_libraries(calc-gui PRIVATE calculator webview::core)
