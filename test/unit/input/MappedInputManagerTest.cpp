#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Minimal InputManager mock
class InputManager {
 public:
  static constexpr int BTN_BACK = 0;
  static constexpr int BTN_CONFIRM = 1;
  static constexpr int BTN_LEFT = 2;
  static constexpr int BTN_RIGHT = 3;
  static constexpr int BTN_UP = 4;
  static constexpr int BTN_DOWN = 5;
  static constexpr int BTN_POWER = 6;
};

// Inline Settings enums
namespace papyrix {
struct Settings {
  enum SideButtonLayout : uint8_t { PrevNext = 0, NextPrev = 1 };
  enum FrontButtonLayout : uint8_t { FrontBCLR = 0, FrontLRBC = 1 };

  uint8_t sideButtonLayout = PrevNext;
  uint8_t frontButtonLayout = FrontBCLR;
};
}  // namespace papyrix

// Inline button mapping logic from MappedInputManager
enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };

struct Labels {
  const char* btn1;
  const char* btn2;
  const char* btn3;
  const char* btn4;
};

int mapButton(Button button, papyrix::Settings* settings) {
  const auto frontLayout = settings ? static_cast<papyrix::Settings::FrontButtonLayout>(settings->frontButtonLayout)
                                    : papyrix::Settings::FrontBCLR;
  const auto sideLayout = settings ? static_cast<papyrix::Settings::SideButtonLayout>(settings->sideButtonLayout)
                                   : papyrix::Settings::PrevNext;

  switch (button) {
    case Button::Back:
      switch (frontLayout) {
        case papyrix::Settings::FrontLRBC:
          return InputManager::BTN_LEFT;
        case papyrix::Settings::FrontBCLR:
        default:
          return InputManager::BTN_BACK;
      }
    case Button::Confirm:
      switch (frontLayout) {
        case papyrix::Settings::FrontLRBC:
          return InputManager::BTN_RIGHT;
        case papyrix::Settings::FrontBCLR:
        default:
          return InputManager::BTN_CONFIRM;
      }
    case Button::Left:
      switch (frontLayout) {
        case papyrix::Settings::FrontLRBC:
          return InputManager::BTN_BACK;
        case papyrix::Settings::FrontBCLR:
        default:
          return InputManager::BTN_LEFT;
      }
    case Button::Right:
      switch (frontLayout) {
        case papyrix::Settings::FrontLRBC:
          return InputManager::BTN_CONFIRM;
        case papyrix::Settings::FrontBCLR:
        default:
          return InputManager::BTN_RIGHT;
      }
    case Button::Up:
      return InputManager::BTN_UP;
    case Button::Down:
      return InputManager::BTN_DOWN;
    case Button::Power:
      return InputManager::BTN_POWER;
    case Button::PageBack:
      switch (sideLayout) {
        case papyrix::Settings::NextPrev:
          return InputManager::BTN_DOWN;
        case papyrix::Settings::PrevNext:
        default:
          return InputManager::BTN_UP;
      }
    case Button::PageForward:
      switch (sideLayout) {
        case papyrix::Settings::NextPrev:
          return InputManager::BTN_UP;
        case papyrix::Settings::PrevNext:
        default:
          return InputManager::BTN_DOWN;
      }
  }
  return InputManager::BTN_BACK;
}

Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next,
                 papyrix::Settings* settings) {
  const auto layout = settings ? static_cast<papyrix::Settings::FrontButtonLayout>(settings->frontButtonLayout)
                               : papyrix::Settings::FrontBCLR;

  switch (layout) {
    case papyrix::Settings::FrontLRBC:
      return {previous, next, back, confirm};
    case papyrix::Settings::FrontBCLR:
    default:
      return {back, confirm, previous, next};
  }
}

int main() {
  TestUtils::TestRunner runner("MappedInputManagerTest");

  // === Front button mapping: BCLR (default) ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontBCLR;

    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Back, &settings), "BCLR: Back -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Confirm, &settings), "BCLR: Confirm -> BTN_CONFIRM");
    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Left, &settings), "BCLR: Left -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_RIGHT, mapButton(Button::Right, &settings), "BCLR: Right -> BTN_RIGHT");
  }

  // === Front button mapping: LRBC (swapped) ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontLRBC;

    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Back, &settings), "LRBC: Back -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_RIGHT, mapButton(Button::Confirm, &settings), "LRBC: Confirm -> BTN_RIGHT");
    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Left, &settings), "LRBC: Left -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Right, &settings), "LRBC: Right -> BTN_CONFIRM");
  }

  // === Side button mapping: PrevNext (default) ===
  {
    papyrix::Settings settings;
    settings.sideButtonLayout = papyrix::Settings::PrevNext;

    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageBack, &settings), "PrevNext: PageBack -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageForward, &settings),
                    "PrevNext: PageForward -> BTN_DOWN");
  }

  // === Side button mapping: NextPrev (swapped) ===
  {
    papyrix::Settings settings;
    settings.sideButtonLayout = papyrix::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageBack, &settings), "NextPrev: PageBack -> BTN_DOWN");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageForward, &settings),
                    "NextPrev: PageForward -> BTN_UP");
  }

  // === Combined: LRBC front + NextPrev side ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontLRBC;
    settings.sideButtonLayout = papyrix::Settings::NextPrev;

    runner.expectEq(InputManager::BTN_LEFT, mapButton(Button::Back, &settings), "Combined: Back -> BTN_LEFT");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageBack, &settings),
                    "Combined: PageBack -> BTN_DOWN");
  }

  // === Non-remapped buttons are unaffected ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontLRBC;

    runner.expectEq(InputManager::BTN_UP, mapButton(Button::Up, &settings), "Up always -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::Down, &settings), "Down always -> BTN_DOWN");
    runner.expectEq(InputManager::BTN_POWER, mapButton(Button::Power, &settings), "Power always -> BTN_POWER");
  }

  // === Label mapping: BCLR ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontBCLR;

    auto labels = mapLabels("Back", "OK", "Prev", "Next", &settings);
    runner.expectTrue(strcmp(labels.btn1, "Back") == 0, "BCLR labels: btn1 = Back");
    runner.expectTrue(strcmp(labels.btn2, "OK") == 0, "BCLR labels: btn2 = OK");
    runner.expectTrue(strcmp(labels.btn3, "Prev") == 0, "BCLR labels: btn3 = Prev");
    runner.expectTrue(strcmp(labels.btn4, "Next") == 0, "BCLR labels: btn4 = Next");
  }

  // === Label mapping: LRBC ===
  {
    papyrix::Settings settings;
    settings.frontButtonLayout = papyrix::Settings::FrontLRBC;

    auto labels = mapLabels("Back", "OK", "Prev", "Next", &settings);
    runner.expectTrue(strcmp(labels.btn1, "Prev") == 0, "LRBC labels: btn1 = Prev");
    runner.expectTrue(strcmp(labels.btn2, "Next") == 0, "LRBC labels: btn2 = Next");
    runner.expectTrue(strcmp(labels.btn3, "Back") == 0, "LRBC labels: btn3 = Back");
    runner.expectTrue(strcmp(labels.btn4, "OK") == 0, "LRBC labels: btn4 = OK");
  }

  // === nullptr settings defaults to BCLR/PrevNext ===
  {
    runner.expectEq(InputManager::BTN_BACK, mapButton(Button::Back, nullptr), "nullptr: Back -> BTN_BACK");
    runner.expectEq(InputManager::BTN_CONFIRM, mapButton(Button::Confirm, nullptr),
                    "nullptr: Confirm -> BTN_CONFIRM");
    runner.expectEq(InputManager::BTN_UP, mapButton(Button::PageBack, nullptr), "nullptr: PageBack -> BTN_UP");
    runner.expectEq(InputManager::BTN_DOWN, mapButton(Button::PageForward, nullptr),
                    "nullptr: PageForward -> BTN_DOWN");

    auto labels = mapLabels("Back", "OK", "Prev", "Next", nullptr);
    runner.expectTrue(strcmp(labels.btn1, "Back") == 0, "nullptr labels: btn1 = Back");
    runner.expectTrue(strcmp(labels.btn2, "OK") == 0, "nullptr labels: btn2 = OK");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
