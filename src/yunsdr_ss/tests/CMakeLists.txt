CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/tests")
SET(YUNSDR_TXRX_SRC "")
list(APPEND YUNSDR_TXRX_SRC ${PROJECT_SOURCE_DIR}/src/yunsdr_ss/tests/yunsdr_ss_txrx.c)
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/yunsdr_ss/include")

ADD_EXECUTABLE(yunsdr_ss_txrx ${YUNSDR_TXRX_SRC})
target_link_libraries(yunsdr_ss_txrx m)
target_link_libraries(yunsdr_ss_txrx yunsdr_ss)

