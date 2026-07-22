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

configure_file(compact/Simple_ILD_G4OCCT.xml.in
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
    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{LD_LIBRARY_PATH}"
    "ROOT_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{ROOT_LIBRARY_PATH}"
    "DD4HEP_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{DD4HEP_LIBRARY_PATH}")
foreach(_ds ${Geant4_DATASETS})
  list(APPEND _dd4hep_plugin_env
       "${Geant4_DATASET_${_ds}_ENVVAR}=${Geant4_DATASET_${_ds}_PATH}")
endforeach()

add_test(NAME run_exampleDD4hepSimpleDetector
         COMMAND $<TARGET_FILE:exampleDD4hepSimpleDetector>)
add_test(NAME ddsim_exampleDD4hepSimpleDetector
         COMMAND ddsim --compactFile
                 ${CMAKE_CURRENT_BINARY_DIR}/Simple_ILD_G4OCCT.xml --runType
                 batch --enableGun --gun.particle geantino --gun.energy 1*GeV
                 --numberOfEvents 1 --outputFile
                 ${CMAKE_CURRENT_BINARY_DIR}/ddsim_exampleDD4hepSimpleDetector.edm4hep.root)
set_tests_properties(
  run_exampleDD4hepSimpleDetector
  PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ENVIRONMENT
             "${_dd4hep_plugin_env}" LABELS "dd4hep" TIMEOUT 120)
set_tests_properties(
  ddsim_exampleDD4hepSimpleDetector
  PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" ENVIRONMENT
             "${_dd4hep_plugin_env}" LABELS "dd4hep;ddsim" TIMEOUT 120)
