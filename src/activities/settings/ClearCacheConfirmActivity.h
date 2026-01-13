#pragma once

#include <functional>

#include "activities/Activity.h"

class ClearCacheConfirmActivity final : public Activity {
  const std::function<void(bool success)> onComplete;
  int selection = 0;  // 0 = Yes, 1 = No

  void render() const;
  void performClear();

 public:
  explicit ClearCacheConfirmActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     std::function<void(bool success)> onComplete)
      : Activity("ClearCacheConfirm", renderer, mappedInput), onComplete(std::move(onComplete)) {}

  void onEnter() override;
  void loop() override;
};
