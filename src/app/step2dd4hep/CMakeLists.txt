# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

add_executable(step2dd4hep main.cc)

# step2dd4hep uses OCCT XDE directly and does not link against Geant4; it is a
# standalone geometry-inspection tool that only needs OpenCASCADE.
target_include_directories(step2dd4hep SYSTEM
                           PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(step2dd4hep PRIVATE ${OpenCASCADE_LIBRARIES})

install(TARGETS step2dd4hep RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ── Tests ─────────────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  # Fixture paths reused from the core test suite.
  set(_STEP2DD4HEP_SOLID_STEP
      "${CMAKE_SOURCE_DIR}/src/tests/fixtures/geometry/direct-primitives/G4Box/box-20x30x40-v1/shape.step"
  )
  set(_STEP2DD4HEP_ASSEMBLY_STEP
      "${CMAKE_SOURCE_DIR}/src/tests/fixtures/assembly-comparison/triple-box-v1/shape.step"
  )

  # Working directories for each test; created at configure time.
  set(_STEP2DD4HEP_SOLID_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_solid")
  set(_STEP2DD4HEP_ASSEMBLY_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_assembly")
  set(_STEP2DD4HEP_BADFILE_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_badfile")
  set(_STEP2DD4HEP_EXISTING_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_existing_map")
  file(MAKE_DIRECTORY "${_STEP2DD4HEP_SOLID_DIR}")
  file(MAKE_DIRECTORY "${_STEP2DD4HEP_ASSEMBLY_DIR}")
  file(MAKE_DIRECTORY "${_STEP2DD4HEP_BADFILE_DIR}")
  file(MAKE_DIRECTORY "${_STEP2DD4HEP_EXISTING_DIR}")

  # Seed material map for the --existing-map test.
  set(_STEP2DD4HEP_SEED_MAP "${_STEP2DD4HEP_EXISTING_DIR}/seed_materials.xml")
  file(
    WRITE "${_STEP2DD4HEP_SEED_MAP}"
    "<!-- seed -->\n<material_map>\n  <entry step_name=\"Component\" dd4hep_material=\"G4_Al\"/>\n</material_map>\n"
  )

  set(_CHECK_CONTAINS "${CMAKE_CURRENT_SOURCE_DIR}/check_contains.cmake")

  # ── 1. Solid STEP: exits 0 ────────────────────────────────────────────────
  add_test(
    NAME step2dd4hep_solid_runs
    COMMAND $<TARGET_FILE:step2dd4hep> -o shape.xml -m shape_materials.xml
            "${_STEP2DD4HEP_SOLID_STEP}"
    WORKING_DIRECTORY "${_STEP2DD4HEP_SOLID_DIR}")

  # ── 2. Solid: compact XML exists and contains G4OCCT_STEPAssembly ─────────
  add_test(
    NAME step2dd4hep_solid_compact_plugin
    COMMAND ${CMAKE_COMMAND} -D "FILE=${_STEP2DD4HEP_SOLID_DIR}/shape.xml" -D
            "STRING=G4OCCT_STEPAssembly" -P "${_CHECK_CONTAINS}")

  # ── 3. Solid: compact XML references the STEP file path ──────────────────
  add_test(
    NAME step2dd4hep_solid_compact_path
    COMMAND ${CMAKE_COMMAND} -D "FILE=${_STEP2DD4HEP_SOLID_DIR}/shape.xml" -D
            "STRING=box-20x30x40-v1" -P "${_CHECK_CONTAINS}")

  # ── 4. Solid: material map is generated ──────────────────────────────────
  add_test(
    NAME step2dd4hep_solid_matmap_exists
    COMMAND
      ${CMAKE_COMMAND} -D "FILE=${_STEP2DD4HEP_SOLID_DIR}/shape_materials.xml"
      -D "STRING=material_map" -P "${_CHECK_CONTAINS}")

  # ── 5. Assembly STEP: exits 0 and compact contains material entry ─────────
  add_test(
    NAME step2dd4hep_assembly_runs
    COMMAND $<TARGET_FILE:step2dd4hep> -o assembly.xml -m assembly_materials.xml
            "${_STEP2DD4HEP_ASSEMBLY_STEP}"
    WORKING_DIRECTORY "${_STEP2DD4HEP_ASSEMBLY_DIR}")

  add_test(
    NAME step2dd4hep_assembly_compact_has_entry
    COMMAND
      ${CMAKE_COMMAND} -D "FILE=${_STEP2DD4HEP_ASSEMBLY_DIR}/assembly.xml" -D
      "STRING=step_name" -P "${_CHECK_CONTAINS}")

  add_test(
    NAME step2dd4hep_assembly_matmap_has_component
    COMMAND
      ${CMAKE_COMMAND} -D
      "FILE=${_STEP2DD4HEP_ASSEMBLY_DIR}/assembly_materials.xml" -D
      "STRING=Component" -P "${_CHECK_CONTAINS}")

  # ── 6. Non-existent file: tool must print an error ───────────────────────
  add_test(
    NAME step2dd4hep_missing_file_fails
    COMMAND $<TARGET_FILE:step2dd4hep> -o out.xml -m out_mat.xml
            nonexistent_file.step
    WORKING_DIRECTORY "${_STEP2DD4HEP_BADFILE_DIR}")
  set_tests_properties(step2dd4hep_missing_file_fails
                       PROPERTIES PASS_REGULAR_EXPRESSION "not found|failed to read")

  # ── 7. No arguments: tool must print usage ────────────────────────────────
  add_test(NAME step2dd4hep_no_args_fails COMMAND $<TARGET_FILE:step2dd4hep>)
  set_tests_properties(step2dd4hep_no_args_fails PROPERTIES PASS_REGULAR_EXPRESSION
                                                            "Usage:")

  # ── 8. --existing-map seeds known mappings (no TODO for Component) ────────
  add_test(
    NAME step2dd4hep_existing_map_runs
    COMMAND
      $<TARGET_FILE:step2dd4hep> --existing-map "${_STEP2DD4HEP_SEED_MAP}" -o
      seeded.xml -m seeded_materials.xml "${_STEP2DD4HEP_ASSEMBLY_STEP}"
    WORKING_DIRECTORY "${_STEP2DD4HEP_EXISTING_DIR}")

  add_test(
    NAME step2dd4hep_existing_map_no_todo
    COMMAND
      ${CMAKE_COMMAND} -D
      "FILE=${_STEP2DD4HEP_EXISTING_DIR}/seeded_materials.xml" -D
      "STRING=G4_Al" -P "${_CHECK_CONTAINS}")

  # ── Fixture chains ────────────────────────────────────────────────────────
  set_tests_properties(
    step2dd4hep_solid_compact_plugin step2dd4hep_solid_compact_path
    step2dd4hep_solid_matmap_exists PROPERTIES FIXTURES_REQUIRED
                                               step2dd4hep_solid_fixture)
  set_tests_properties(step2dd4hep_solid_runs
                       PROPERTIES FIXTURES_SETUP step2dd4hep_solid_fixture)

  set_tests_properties(
    step2dd4hep_assembly_compact_has_entry
    step2dd4hep_assembly_matmap_has_component
    PROPERTIES FIXTURES_REQUIRED step2dd4hep_assembly_fixture)
  set_tests_properties(step2dd4hep_assembly_runs
                       PROPERTIES FIXTURES_SETUP step2dd4hep_assembly_fixture)

  set_tests_properties(
    step2dd4hep_existing_map_no_todo PROPERTIES FIXTURES_REQUIRED
                                                step2dd4hep_existing_fixture)
  set_tests_properties(step2dd4hep_existing_map_runs
                       PROPERTIES FIXTURES_SETUP step2dd4hep_existing_fixture)

  # ── Timeouts ──────────────────────────────────────────────────────────────
  # Note: WORKING_DIRECTORY is intentionally not repeated here; each add_test()
  # already specifies the correct per-test directory and a later
  # set_tests_properties() call would override it.
  set_tests_properties(
    step2dd4hep_solid_runs step2dd4hep_assembly_runs
    step2dd4hep_existing_map_runs step2dd4hep_missing_file_fails
    step2dd4hep_no_args_fails PROPERTIES TIMEOUT 60)
  set_tests_properties(
    step2dd4hep_solid_compact_plugin step2dd4hep_solid_compact_path
    step2dd4hep_solid_matmap_exists step2dd4hep_assembly_compact_has_entry
    step2dd4hep_assembly_matmap_has_component step2dd4hep_existing_map_no_todo
    PROPERTIES TIMEOUT 10)
endif()
