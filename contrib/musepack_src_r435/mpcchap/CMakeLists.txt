FIND_PATH(CUEFILE_INCLUDE_DIR cuetools/cuefile.h
	${libmpc_SOURCE_DIR}/../libcuefile/include
)

FIND_LIBRARY(CUEFILE_LIBRARY NAMES cuefile PATHS
	${CUEFILE_INCLUDE_DIR}/../src
)

include_directories(${libmpc_SOURCE_DIR}/include)
include_directories(${CUEFILE_INCLUDE_DIR})

link_directories(${libmpc_BINARY_DIR}/libmpcdec)
link_directories(${libmpc_BINARY_DIR}/libmpcenc)

add_executable(mpcchap mpcchap dictionary iniparser ${libmpc_SOURCE_DIR}/common/tags)

target_link_libraries(mpcchap mpcdec_static)
target_link_libraries(mpcchap mpcenc_static)
target_link_libraries(mpcchap ${CUEFILE_LIBRARY})

if(NOT MSVC)
target_link_libraries(mpcchap m)
endif(NOT MSVC)

