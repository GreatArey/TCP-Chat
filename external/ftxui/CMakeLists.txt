include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
  GIT_SHALLOW    TRUE
  PREFIX         ${CMAKE_CURRENT_BINARY_DIR}
)

FetchContent_MakeAvailable(ftxui)