#include "OpdsServerListActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "config.h"

namespace {
std::string truncateWithEllipsis(const std::string& str, size_t maxLen) {
  if (str.length() <= maxLen) return str;
  return str.substr(0, maxLen - 3) + "...";
}
}  // namespace

void OpdsServerListActivity::taskTrampoline(void* param) {
  auto* self = static_cast<OpdsServerListActivity*>(param);
  self->displayTaskLoop();
}

void OpdsServerListActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void OpdsServerListActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Load servers from file
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  OPDS_STORE.loadFromFile();
  xSemaphoreGive(renderingMutex);

  selectedIndex = 0;
  updateRequired = true;

  xTaskCreate(&OpdsServerListActivity::taskTrampoline, "OpdsListTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void OpdsServerListActivity::onExit() {
  Activity::onExit();

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void OpdsServerListActivity::render() const {
  renderer.clearScreen(THEME.backgroundColor);

  // Title
  renderer.drawCenteredText(THEME.readerFontId, 10, "Net Library", THEME.primaryTextBlack, BOLD);

  constexpr int startY = 50;
  constexpr int itemHeight = 55;
  constexpr int leftMargin = 25;

  const auto& servers = OPDS_STORE.getServers();

  if (servers.empty()) {
    // Show instructions when no servers configured
    renderer.drawCenteredText(THEME.uiFontId, 150, "No servers configured", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.uiFontId, 180, "Edit /opds.ini on SD card", THEME.primaryTextBlack);
    renderer.drawCenteredText(THEME.uiFontId, 210, "to add OPDS servers", THEME.primaryTextBlack);
  } else {
    // Draw each server
    for (size_t i = 0; i < servers.size(); i++) {
      const int y = startY + static_cast<int>(i) * itemHeight;
      const bool isSelected = (static_cast<int>(i) == selectedIndex);
      const auto& server = servers[i];

      const std::string displayName = truncateWithEllipsis(server.name, 30);

      if (isSelected) {
        renderer.drawText(THEME.uiFontId, leftMargin, y, ">", THEME.primaryTextBlack);
        renderer.drawText(THEME.uiFontId, leftMargin + 15, y, displayName.c_str(), THEME.primaryTextBlack, BOLD);
      } else {
        renderer.drawText(THEME.uiFontId, leftMargin + 15, y, displayName.c_str(), THEME.primaryTextBlack);
      }

      // Show URL below name in smaller text
      const std::string displayUrl = truncateWithEllipsis(server.url, 35);
      renderer.drawText(THEME.uiFontId, leftMargin + 25, y + 25, displayUrl.c_str(), THEME.primaryTextBlack);
    }
  }

  // Button hints
  const auto labels = mappedInput.mapLabels("Back", servers.empty() ? "" : "Connect", "", "");
  renderer.drawButtonHints(THEME.uiFontId, labels.btn1, labels.btn2, labels.btn3, labels.btn4, THEME.primaryTextBlack);
  renderer.displayBuffer();
}

void OpdsServerListActivity::loop() {
  const auto& servers = OPDS_STORE.getServers();
  const int maxIndex = servers.empty() ? 0 : static_cast<int>(servers.size()) - 1;

  // Navigation - Up
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selectedIndex > 0) {
      selectedIndex--;
      updateRequired = true;
    }
  }

  // Navigation - Down
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selectedIndex < maxIndex) {
      selectedIndex++;
      updateRequired = true;
    }
  }

  // Confirm - Select server
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!servers.empty() && selectedIndex < static_cast<int>(servers.size())) {
      const auto* server = OPDS_STORE.getServer(selectedIndex);
      if (server && onServerSelected) {
        onServerSelected(*server);
      }
    }
  }

  // Back - go back to settings
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (onGoBack) {
      onGoBack();
    }
  }
}
