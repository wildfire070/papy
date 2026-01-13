#include "ClearCacheConfirmActivity.h"

#include <GfxRenderer.h>

#include "CacheManager.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"

void ClearCacheConfirmActivity::onEnter() {
  Activity::onEnter();
  render();
}

void ClearCacheConfirmActivity::loop() {
  // Handle selection change with Left/Right or Up/Down
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selection > 0) {
      selection--;
      render();
    }
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
             mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selection < 1) {
      selection++;
      render();
    }
  }

  // Handle confirm
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selection == 0) {
      performClear();
    } else {
      onComplete(true);  // Cancelled, but not an error
    }
    return;
  }

  // Handle back/cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onComplete(true);  // Cancelled, but not an error
    return;
  }
}

void ClearCacheConfirmActivity::performClear() {
  // Show clearing message
  renderer.clearScreen(THEME.backgroundColor);
  renderer.drawCenteredText(THEME.uiFontId, renderer.getScreenHeight() / 2, "Clearing cache...", THEME.primaryTextBlack);
  renderer.displayBuffer();

  // Perform the clear
  const int result = CacheManager::clearAllBookCaches();

  // Show result briefly
  renderer.clearScreen(THEME.backgroundColor);
  if (result >= 0) {
    char msg[64];
    if (result == 0) {
      snprintf(msg, sizeof(msg), "No caches to clear");
    } else if (result == 1) {
      snprintf(msg, sizeof(msg), "Cleared 1 book cache");
    } else {
      snprintf(msg, sizeof(msg), "Cleared %d book caches", result);
    }
    renderer.drawCenteredText(THEME.uiFontId, renderer.getScreenHeight() / 2, msg, THEME.primaryTextBlack);
  } else {
    renderer.drawCenteredText(THEME.uiFontId, renderer.getScreenHeight() / 2, "Failed to clear cache", THEME.primaryTextBlack);
  }
  renderer.displayBuffer();

  // Brief delay to show result
  vTaskDelay(1500 / portTICK_PERIOD_MS);

  onComplete(result >= 0);
}

void ClearCacheConfirmActivity::render() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto lineHeight = renderer.getLineHeight(THEME.uiFontId);
  const auto top = (pageHeight - lineHeight * 3) / 2;

  renderer.clearScreen(THEME.backgroundColor);

  // Title
  renderer.drawCenteredText(THEME.readerFontId, top - 40, "Clear Cache?", THEME.primaryTextBlack, BOLD);

  // Description
  renderer.drawCenteredText(THEME.uiFontId, top, "This will delete all book caches", THEME.primaryTextBlack);
  renderer.drawCenteredText(THEME.uiFontId, top + lineHeight, "and reading progress.", THEME.primaryTextBlack);

  // Yes/No buttons
  const int buttonY = top + lineHeight * 3;
  constexpr int buttonWidth = 60;
  constexpr int buttonSpacing = 30;
  constexpr int totalWidth = buttonWidth * 2 + buttonSpacing;
  const int startX = (pageWidth - totalWidth) / 2;

  // Draw "Yes" button
  if (selection == 0) {
    renderer.drawText(THEME.uiFontId, startX, buttonY, "[Yes]", THEME.primaryTextBlack);
  } else {
    renderer.drawText(THEME.uiFontId, startX + 4, buttonY, "Yes", THEME.primaryTextBlack);
  }

  // Draw "No" button
  if (selection == 1) {
    renderer.drawText(THEME.uiFontId, startX + buttonWidth + buttonSpacing, buttonY, "[No]", THEME.primaryTextBlack);
  } else {
    renderer.drawText(THEME.uiFontId, startX + buttonWidth + buttonSpacing + 4, buttonY, "No", THEME.primaryTextBlack);
  }

  // Help text
  renderer.drawCenteredText(THEME.smallFontId, pageHeight - 30, "LEFT/RIGHT: Select | OK: Confirm | BACK: Cancel",
                            THEME.primaryTextBlack);

  renderer.displayBuffer();
}
