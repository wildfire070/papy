#include "NetworkModeSelectionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "ThemeManager.h"
#include "config.h"

namespace {
constexpr int MENU_ITEM_COUNT = 2;
const char* MENU_ITEMS[MENU_ITEM_COUNT] = {"Join a Network", "Create Hotspot"};
const char* MENU_DESCRIPTIONS[MENU_ITEM_COUNT] = {"Connect to an existing WiFi network",
                                                  "Create a WiFi network others can join"};
}  // namespace

void NetworkModeSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<NetworkModeSelectionActivity*>(param);
  self->displayTaskLoop();
}

void NetworkModeSelectionActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection
  selectedIndex = 0;

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&NetworkModeSelectionActivity::taskTrampoline, "NetworkModeTask",
              2048,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void NetworkModeSelectionActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void NetworkModeSelectionActivity::loop() {
  // Handle back button - cancel
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onCancel();
    return;
  }

  // Handle confirm button - select current option
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    const NetworkMode mode = (selectedIndex == 0) ? NetworkMode::JOIN_NETWORK : NetworkMode::CREATE_HOTSPOT;
    onModeSelected(mode);
    return;
  }

  // Handle navigation
  const bool prevPressed = mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool nextPressed = mappedInput.wasPressed(MappedInputManager::Button::Down) ||
                           mappedInput.wasPressed(MappedInputManager::Button::Right);

  if (prevPressed) {
    selectedIndex = (selectedIndex + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
    updateRequired = true;
  } else if (nextPressed) {
    selectedIndex = (selectedIndex + 1) % MENU_ITEM_COUNT;
    updateRequired = true;
  }
}

void NetworkModeSelectionActivity::displayTaskLoop() {
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

void NetworkModeSelectionActivity::render() const {
  renderer.clearScreen(THEME.backgroundColor);

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // Draw header
  renderer.drawCenteredText(THEME.readerFontId, 10, "File Transfer", THEME.primaryTextBlack, BOLD);

  // Draw subtitle
  renderer.drawCenteredText(THEME.uiFontId, 50, "How would you like to connect?", THEME.primaryTextBlack, REGULAR);

  // Draw menu items as grid boxes (matching HomeActivity style)
  constexpr int itemHeight = 80;
  constexpr int itemWidth = 400;
  constexpr int itemGap = 10;
  const int itemX = (pageWidth - itemWidth) / 2;
  const int startY = (pageHeight - (MENU_ITEM_COUNT * (itemHeight + itemGap) - itemGap)) / 2;

  for (int i = 0; i < MENU_ITEM_COUNT; i++) {
    const int itemY = startY + i * (itemHeight + itemGap);
    const bool isSelected = (i == selectedIndex);

    if (isSelected) {
      // Selected - filled background (like HomeActivity)
      renderer.fillRect(itemX, itemY, itemWidth, itemHeight, THEME.selectionFillBlack);
    } else {
      // Not selected - bordered (like HomeActivity)
      renderer.drawRect(itemX, itemY, itemWidth, itemHeight, THEME.primaryTextBlack);
    }

    // Draw title and description centered within box
    const bool textColor = isSelected ? THEME.selectionTextBlack : THEME.primaryTextBlack;

    // Title - centered horizontally, positioned in upper portion
    const int titleWidth = renderer.getTextWidth(THEME.uiFontId, MENU_ITEMS[i]);
    const int titleX = itemX + (itemWidth - titleWidth) / 2;
    const int titleY = itemY + 10;
    renderer.drawText(THEME.uiFontId, titleX, titleY, MENU_ITEMS[i], textColor);

    // Description - centered horizontally, positioned in lower portion
    const int descWidth = renderer.getTextWidth(THEME.smallFontId, MENU_DESCRIPTIONS[i]);
    const int descX = itemX + (itemWidth - descWidth) / 2;
    const int descY = itemY + 55;
    renderer.drawText(THEME.smallFontId, descX, descY, MENU_DESCRIPTIONS[i], textColor);
  }

  // Draw help text at bottom
  const auto labels = mappedInput.mapLabels("« Back", "Select", "", "");
  renderer.drawButtonHints(THEME.uiFontId, labels.btn1, labels.btn2, labels.btn3, labels.btn4, THEME.primaryTextBlack);

  renderer.displayBuffer();
}
