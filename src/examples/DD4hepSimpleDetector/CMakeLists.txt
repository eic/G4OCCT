# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

cmake_minimum_required(VERSION 3.16)

find_file(
  G4OCCT_DD4HEP_ELEMENTS_XML
  NAMES elements.xml
  HINTS "${DD4hep_ROOT}/DDDetectors/compact"
        "${DD4hep_DIR}/../../../DDDetectors/compact" REQUIRED)
find_file(
  G4OCCT_DD4HEP_MATERIALS_XML
  NAMES materials.xml
  HINTS "${DD4hep_ROOT}/DDDetectors/compact"
        "${DD4hep_DIR}/../../../DDDetectors/compact" REQUIRED)

set(G4OCCT_DD4HEP_SIMPLE_STEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/step")

configure_file(
  compact/Simple_ILD_G4OCCT.xml.in
  "${CMAKE_CURRENT_BINARY_DIR}/Simple_ILD_G4OCCT.xml" @ONLY)

add_executable(exampleDD4hepSimpleDetector exampleDD4hepSimpleDetector.cc)

target_link_libraries(exampleDD4hepSimpleDetector PRIVATE DD4hep::DDCore
                                                          G4OCCTDD4hep)

if(TARGET Components_G4OCCTDD4hep)
  add_dependencies(exampleDD4hepSimpleDetector Components_G4OCCTDD4hep)
endif()

target_compile_definitions(
  exampleDD4hepSimpleDetector
  PRIVATE
    G4OCCT_DD4HEP_SIMPLE_COMPACT="${CMAKE_CURRENT_BINARY_DIR}/Simple_ILD_G4OCCT.xml"
    G4OCCT_DD4HEP_PLUGIN_LIBRARY="$<TARGET_FILE:G4OCCTDD4hep>")

set(_dd4hep_plugin_env
    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{LD_LIBRARY_PATH};ROOT_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{ROOT_LIBRARY_PATH};DD4HEP_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{DD4HEP_LIBRARY_PATH}")

add_test(NAME run_exampleDD4hepSimpleDetector
         COMMAND $<TARGET_FILE:exampleDD4hepSimpleDetector>)
set_tests_properties(
  run_exampleDD4hepSimpleDetector
  PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ENVIRONMENT
             "${_dd4hep_plugin_env}" LABELS "dd4hep" TIMEOUT 120)
