set(target_name client)

add_executable(${target_name})

include(CompileOptions)
set_compile_options(${target_name})

target_sources(
  ${target_name}
  PRIVATE
    client/TCPClient.cpp
)

target_link_libraries(
  ${target_name}
  PRIVATE
    my_chat_lib
    ftxui::screen
    ftxui::dom
    ftxui::component
)