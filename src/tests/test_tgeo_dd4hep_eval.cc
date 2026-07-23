// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 G4OCCT Contributors

#include "G4OCCT/TGeoOCCTSolid.hh"

#include <DD4hep/Shapes.h>

#include <gtest/gtest.h>

#include <filesystem>

TEST(TGeoOCCTSolidDD4hepEval, WrapsAsDD4hepSolid) {
  const auto step =
      std::filesystem::path(G4OCCT_TEST_SOURCE_DIR) / "fixtures" / "geometry" /
      "direct-primitives" / "G4Box" / "box-20x30x40-v1" / "shape.step";
  auto* raw = TGeoOCCTSolid::FromSTEP("dd4hep_box", step.string());
  dd4hep::Solid solid(raw);
  ASSERT_TRUE(solid.isValid());
  TGeoShape* shape = solid;
  ASSERT_NE(shape, nullptr);
  EXPECT_NE(dynamic_cast<TGeoOCCTSolid*>(shape), nullptr);
  EXPECT_STREQ(shape->GetName(), "dd4hep_box");
}
