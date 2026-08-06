# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

add_executable(step2dd4hep main.cc)

# step2dd4hep uses OCCT XDE directly and does not link against Geant4; it is
# a standalone geometry-inspection tool that only needs OpenCASCADE.
target_include_directories(step2dd4hep SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(step2dd4hep PRIVATE ${OpenCASCADE_LIBRARIES})

install(TARGETS step2dd4hep RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
