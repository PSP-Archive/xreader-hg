include_directories(${libmpc_SOURCE_DIR}/include)

include_directories(${libmpc_SOURCE_DIR}/libwavformat)
link_directories(${libmpc_BINARY_DIR}/libwavformat)
include_directories(${libmpc_SOURCE_DIR}/libmpcdec)
link_directories(${libmpc_BINARY_DIR}/libmpcdec)

if(MSVC)
include_directories(${libmpc_SOURCE_DIR}/win32)
add_executable(mpcdec mpcdec ${libmpc_SOURCE_DIR}/win32/attgetopt)
endif(MSVC)

add_executable(mpcdec mpcdec)

if(NOT MSVC)
target_link_libraries(mpcdec m)
endif(NOT MSVC)

target_link_libraries(mpcdec wavformat_static)
target_link_libraries(mpcdec mpcdec_static)
