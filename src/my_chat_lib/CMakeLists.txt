set(target_name my_chat_lib)

add_library(${target_name} STATIC
  ${target_name}/MCL.cpp
  ${target_name}/MCL.hpp
)

include(CompileOptions)

set_compile_options(${target_name})

target_include_directories(
  ${target_name}
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)