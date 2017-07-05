// Copyright 2016 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <sstream>

#include "flatui/flatui.h"
#include "flatui/flatui_common.h"
#include "fplbase/input.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "motive/engine.h"
#include "motive/init.h"

using flatui::AnimCurveDescription;
using flatui::kAnimEaseInEaseOut;
using flatui::kAnimSpring;
using flatui::EndGroup;
using flatui::HashedId;
using flatui::Margin;
using flatui::Run;
using flatui::SequenceId;
using flatui::SetVirtualResolution;
using flatui::StartGroup;
using mathfu::vec2i;
using mathfu::vec2;
using mathfu::vec3;
using mathfu::vec4;
using motive::MotiveEngine;
using motive::MotiveTime;

// Enum to keep track of the textures used in the game.
enum CorgiTexture {
  kCorgiTextureBackground,
  kCorgiTextureHappyHead,
  kCorgiTextureNeutralHead,
  kCorgiTextureHorn,
  kCorgiTextureCorgiBody,
  kCorgiTextureWings,
  kCorgiTextureHeartIcon,
  kCorgiTextureShadow,
  kCorgiTextureClouds,
  kCorgiTextureCount,
};

// These curves describe the motion of our animations.
// See the comment above kCurveDescriptionin in flatuianim.cpp for an
// in-depth description.
static const AnimCurveDescription kColorCurveDescription(kAnimEaseInEaseOut,
                                                         1.0f, 15000.0f, 0.8f);
static const AnimCurveDescription kPointsColorCurveDescription(
    kAnimEaseInEaseOut, 1.0f, 4000.0f, 0.5f);
static const AnimCurveDescription kSpriteCurveDescription(kAnimEaseInEaseOut,
                                                          10.0f, 2000.0f, 0.5f);
static const AnimCurveDescription kScoreSizeGrowCurveDescription(
    kAnimEaseInEaseOut, 10.0f, 2000.0f, 0.1f);
static const AnimCurveDescription kScoreSizeShrinkCurveDescription(
    kAnimEaseInEaseOut, 10.0f, 2000.0f, 0.5f);
static const AnimCurveDescription kPointSizeCurveDescription(kAnimEaseInEaseOut,
                                                             10.0f, 2000.0f,
                                                             0.5f);
static const AnimCurveDescription kHeartSizeCurveDescription(kAnimEaseInEaseOut,
                                                             10.0f, 6000.0f,
                                                             0.5f);
static const AnimCurveDescription kSpringCurveDescription(kAnimSpring, 20.0f,
                                                          2000.0f, 0.5f);
static const AnimCurveDescription kCloudCurveDescription(kAnimSpring, 100.0f,
                                                         40000.0f, 1.0f);

// Constant variables to be used.
// Note, the kCorgiScale variable is used to scale all the separate corgi
// assets so that they look correct together on screen. The corgi asset
// related constants have already been set to look proportional/correctly
// offset. To change the overall size of the corgi, modify kCorgiScale.
// If the size is changed on the corgi, the sprites' start positions
// may have to be edited manually.
static const float kCorgiScale = 4.0f / 3.0f;
static const vec2i kWindowSize = vec2i(500, 1920);
static const vec4 kWhiteColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
static const vec4 kPinkColor = vec4(1.0f, 0.47f, 0.56f, 1.0f);
static const vec4 kGreyColor = vec4(0.5f, 0.5f, 0.5f, 0.5f);
static const vec4 kTransparent = vec4(1.0f, 0.47f, 0.56f, 0.0f);
static const vec2 kScoreLabelOffset = vec2(0.0f, 30.0f);
static const vec2 kCorgiHornOffset = vec2(-60.0f, -700.0f) * kCorgiScale;
static const vec2 kCorgiShadowOffset = vec2(12.0f, 70.0f) * kCorgiScale;
static const float kSpriteYTargetPosition = -1100.0f;
static const float kSpriteYStartPosition = 0.0f;
static const float kYVirtualResolution = 1080.0f;
static const float kCorgiBodySize = 600.0f * kCorgiScale;
static const float kCorgiHeadSize = 400.0f * kCorgiScale;
static const float kCorgiHeadBottomOffset = -20.0f * kCorgiScale;
static const float kCorgiHeadBottomMargin = 275.0f * kCorgiScale;
static const float kCorgiHeadLeftOffset = -50.0f * kCorgiScale;
static const float kDefaultLabelSize = 150.0f;
static const float kLargeLabelSize = 300.0f;
static const float kLoadingLabelSize = 100.0f;
static const float kLabelSizeIncrement = 0.3f;
static const float kDefaultIconSize = 60.0f;
static const float kLargeIconSize = 160.0f;
static const float kYPointsSizeTarget = 150.0f;
static const float kYPointsSizeStart = 20.0f;
static const float kCorgiLift = -160.0f * kCorgiScale;
static const float kCorgiHornSize = 275.0f * kCorgiScale;
static const float kCorgiShadowSize = 650.0f * kCorgiScale;
static const float kCorgiWingSize = 850.0f * kCorgiScale;
static const float kCloudSize = 900.0f;
static const float kCloudCenterPosition = 10.0f;
static const float kCloudEndXPosition = 700.0f;
static const float kCloudYPosition = -500.0f;
static const float kCloudOffset = 900.0f;
static const int kEvolutionPoint = 100;
static const int kClickScore = 10;
static const int kNumClouds = 3;
static const char kCorguiTitleBanner[] = "CorgUI";
static const char kSpriteYPosId[] = "sprites_ascending";
static const char kFlashId[] = "text_flashing";
static const char kSizeId[] = "label_size_changing";
static const char kHeartSizeId[] = "heart_size_changing";
static const char kCurveId[] = "text_curving";
static const char kTextColorId[] = "text_color_changing";
static const char kScoreColorId[] = "score_color_changing";
static const char kScoreId[] = "score_size_changing";
static const char kLoadingString[] = "Loading CorgUI";
static const char kSpringId[] = "spring_x";
static const char kScrollId[] = "scroll_cloud";
static const vec4 kTextColors[] = {
    vec4(1.0f, 1.0f, 1.0f, 1.0f), vec4(0.87f, 0.52f, 0.54f, 1.0f),
    vec4(0.0f, 1.0f, 0.0f, 1.0f), vec4(0.0f, 1.0f, 1.0f, 1.0f),
};

struct corgi_texture_package {
  const char *textureName;
  bool usesAlpha;
};

static const corgi_texture_package kCorgiTexturePackages[] = {
    {"textures/corgi_bg.webp", false},
    {"textures/corgi_happy.webp", true},
    {"textures/corgi_neutral.webp", true},
    {"textures/corgi_horn.webp", true},
    {"textures/corgi_body.webp", true},
    {"textures/corgi_wings.webp", true},
    {"textures/heart_icon.webp", true},
    {"textures/corgi_shadow.webp", true},
    {"textures/scrolling_clouds.webp", true}};

static const float kSpriteXStartPositions[] = {
    30.0f, 140.0f, 60.0f, 100.0f, 80.0f,
};
static const char kFontPath[] = "fonts/LuckiestGuy.ttf";
static_assert(FPL_ARRAYSIZE(kCorgiTexturePackages) == kCorgiTextureCount,
              "kCorgiTextureNames not consistent with CorgiTexture enum");

// Struct to hold the state of the game.
struct CorgiGameState {
  CorgiGameState()
      : score_text_color_target(0),
        sprite_start(0),
        score(0),
        score_label_size(kDefaultLabelSize),
        score_label_target(kDefaultLabelSize),
        movement_sign(-1.0f),
        score_text_color(kWhiteColor),
        start_cloud(false) {}
  int score_text_color_target;
  int sprite_start;
  int score;
  float score_label_size;
  float score_label_target;
  float movement_sign;
  vec4 score_text_color;
  bool start_cloud;
};

static const std::string CalculateStringFromInt(int int_to_change) {
  std::stringstream ss;
  ss << int_to_change;
  return ss.str();
}

extern "C" int FPL_main(int /*argc*/, char **argv) {
  MotiveEngine motive_engine;

  // Set up the renderer not to use hardware scaler if we are on Android.
  fplbase::Renderer renderer;
  renderer.Initialize(kWindowSize, kCorguiTitleBanner,
                      fplbase::kWindowModeWindowedNative);

  // Set up the input system.
  fplbase::InputSystem input;
  input.Initialize();

  // Set the local directory to the assets folder for this sample.
  bool result = fplbase::ChangeToUpstreamDir(argv[0], "assets");
  assert(result);
  (void)result;

  // Set up the asset and font managers.
  fplbase::AssetManager assetman(renderer);
  flatui::FontManager fontman;
  fontman.Open(kFontPath);

  // Initialize variables that will be used in the demo.
  CorgiGameState curr_game;

  // Load up the textures that will be used by the demo.
  const fplbase::Texture *corgi_textures[kCorgiTextureCount];

  for (int i = 0; i < kCorgiTextureCount; ++i) {
    corgi_textures[i] = assetman.LoadTexture(
        kCorgiTexturePackages[i].textureName,
        kCorgiTexturePackages[i].usesAlpha ? fplbase::kFormat8888
                                             : fplbase::kFormatAuto);
  }

  assetman.StartLoadingTextures();

  while (!(input.exit_requested())) {
    // Advance to the next frame to update our screen.
    input.AdvanceFrame(&renderer.window_size());
    // If the back button has been hit on Android, exit the app.
    if (input.GetButton(fplbase::FPLK_AC_BACK).went_down()) {
      break;
    }
    renderer.AdvanceFrame(input.minimized(), input.Time());
    motive_engine.AdvanceFrame(static_cast<MotiveTime>(
        input.DeltaTime() * flatui::kSecondsToMotiveTime));
    renderer.ClearFrameBuffer(mathfu::kZeros4f);

    std::string curr_game_score = CalculateStringFromInt(curr_game.score);

    // Define flatui block. Note that the block is executed multiple times,
    // One for a layout pass and another for a rendering pass.
    Run(assetman, fontman, input, &motive_engine, [&]() {
      // Set a virtual resolution all coordinates will use. 1080 is now the
      // size of the smallest dimension (Y in landscape mode).
      SetVirtualResolution(kYVirtualResolution);

      // Display loading screen until assets have loaded.
      if (!assetman.TryFinalize()) {
        StartGroup(flatui::kLayoutHorizontalCenter);
        PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                      mathfu::kZeros2f);
        flatui::Label(kLoadingString, kLoadingLabelSize);
        EndGroup();
        return;
      }

      flatui::RenderTexture(*corgi_textures[kCorgiTextureBackground],
                            mathfu::kZeros2i, renderer.window_size());

      // Start outer group.
      StartGroup(flatui::kLayoutOverlay);
      PositionGroup(flatui::kAlignLeft, flatui::kAlignTop, mathfu::kZeros2f);

      // Set the colors of the button.
      flatui::SetHoverClickColor(mathfu::kZeros4f, mathfu::kZeros4f);

      // Create groups for the corgi button.
      // Draw corgi's shadow.
      StartGroup(flatui::kLayoutOverlay);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                    kCorgiShadowOffset);
      flatui::Image(*corgi_textures[kCorgiTextureShadow], kCorgiShadowSize);
      EndGroup();

      // Draw the corgi's wings if it has evolved twice.
      // The wing evolution point is being marked as three times
      // our kEvolutionPoint.
      if (curr_game.score >= kEvolutionPoint * 3.0f) {
        StartGroup(flatui::kLayoutOverlay);
        PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                      vec2(-30.0f, -165.0f));
        flatui::Image(*corgi_textures[kCorgiTextureWings], kCorgiWingSize);
        EndGroup();
      }
      StartGroup(flatui::kLayoutOverlay);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                    vec2(0.0f, kCorgiLift));
      flatui::Image(*corgi_textures[kCorgiTextureCorgiBody], kCorgiBodySize);
      EndGroup();

      // Create group for the head and offset it so it's above the body.
      StartGroup(flatui::kLayoutOverlay);
      PositionGroup(
          flatui::kAlignCenter, flatui::kAlignBottom,
          vec2(kCorgiHeadLeftOffset, kCorgiHeadBottomOffset + kCorgiLift));
      // Give the button a bottom margin so that parts of its body will
      // also be clickable. The offset is needed to keep the head in the correct
      // position while only making the corgi's body and not the area below
      // it clickable.
      auto corgi_button = ToggleImageButton(
          *corgi_textures[kCorgiTextureNeutralHead],
          *corgi_textures[kCorgiTextureHappyHead], kCorgiHeadSize,
          Margin(0.0f, 0.0f, 0.0f,
                 kCorgiHeadBottomMargin + kCorgiHeadBottomOffset),
          "corgi_head");
      EndGroup();
      // Draw the corgi's horn if it has evolved once.
      if (curr_game.score >= kEvolutionPoint) {
        StartGroup(flatui::kLayoutOverlay);
        PositionGroup(flatui::kAlignCenter, flatui::kAlignBottom,
                      kCorgiHornOffset);
        flatui::Image(*corgi_textures[kCorgiTextureHorn], kCorgiHornSize);
        EndGroup();
      }

      // Check to see if the button has been clicked and respond.
      if (corgi_button & flatui::kEventWentUp) {
        // Increment the score.
        curr_game.score += kClickScore;
        // Flip the movement sign.
        curr_game.movement_sign *= -1.0f;
        // Update heart start position.
        curr_game.sprite_start = (curr_game.sprite_start + 1) %
                                 FPL_ARRAYSIZE(kSpriteXStartPositions);

        // Animate the score label size changing.
        curr_game.score_label_target =
            std::max(curr_game.score_label_target, curr_game.score_label_size);
        curr_game.score_label_target +=
            kLabelSizeIncrement *
            (kLargeLabelSize - curr_game.score_label_target);
        flatui::StartAnimation<float>(kScoreId, curr_game.score_label_target,
                                      0.0f, kScoreSizeGrowCurveDescription);

        // Add points and heart sprites. Both will grow and float upwards
        // until offscreen. The points will fade in color until transparent.
        SequenceId seq = flatui::AddSprite(
            kSpriteYPosId,
            [&corgi_textures, &curr_game](SequenceId seq) -> bool {
              // Set up the animatable for the sprites' y positions.
              const HashedId y_position_sprite_hash =
                  flatui::HashedSequenceId(kSpriteYPosId, seq);
              const float sprite_position = flatui::Animatable<float>(
                  y_position_sprite_hash, kSpriteYStartPosition);

              // Change the text size.
              const HashedId label_size_sprite_hash =
                  flatui::HashedSequenceId(kSizeId, seq);
              const float label_size = flatui::Animatable<float>(
                  label_size_sprite_hash, kYPointsSizeStart);

              // Change the text color.
              const HashedId text_color_sprite_hash =
                  flatui::HashedSequenceId(kTextColorId, seq);
              const vec4 text_color =
                  flatui::Animatable<vec4>(text_color_sprite_hash, kPinkColor);

              // Change the heart size.
              const HashedId heart_size_sprite_hash =
                  flatui::HashedSequenceId(kHeartSizeId, seq);
              const float heart_size = flatui::Animatable<float>(
                  heart_size_sprite_hash, kDefaultIconSize);

              // Start the sprite's group.
              StartGroup(flatui::kLayoutVerticalCenter, 0,
                         CalculateStringFromInt(seq).c_str());
              // Position the group to where the animation currently has
              // the spring animation and sprite_position.
              const HashedId spring_sprite_hash =
                  flatui::HashedSequenceId(kSpringId, seq);
              const float curr_sprite_x = flatui::Animatable(
                  spring_sprite_hash,
                  -curr_game.movement_sign *
                      kSpriteXStartPositions[curr_game.sprite_start]);
              PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                            vec2(curr_sprite_x, sprite_position));
              flatui::SetTextColor(text_color);
              flatui::Label(CalculateStringFromInt(kClickScore).c_str(),
                            label_size);
              flatui::Image(*corgi_textures[kCorgiTextureHeartIcon],
                            heart_size);
              EndGroup();

              const bool no_time_remaining =
                  flatui::AnimationTimeRemaining(y_position_sprite_hash) <= 0 &&
                  flatui::AnimationTimeRemaining(label_size_sprite_hash) <= 0 &&
                  flatui::AnimationTimeRemaining(text_color_sprite_hash) <= 0 &&
                  flatui::AnimationTimeRemaining(spring_sprite_hash) <= 0 &&
                  flatui::AnimationTimeRemaining(heart_size_sprite_hash) <= 0;
              return no_time_remaining;
            });
        // Find and start position sprite animation.
        const HashedId y_position_sprite_hash =
            flatui::HashedSequenceId(kSpriteYPosId, seq);
        const HashedId spring_sprite_hash =
            flatui::HashedSequenceId(kSpringId, seq);
        flatui::StartAnimation<float>(y_position_sprite_hash,
                                      kSpriteYTargetPosition, 0.0f,
                                      kSpriteCurveDescription);
        flatui::StartAnimation<float>(
            spring_sprite_hash,
            curr_game.movement_sign *
                kSpriteXStartPositions[curr_game.sprite_start],
            0.0f, kSpringCurveDescription);

        // Find and start the size sprite animations.
        const HashedId size_sprite_hash =
            flatui::HashedSequenceId(kSizeId, seq);
        flatui::StartAnimation<float>(size_sprite_hash, kYPointsSizeTarget,
                                      0.0f, kSpriteCurveDescription);
        const HashedId heart_size_sprite_hash =
            flatui::HashedSequenceId(kHeartSizeId, seq);
        flatui::StartAnimation<float>(heart_size_sprite_hash, kLargeIconSize,
                                      0.0f, kHeartSizeCurveDescription);

        // Find and start text color sprite animations.
        const HashedId text_color_sprite_hash =
            flatui::HashedSequenceId(kTextColorId, seq);
        flatui::StartAnimation<vec4>(text_color_sprite_hash, kTransparent,
                                     mathfu::kZeros4f, kColorCurveDescription);

        // Find and start the flash sprite animations, if we've hit an evolution
        // point mark.
        if (curr_game.score % kEvolutionPoint == 0) {
          curr_game.score_text_color_target =
              (curr_game.score_text_color_target + 1) %
              FPL_ARRAYSIZE(kTextColors);
          // Animate the score colors.
          flatui::StartAnimation<vec4>(
              kScoreColorId, kTextColors[curr_game.score_text_color_target],
              mathfu::kZeros4f, kPointsColorCurveDescription);
        }
      }

      // Draw a stream of kNumClouds clouds.
      const float sprite_position =
          flatui::Animatable<float>(kScrollId, kCloudEndXPosition);
      for (int i = 0; i < kNumClouds; ++i) {
        flatui::StartGroup(flatui::kLayoutHorizontalTop);
        PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                      vec2((sprite_position - i * kCloudSize) + kCloudOffset,
                           kCloudYPosition));
        flatui::Image(*corgi_textures[kCorgiTextureClouds], kCloudSize);
        flatui::EndGroup();
      }
      // Start the cloud scrolling if not started yet.
      if (!curr_game.start_cloud) {
        curr_game.start_cloud = true;
        flatui::StartAnimation<float>(kScrollId, -kCloudCenterPosition, 0.0f,
                                      kCloudCurveDescription);
      }
      // Draw any sprites that may have been added.
      flatui::DrawSprites(kSpriteYPosId);
      flatui::DrawSprites(kFlashId);
      flatui::DrawSprites(kSizeId);
      flatui::DrawSprites(kCurveId);

      // Create a score label to display the current score.
      // When the button is clicked, this label will grow
      // larger. It will grow in size about thirty percent of
      // kLargeLabelSize subtracted by either the current size
      // target or the current size, whichever is larger.
      // With this, it will grow larger with each consecutive,
      // continued click.
      // Once the clicking has stopped, the label will shrink
      // back down to its original size.
      // Set the text color of the score.
      curr_game.score_text_color =
          flatui::Animatable<vec4>(kScoreColorId, kWhiteColor);
      flatui::SetTextColor(curr_game.score_text_color);

      StartGroup(flatui::kLayoutHorizontalTop);
      PositionGroup(flatui::kAlignCenter, flatui::kAlignTop, kScoreLabelOffset);
      flatui::SetTextColor(curr_game.score_text_color);
      // Animate the score label size.
      curr_game.score_label_size =
          flatui::Animatable<float>(kScoreId, kDefaultLabelSize);
      flatui::Label(curr_game_score.c_str(),
                    std::max(kDefaultLabelSize, curr_game.score_label_size),
                    flatui::HashId("curr_game_score"));
      EndGroup();

      // Animate the score being changed.
      if (flatui::AnimationTimeRemaining(kScoreId) <= 0) {
        flatui::StartAnimation<float>(kScoreId, kDefaultLabelSize, 0.0f,
                                      kScoreSizeShrinkCurveDescription);
        curr_game.score_label_target = kDefaultLabelSize;
      }

      if (flatui::AnimationTimeRemaining(kScoreColorId) <= 0 &&
          curr_game.score_text_color_target != 0) {
        curr_game.score_text_color_target =
            (curr_game.score_text_color_target + 1) %
            FPL_ARRAYSIZE(kTextColors);
        flatui::StartAnimation<vec4>(
            kScoreColorId, kTextColors[curr_game.score_text_color_target],
            mathfu::kZeros4f, kPointsColorCurveDescription);
      }

      EndGroup();
    });
  }
  return 0;
}
