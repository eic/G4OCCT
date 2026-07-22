# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

cmake_minimum_required(VERSION 3.16)

# ─── DD4hep detector-element plugins ─────────────────────────────────────────
#
# dd4hep_add_plugin() is provided by DD4hepConfig.cmake.  It produces a shared
# library (libG4OCCTDD4hep.so) and registers it with DD4hep's factory system.

# DD4hep's released dd4hep_generate_rootmap() does not yet expose the
# DD4HEP_GENERATE_ROOTMAP_EXTRA_ENV hook added upstream in
# AIDASoft/DD4hep@83da461fbb9d19a95ad2c16b11168b56334ccf51.  Under ASAN/TSAN the
# uninstrumented listcomponents helper dlopens our instrumented plugin and fails
# before the sanitizer runtime is initialised.  Override the macro locally until
# that DD4hep change is released.
if(USE_ASAN OR USE_TSAN)
  unset(_g4occt_preload_sanitizer_lib)

  if(USE_ASAN)
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
      OUTPUT_VARIABLE _g4occt_preload_sanitizer_lib_asan
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "${_g4occt_preload_sanitizer_lib_asan}")
      list(APPEND _g4occt_preload_sanitizer_lib
           "${_g4occt_preload_sanitizer_lib_asan}")
    endif()
  endif()

  if(USE_TSAN)
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libtsan.so
      OUTPUT_VARIABLE _g4occt_preload_sanitizer_lib_tsan
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(EXISTS "${_g4occt_preload_sanitizer_lib_tsan}")
      list(APPEND _g4occt_preload_sanitizer_lib
           "${_g4occt_preload_sanitizer_lib_tsan}")
    endif()
  endif()

  function(dd4hep_generate_rootmap library)
    if(APPLE)
      set(ENV_VAR DYLD_LIBRARY_PATH)
    else()
      set(ENV_VAR LD_LIBRARY_PATH)
    endif()
    set(${ENV_VAR}_VALUE
        $<TARGET_FILE_DIR:${library}>:$<TARGET_FILE_DIR:DD4hep::DD4hepGaudiPluginMgr>:$ENV{${ENV_VAR}})

    set(rootmapfile ${CMAKE_SHARED_MODULE_PREFIX}${library}.components)

    string(JOIN ":" LD_PRELOAD_VALUE ${_g4occt_preload_sanitizer_lib})
    if(LD_PRELOAD_VALUE)
      set(EXTRA_ENV
          "LD_PRELOAD=${LD_PRELOAD_VALUE}"
          "ASAN_OPTIONS=detect_leaks=0:verify_asan_link_order=0"
          "LSAN_OPTIONS=detect_leaks=0")
    endif()

    add_custom_command(
      OUTPUT ${rootmapfile}
      DEPENDS ${library}
      COMMAND
        ${CMAKE_COMMAND} -E env "${ENV_VAR}=${${ENV_VAR}_VALUE}" ${EXTRA_ENV}
        "ROOT_LIBRARY_PATH=${DD4HEP_ROOT_LIBRARY_PATH}"
        $<TARGET_FILE:DD4hep::listcomponents> -o ${rootmapfile}
        $<TARGET_FILE_NAME:${library}>
      WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH}
      COMMAND_EXPAND_LISTS)

    add_custom_target(Components_${library} ALL DEPENDS ${rootmapfile})
    install(FILES $<TARGET_FILE_DIR:${library}>/${rootmapfile}
            DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endfunction()
endif()

dd4hep_add_plugin(
  G4OCCTDD4hep
  SOURCES
  G4OCCT_STEPSolid.cc
  G4OCCT_STEPSolid_impl.cc
  G4OCCT_STEPAssembly.cc
  G4OCCT_STEPAssembly_impl.cc
  G4OCCT_STEPAssemblySD.cc)

target_link_libraries(G4OCCTDD4hep PRIVATE G4OCCT::G4OCCT DD4hep::DDCore)

# ─── Tests ────────────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
