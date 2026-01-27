#include "HomeState.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <CoverHelpers.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <Markdown.h>
#include <SDCardManager.h>
#include <Txt.h>
#include <esp_system.h>

#include "../config.h"
#include "../content/ContentTypes.h"
#include "../core/BootMode.h"
#include "../core/Core.h"
#include "Battery.h"
#include "MappedInputManager.h"
#include "ThemeManager.h"

namespace papyrix {

HomeState::HomeState(GfxRenderer& renderer) : renderer_(renderer) {}

HomeState::~HomeState() {
  stopCoverGenTask();
  freeCoverBuffer();
}

void HomeState::enter(Core& core) {
  Serial.println("[HOME] Entering");

  // Load last book info if content is still open
  loadLastBook(core);

  // Update battery
  updateBattery();

  view_.needsRender = true;
}

void HomeState::exit(Core& core) {
  Serial.println("[HOME] Exiting");
  stopCoverGenTask();
  freeCoverBuffer();
  view_.clear();
}

void HomeState::loadLastBook(Core& core) {
  // Reset cover state
  coverBmpPath_.clear();
  hasCoverImage_ = false;
  coverLoadFailed_ = false;
  coverRendered_ = false;
  freeCoverBuffer();
  stopCoverGenTask();
  coverGenComplete_ = false;

  // If content already open, use it
  if (core.content.isOpen()) {
    const auto& meta = core.content.metadata();
    view_.setBook(meta.title, meta.author, core.buf.path);

    // Check if thumbnail already exists, otherwise start async generation
    if (core.settings.showImages) {
      coverBmpPath_ = core.content.getThumbnailPath();
      if (!coverBmpPath_.empty() && SdMan.exists(coverBmpPath_.c_str())) {
        hasCoverImage_ = true;
        Serial.printf("[%lu] [HOME] Using cached thumbnail: %s\n", millis(), coverBmpPath_.c_str());
      } else {
        // Start async generation
        Serial.printf("[%lu] [HOME] Thumbnail not found, starting async generation\n", millis());
        startCoverGenTask(core.buf.path, PAPYRIX_CACHE_DIR);
      }
    }
    view_.hasCoverBmp = hasCoverImage_;
    return;
  }

  // Try to load from saved path in settings
  const char* savedPath = core.settings.lastBookPath;
  if (savedPath[0] != '\0' && core.storage.exists(savedPath)) {
    // Open temporarily to get metadata
    auto result = core.content.open(savedPath, PAPYRIX_CACHE_DIR);
    if (result.ok()) {
      const auto& meta = core.content.metadata();
      view_.setBook(meta.title, meta.author, savedPath);
      // Set path in buf for "Continue Reading" button
      strncpy(core.buf.path, savedPath, sizeof(core.buf.path) - 1);
      core.buf.path[sizeof(core.buf.path) - 1] = '\0';

      // Check if thumbnail already exists, otherwise start async generation
      if (core.settings.showImages) {
        coverBmpPath_ = core.content.getThumbnailPath();
        if (!coverBmpPath_.empty() && SdMan.exists(coverBmpPath_.c_str())) {
          hasCoverImage_ = true;
          Serial.printf("[%lu] [HOME] Using cached thumbnail: %s\n", millis(), coverBmpPath_.c_str());
        } else {
          // Start async generation
          Serial.printf("[%lu] [HOME] Thumbnail not found, starting async generation\n", millis());
          startCoverGenTask(savedPath, PAPYRIX_CACHE_DIR);
        }
      }
      view_.hasCoverBmp = hasCoverImage_;

      // Close to free memory (will reopen when user selects Continue Reading)
      core.content.close();
    } else {
      view_.clearBook();
    }
  } else {
    view_.clearBook();
  }
}

void HomeState::updateBattery() {
  int percent = batteryMonitor.readPercentage();
  view_.setBattery(percent);
}

StateTransition HomeState::update(Core& core) {
  Event e;
  while (core.events.pop(e)) {
    switch (e.type) {
      case EventType::ButtonPress:
        switch (e.button) {
          case Button::Back:
            // btn1: Read - Continue reading if book is open
            if (view_.hasBook) {
              showTransitionNotification("Opening book...");
              saveTransition(BootMode::READER, core.buf.path, ReturnTo::HOME);
              vTaskDelay(50 / portTICK_PERIOD_MS);
              ESP.restart();
            }
            break;

          case Button::Center:
            // btn2: Files
            return StateTransition::to(StateId::FileList);

          case Button::Left:
            // btn3: Sync
            return StateTransition::to(StateId::Sync);

          case Button::Right:
            // btn4: Settings
            return StateTransition::to(StateId::Settings);

          case Button::Up:
          case Button::Down:
          case Button::Power:
            // Side buttons unused on home screen
            break;
        }
        break;

      case EventType::ButtonLongPress:
        if (e.button == Button::Power) {
          return StateTransition::to(StateId::Sleep);
        }
        break;

      default:
        break;
    }
  }

  return StateTransition::stay(StateId::Home);
}

void HomeState::render(Core& core) {
  // Check if async cover generation completed
  if (coverGenComplete_.exchange(false)) {
    // Copy path from task (safe now that flag was set)
    coverBmpPath_ = generatedCoverPath_;
    if (!coverBmpPath_.empty() && SdMan.exists(coverBmpPath_.c_str())) {
      hasCoverImage_ = true;
      view_.hasCoverBmp = true;
      view_.needsRender = true;
      Serial.println("[HOME] Async cover generation completed");
    }
  }

  if (!view_.needsRender) {
    return;
  }

  const Theme& theme = THEME;

  // If we have a stored cover buffer, restore it instead of re-reading from SD
  const bool bufferRestored = coverBufferStored_ && restoreCoverBuffer();

  // When cover is present, HomeState handles clear and card border
  // so cover can be drawn before text boxes
  if (hasCoverImage_ && !coverLoadFailed_) {
    const auto card = ui::CardDimensions::calculate(renderer_.getScreenWidth(), renderer_.getScreenHeight());

    if (!bufferRestored) {
      renderer_.clearScreen(theme.backgroundColor);

      // Draw card border
      renderer_.drawRect(card.x, card.y, card.width, card.height, theme.primaryTextBlack);

      // Render cover inside card (first time only)
      if (!coverRendered_) {
        renderCoverToCard();
        if (!coverLoadFailed_) {
          // Store buffer after first successful render
          coverBufferStored_ = storeCoverBuffer();
          coverRendered_ = true;
        }
      }
    }
  }

  // Render rest of UI (text boxes will draw on top of cover)
  ui::render(renderer_, theme, view_);

  renderer_.displayBuffer();
  view_.needsRender = false;
  core.display.markDirty();
}

void HomeState::renderCoverToCard() {
  FsFile file;
  if (!SdMan.openFileForRead("HOME", coverBmpPath_, file)) {
    coverLoadFailed_ = true;
    Serial.printf("[%lu] [HOME] Failed to open cover BMP: %s\n", millis(), coverBmpPath_.c_str());
    return;
  }

  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    file.close();
    coverLoadFailed_ = true;
    Serial.printf("[%lu] [HOME] Failed to parse cover BMP: %s\n", millis(), coverBmpPath_.c_str());
    return;
  }

  const auto card = ui::CardDimensions::calculate(renderer_.getScreenWidth(), renderer_.getScreenHeight());
  const auto coverArea = card.getCoverArea();
  auto rect = CoverHelpers::calculateCenteredRect(bitmap.getWidth(), bitmap.getHeight(), coverArea.x, coverArea.y,
                                                  coverArea.width, coverArea.height);

  renderer_.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
  file.close();
}

void HomeState::startCoverGenTask(const char* bookPath, const char* cacheDir) {
  stopCoverGenTask();

  pendingBookPath_ = bookPath ? bookPath : "";
  pendingCacheDir_ = cacheDir ? cacheDir : "";
  generatedCoverPath_.clear();
  coverGenComplete_ = false;

  xTaskCreate(&HomeState::coverGenTrampoline, "CoverGen", 4096, this, 0, &coverGenTaskHandle_);
  Serial.println("[HOME] Started async cover generation task");
}

void HomeState::stopCoverGenTask() {
  if (coverGenTaskHandle_) {
    vTaskDelete(coverGenTaskHandle_);
    coverGenTaskHandle_ = nullptr;
    Serial.println("[HOME] Stopped cover generation task");
  }
}

void HomeState::coverGenTrampoline(void* arg) {
  auto* self = static_cast<HomeState*>(arg);
  self->coverGenTask();
}

void HomeState::coverGenTask() {
  Serial.printf("[HOME] Cover gen task running for: %s\n", pendingBookPath_.c_str());

  // Detect content type from file extension
  ContentType type = detectContentType(pendingBookPath_.c_str());
  bool success = false;

  switch (type) {
    case ContentType::Epub: {
      Epub epub(pendingBookPath_, pendingCacheDir_);
      if (epub.load(false) && epub.generateThumbBmp()) {
        generatedCoverPath_ = epub.getThumbBmpPath();
        success = true;
      }
      break;
    }
    case ContentType::Txt: {
      Txt txt(pendingBookPath_, pendingCacheDir_);
      if (txt.load() && txt.generateThumbBmp()) {
        generatedCoverPath_ = txt.getThumbBmpPath();
        success = true;
      }
      break;
    }
    case ContentType::Markdown: {
      Markdown md(pendingBookPath_, pendingCacheDir_);
      if (md.load() && md.generateThumbBmp()) {
        generatedCoverPath_ = md.getThumbBmpPath();
        success = true;
      }
      break;
    }
    default:
      Serial.printf("[HOME] Unsupported content type for cover generation\n");
      break;
  }

  if (success) {
    coverGenComplete_ = true;
    Serial.println("[HOME] Cover generation task completed successfully");
  } else {
    Serial.println("[HOME] Cover generation task failed");
  }

  // Suspend self - will be deleted by stopCoverGenTask() or destructor
  vTaskSuspend(nullptr);
}

bool HomeState::storeCoverBuffer() {
  uint8_t* frameBuffer = renderer_.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  // Free any existing buffer first
  freeCoverBuffer();

  const size_t bufferSize = GfxRenderer::getBufferSize();
  coverBuffer_ = static_cast<uint8_t*>(malloc(bufferSize));
  if (!coverBuffer_) {
    Serial.println("[HOME] Failed to allocate cover buffer");
    return false;
  }

  memcpy(coverBuffer_, frameBuffer, bufferSize);
  Serial.printf("[HOME] Stored cover buffer (%u bytes)\n", bufferSize);
  return true;
}

bool HomeState::restoreCoverBuffer() {
  if (!coverBuffer_) {
    return false;
  }

  uint8_t* frameBuffer = renderer_.getFrameBuffer();
  if (!frameBuffer) {
    return false;
  }

  const size_t bufferSize = GfxRenderer::getBufferSize();
  memcpy(frameBuffer, coverBuffer_, bufferSize);
  return true;
}

void HomeState::freeCoverBuffer() {
  if (coverBuffer_) {
    free(coverBuffer_);
    coverBuffer_ = nullptr;
  }
  coverBufferStored_ = false;
}

}  // namespace papyrix
