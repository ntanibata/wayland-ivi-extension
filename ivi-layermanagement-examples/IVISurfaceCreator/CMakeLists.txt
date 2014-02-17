############################################################################
#
# Copyright 2010-2014 BMW Car IT GmbH
# Copyright (C) 2011 DENSO CORPORATION and Robert Bosch Car Multimedia Gmbh
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#		http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
############################################################################

cmake_minimum_required (VERSION 2.6)

project(IVISurfaceCreator)

find_package(PkgConfig)
pkg_check_modules(WAYLAND_CLIENT wayland-client REQUIRED)

GET_TARGET_PROPERTY(ILM_CLIENT_INCLUDE_DIRS  ilmClient  INCLUDE_DIRECTORIES)
GET_TARGET_PROPERTY(ILM_CONTROL_INCLUDE_DIRS ilmControl INCLUDE_DIRECTORIES)

include_directories(
    include
    ${ILM_CLIENT_INCLUDE_DIRS}
    ${ILM_CONTROL_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)

link_directories(
    ${WAYLAND_CLIENT_LIBRARY_DIRS}
)


#set (HEADER_FILES
#    include/Camera.h
#)

set (SRC_FILES
    src/main.c
)

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
#   ${HEADER_FILES}
)


set(LIBS
    ${LIBS}
    ${WAYLAND_CLIENT_LIBRARIES}
    ilmClient
    ilmControl
)

add_dependencies(${PROJECT_NAME}
    ${LIBS}
)

target_link_libraries(${PROJECT_NAME}
    ${LIBS}
)

install(
    TARGETS     ${PROJECT_NAME}
    DESTINATION bin
)
