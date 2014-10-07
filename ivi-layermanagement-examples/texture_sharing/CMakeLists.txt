cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig REQUIRED)

project(texture_sharing_test)

INCLUDE_DIRECTORIES(
    ./include
    ~/work/mesa/include
    ~/work/wayland/src/
    ~/work/wayland/protocol
    ~/work/wayland-ivi-extension/build/protocol
    ../../ivi-layermanagement-api/ilmClient/include
    ../../ivi-layermanagement-api/ilmCommon/include
    ../../ivi-layermanagement-api/ilmControl/include
)


SET(SRC_FILES
    ./src/bitmap.c
    ./src/texture_sharing_event.c
    ./src/texture_sharing_test_runner.c
    ./src/touch_event_util.c
    ./src/ilm_helper.c
    ./src/texture_sharing_api.c
    ./src/texture_sharing.c
    ./src/texture_sharing_stress.c
    ./src/window.c
    ../../build/protocol/ivi-share-extension-protocol.c
    ../../build/protocol/ivi-controller-protocol.c
    ../../build/protocol/ivi-application-protocol.c
    ../../ivi-layermanagement-api/ilmClient/src/ilm_client.c
    ../../ivi-layermanagement-api/ilmCommon/src/ilm_common.c
    ../../ivi-layermanagement-api/ilmControl/src/ilm_control_wayland_platform.c
    ../../ivi-layermanagement-api/ilmClient/src/ilm_client_wayland_platform.c
    ../../ivi-layermanagement-api/ilmCommon/src/ilm_common_wayland_platform.c
)

link_directories(~/usrfs/lib)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_FILES})

find_package(PkgConfig REQUIRED)
pkg_search_module(WaylandClient REQUIRED wayland-client)
target_link_libraries(${PROJECT_NAME} ${WaylandClient_LIBRARIES})

pkg_search_module(WaylandServer REQUIRED wayland-server)
target_link_libraries(${PROJECT_NAME} ${WaylandServer_LIBRARIES})

pkg_search_module(WaylandEgl REQUIRED wayland-egl)
target_link_libraries(${PROJECT_NAME} ${WaylandEgl_LIBRARIES})

pkg_search_module(OpenGL REQUIRED gl)
target_link_libraries(${PROJECT_NAME} ${OpenGL_LIBRARIES})

pkg_search_module(EGL REQUIRED egl)
target_link_libraries(${PROJECT_NAME} ${EGL_LIBRARIES})

target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} -lpthread)

add_library(TextureSharing SHARED ${SRC_FILES})

SET(TRIANGLE_SRC_FILES
    ./src/triangles.c
    ./src/bitmap.c
    ./src/ilm_helper.c
    ./src/window.c
    ../../build/protocol/ivi-share-extension-protocol.c
    ../../build/protocol/ivi-controller-protocol.c
    ../../build/protocol/ivi-application-protocol.c
    ../../ivi-layermanagement-api/ilmClient/src/ilm_client.c
    ../../ivi-layermanagement-api/ilmCommon/src/ilm_common.c
    ../../ivi-layermanagement-api/ilmControl/src/ilm_control_wayland_platform.c
    ../../ivi-layermanagement-api/ilmClient/src/ilm_client_wayland_platform.c
    ../../ivi-layermanagement-api/ilmCommon/src/ilm_common_wayland_platform.c
)

ADD_EXECUTABLE(triangles_test ${TRIANGLE_SRC_FILES})

set_target_properties(triangles_test PROPERTIES COMPILE_DEFINITIONS "USE_TRIANGLE_MAIN")

target_link_libraries(triangles_test ${WaylandClient_LIBRARIES})

target_link_libraries(triangles_test ${WaylandServer_LIBRARIES})

target_link_libraries(triangles_test ${WaylandEgl_LIBRARIES})

target_link_libraries(triangles_test ${OpenGL_LIBRARIES})

target_link_libraries(triangles_test ${EGL_LIBRARIES})

target_link_libraries(triangles_test ${CMAKE_THREAD_LIBS_INIT} -lpthread -lm)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
