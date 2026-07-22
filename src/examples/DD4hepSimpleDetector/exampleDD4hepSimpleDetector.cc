// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include <DD4hep/DetElement.h>
#include <DD4hep/Detector.h>

#include <dlfcn.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef G4OCCT_DD4HEP_SIMPLE_COMPACT
#error "G4OCCT_DD4HEP_SIMPLE_COMPACT must be defined by CMake"
#endif
#ifndef G4OCCT_DD4HEP_PLUGIN_LIBRARY
#error "G4OCCT_DD4HEP_PLUGIN_LIBRARY must be defined by CMake"
#endif

namespace {

void RequireChild(const dd4hep::DetElement::Children& children, const std::string& name) {
  if (children.find(name) == children.end()) {
    throw std::runtime_error("Expected detector element not found: " + name);
  }
}

void LoadDd4hepPlugin() {
  void* handle = dlopen(G4OCCT_DD4HEP_PLUGIN_LIBRARY, RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    throw std::runtime_error("Failed to load DD4hep plugin library '" +
                             std::string{G4OCCT_DD4HEP_PLUGIN_LIBRARY} +
                             "': " + dlerror());
  }
}

} // namespace

int main(int argc, char** argv) {
  const std::string compact_path =
      argc > 1 ? argv[1] : std::string{G4OCCT_DD4HEP_SIMPLE_COMPACT};

  try {
    if (!std::filesystem::exists(compact_path)) {
      throw std::runtime_error("Compact XML not found: " + compact_path);
    }

    LoadDd4hepPlugin();

    dd4hep::Detector& detector = dd4hep::Detector::getInstance();
    detector.fromCompact(compact_path);

    const auto& children = detector.world().children();
    std::size_t support_count = 0;
    std::size_t sensor_count  = 0;

    for (const auto& [name, child] : children) {
      if (!child.isValid() || !child.placement().isValid()) {
        throw std::runtime_error("Invalid detector element placement for: " + name);
      }
      if (name.rfind("VXDLayer0Support_", 0) == 0) {
        ++support_count;
      } else if (name.rfind("VXDLayer0Sensor_", 0) == 0) {
        ++sensor_count;
      }
    }

    RequireChild(children, "VXDLayer0Support_0");
    RequireChild(children, "VXDLayer0Support_9");
    RequireChild(children, "VXDLayer0Sensor_0");
    RequireChild(children, "VXDLayer0Sensor_9");

    if (support_count != 10 || sensor_count != 10) {
      throw std::runtime_error("Expected 10 support ladders and 10 sensor ladders, got " +
                               std::to_string(support_count) + " support and " +
                               std::to_string(sensor_count) + " sensor");
    }

    std::cout << "Loaded " << compact_path << " with " << support_count
              << " STEP support ladders and " << sensor_count
              << " STEP sensor ladders from the SimpleDetector layer-0 port.\n";

    dd4hep::Detector::destroyInstance();
    return 0;
  } catch (const std::exception& ex) {
    dd4hep::Detector::destroyInstance();
    std::cerr << "exampleDD4hepSimpleDetector: " << ex.what() << '\n';
    return 1;
  }
}
