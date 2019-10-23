SET(pwd ${CMAKE_CURRENT_LIST_DIR})

SET(src "")
AUX_SOURCE_DIRECTORY(${pwd} src)
AUX_SOURCE_DIRECTORY(${pwd} ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_executable(npr ${src})
message(STATUS "npr added ${src}")

target_link_libraries(npr ${stdgl_libraries})
FIND_PACKAGE(JPEG REQUIRED)
TARGET_LINK_LIBRARIES(npr ${JPEG_LIBRARIES})
