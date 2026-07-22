# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

cmake_minimum_required(VERSION 3.16)

# ── Paths to STEP fixtures reused from the core test suite ───────────────────
set(G4OCCT_STEP_SOLID_PATH
    "${CMAKE_SOURCE_DIR}/src/tests/fixtures/geometry/direct-primitives/G4Box/box-20x30x40-v1/shape.step"
)
set(G4OCCT_STEP_ASSEMBLY_PATH
    "${CMAKE_SOURCE_DIR}/src/tests/fixtures/assembly-comparison/triple-box-v1/shape.step"
)
set(G4OCCT_STEP_SOLID_COMPACT
    "${CMAKE_CURRENT_BINARY_DIR}/compact_step_solid.xml")
set(G4OCCT_STEP_ASSEMBLY_COMPACT
    "${CMAKE_CURRENT_BINARY_DIR}/compact_step_assembly.xml")

# ── DD4hep standard elements file ────────────────────────────────────────────
#
# DD4hep 1.35 deliberately resets the ROOT TGeoElementTable to empty in
# DetectorImp's constructor, then fills it only from compact XML.
# TGeoMixture::AddElement(TGeoElement*, Double_t) internally calls AddElement(A,
# Z, w) which checks z > table->GetNelements()-1.  If the table contains only a
# handful of entries, any element with a high Z will trigger a ROOT Fatal.
#
# The fix is to include DD4hep's standard elements.xml (which defines all Z=1..
# Z=118 elements) before any material definitions.  After including it the table
# has ~119 entries, so AddElement never fires the z-range check.
#
# elements.xml is installed by DD4hep's DDDetectors package to:
# ${DD4hep_ROOT}/DDDetectors/compact/elements.xml DD4hep_ROOT is exported by
# DD4hepConfig.cmake as the installation prefix.
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
message(STATUS "G4OCCT DD4hep elements: ${G4OCCT_DD4HEP_ELEMENTS_XML}")

# Generate compact XML files with the absolute fixture paths substituted in.
configure_file(compact_step_solid.xml.in "${G4OCCT_STEP_SOLID_COMPACT}" @ONLY)
configure_file(compact_step_assembly.xml.in "${G4OCCT_STEP_ASSEMBLY_COMPACT}"
               @ONLY)

# ── test_dd4hep_plugins ───────────────────────────────────────────────────────

add_executable(test_dd4hep_plugins test_dd4hep_plugins.cc)

target_compile_definitions(
  test_dd4hep_plugins
  PRIVATE G4OCCT_COMPACT_STEP_SOLID="${G4OCCT_STEP_SOLID_COMPACT}"
          G4OCCT_COMPACT_STEP_ASSEMBLY="${G4OCCT_STEP_ASSEMBLY_COMPACT}"
          G4OCCT_DD4HEP_PLUGIN_LIBRARY="$<TARGET_FILE:G4OCCTDD4hep>")

target_include_directories(test_dd4hep_plugins
                           PRIVATE ${CMAKE_SOURCE_DIR}/src/tests)

target_link_libraries(
  test_dd4hep_plugins PRIVATE G4OCCT::G4OCCT G4OCCTDD4hep DD4hep::DDCore
                              GTest::gtest_main)

if(TARGET Components_G4OCCTDD4hep)
  add_dependencies(test_dd4hep_plugins Components_G4OCCTDD4hep)
endif()

# ── CTest registration ────────────────────────────────────────────────────────

set(_dd4hep_plugin_env
    "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{LD_LIBRARY_PATH}"
    "ROOT_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{ROOT_LIBRARY_PATH}"
    "DD4HEP_LIBRARY_PATH=$<TARGET_FILE_DIR:G4OCCTDD4hep>:$ENV{DD4HEP_LIBRARY_PATH}"
)
foreach(ds ${Geant4_DATASETS})
  list(APPEND _dd4hep_plugin_env
       "${Geant4_DATASET_${ds}_ENVVAR}=${Geant4_DATASET_${ds}_PATH}")
endforeach()

add_test(NAME dd4hep_step_solid COMMAND $<TARGET_FILE:test_dd4hep_plugins>
                                        --gtest_filter=STEPSolidTest.*)
add_test(NAME dd4hep_step_assembly COMMAND $<TARGET_FILE:test_dd4hep_plugins>
                                           --gtest_filter=STEPAssemblyTest.*)
add_test(
  NAME ddsim_step_solid
  COMMAND
    ddsim --compactFile ${G4OCCT_STEP_SOLID_COMPACT} --runType batch
    --enableGun --gun.particle geantino --gun.energy 1*GeV --numberOfEvents 1
    --outputFile ${CMAKE_CURRENT_BINARY_DIR}/ddsim_step_solid.edm4hep.root)
add_test(
  NAME ddsim_step_assembly
  COMMAND
    ddsim --compactFile ${G4OCCT_STEP_ASSEMBLY_COMPACT} --runType batch
    --enableGun --gun.particle geantino --gun.energy 1*GeV --numberOfEvents 1
    --outputFile ${CMAKE_CURRENT_BINARY_DIR}/ddsim_step_assembly.edm4hep.root)

set_tests_properties(
  dd4hep_step_solid dd4hep_step_assembly
  PROPERTIES WORKING_DIRECTORY
             "${CMAKE_CURRENT_BINARY_DIR}"
             ENVIRONMENT
             "${_dd4hep_plugin_env}"
             LABELS
             "dd4hep"
             TIMEOUT
             120)
set_tests_properties(
  ddsim_step_solid ddsim_step_assembly
  PROPERTIES WORKING_DIRECTORY
             "${CMAKE_CURRENT_BINARY_DIR}"
             ENVIRONMENT
             "${_dd4hep_plugin_env}"
             LABELS
             "dd4hep;ddsim"
             TIMEOUT
             120)
