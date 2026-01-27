#pragma once

#include <cstdint>

#include "../ui/views/SyncViews.h"
#include "State.h"

class GfxRenderer;

namespace papyrix {

class SyncState : public State {
 public:
  explicit SyncState(GfxRenderer& renderer);
  ~SyncState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Sync; }

 private:
  GfxRenderer& renderer_;
  ui::SyncMenuView menuView_;
  bool needsRender_;
  bool goHome_;
  bool goNetwork_;
};

}  // namespace papyrix
