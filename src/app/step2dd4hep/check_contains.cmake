# cmake-format: off
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 G4OCCT Contributors
# cmake-format: on

# check_contains.cmake — CTest helper that verifies FILE contains STRING.
# Invoked with: cmake -D FILE=<path> -D STRING=<needle> -P check_contains.cmake

if(NOT DEFINED FILE)
  message(FATAL_ERROR "FILE variable is not set")
endif()
if(NOT DEFINED STRING)
  message(FATAL_ERROR "STRING variable is not set")
endif()

if(NOT EXISTS "${FILE}")
  message(FATAL_ERROR "File does not exist: ${FILE}")
endif()

file(READ "${FILE}" _content)
string(FIND "${_content}" "${STRING}" _pos)
if(_pos EQUAL -1)
  message(
    FATAL_ERROR
      "String not found in ${FILE}:\n  expected: ${STRING}\n  content follows:\n${_content}"
  )
endif()
