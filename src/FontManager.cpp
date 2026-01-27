#include "FontManager.h"

#include <EpdFontLoader.h>
#include <SDCardManager.h>

#include <cstring>

#include "config.h"

FontManager& FontManager::instance() {
  static FontManager instance;
  return instance;
}

FontManager::FontManager() = default;

FontManager::~FontManager() {
  unloadAllFonts();
  unloadExternalFont();
}

void FontManager::init(GfxRenderer& r) { renderer = &r; }

bool FontManager::loadFontFamily(const char* familyName, int fontId) {
  if (!renderer || !familyName || !*familyName) {
    return false;
  }

  // Build base path
  char basePath[64];
  snprintf(basePath, sizeof(basePath), "%s/%s", CONFIG_FONTS_DIR, familyName);

  // Check if directory exists
  if (!SdMan.exists(basePath)) {
    Serial.printf("[FONT] Font family not found: %s\n", basePath);
    return false;
  }

  LoadedFamily family;
  family.fontId = fontId;

  // Only load regular font to save memory (~150KB savings)
  // Bold/italic/bold_italic will use the same regular font
  char fontPath[80];
  snprintf(fontPath, sizeof(fontPath), "%s/regular.epdfont", basePath);

  LoadedFont loaded = loadSingleFont(fontPath);
  if (!loaded.font) {
    Serial.printf("[FONT] Failed to load regular font for %s\n", familyName);
    return false;
  }

  family.fonts.push_back(loaded);
  Serial.printf("[FONT] Loaded %s/regular (bold/italic use same)\n", familyName);

  // Create font family with regular font for all styles
  EpdFontFamily fontFamily(loaded.font, loaded.font, loaded.font, loaded.font);
  renderer->insertFont(fontId, fontFamily);

  // Store for cleanup
  loadedFamilies[fontId] = std::move(family);

  Serial.printf("[FONT] Registered font family %s with ID %d\n", familyName, fontId);
  return true;
}

FontManager::LoadedFont FontManager::loadSingleFont(const char* path) {
  LoadedFont result = {nullptr, nullptr, nullptr, nullptr, nullptr};

  if (!SdMan.exists(path)) {
    return result;
  }

  EpdFontLoader::LoadResult loaded = EpdFontLoader::loadFromFile(path);
  if (!loaded.success) {
    Serial.printf("[FONT] Failed to load: %s\n", path);
    return result;
  }

  result.data = loaded.fontData;
  result.bitmap = loaded.bitmap;
  result.glyphs = loaded.glyphs;
  result.intervals = loaded.intervals;
  result.font = new EpdFont(result.data);

  return result;
}

void FontManager::freeFont(LoadedFont& font) {
  delete font.font;
  delete font.data;
  delete[] font.bitmap;
  delete[] font.glyphs;
  delete[] font.intervals;
  font = {nullptr, nullptr, nullptr, nullptr, nullptr};
}

void FontManager::unloadFontFamily(int fontId) {
  auto it = loadedFamilies.find(fontId);
  if (it != loadedFamilies.end()) {
    if (renderer) {
      renderer->removeFont(fontId);
    }
    for (auto& f : it->second.fonts) {
      freeFont(f);
    }
    loadedFamilies.erase(it);
    Serial.printf("[FONT] Unloaded font family ID %d\n", fontId);
  }
}

void FontManager::unloadAllFonts() {
  for (auto& pair : loadedFamilies) {
    if (renderer) {
      renderer->removeFont(pair.second.fontId);
    }
    for (auto& f : pair.second.fonts) {
      freeFont(f);
    }
  }
  loadedFamilies.clear();
  Serial.println("[FONT] Unloaded all fonts");
}

std::vector<std::string> FontManager::listAvailableFonts() {
  std::vector<std::string> fonts;

  FsFile dir = SdMan.open(CONFIG_FONTS_DIR);
  if (!dir || !dir.isDirectory()) {
    return fonts;
  }

  FsFile entry;
  while (entry.openNext(&dir, O_RDONLY)) {
    if (entry.isDirectory()) {
      char name[64];
      entry.getName(name, sizeof(name));
      // Skip hidden directories
      if (name[0] != '.') {
        // Check if it has at least regular.epdfont
        char regularPath[80];
        snprintf(regularPath, sizeof(regularPath), "%s/%s/regular.epdfont", CONFIG_FONTS_DIR, name);
        if (SdMan.exists(regularPath)) {
          fonts.push_back(name);
        }
      }
    }
    entry.close();
  }
  dir.close();

  return fonts;
}

bool FontManager::fontFamilyExists(const char* familyName) {
  if (!familyName || !*familyName) return false;

  char path[80];
  snprintf(path, sizeof(path), "%s/%s/regular.epdfont", CONFIG_FONTS_DIR, familyName);
  return SdMan.exists(path);
}

int FontManager::getFontId(const char* familyName, int builtinFontId) {
  if (!familyName || !*familyName) {
    return builtinFontId;
  }

  int targetId = generateFontId(familyName);
  if (loadedFamilies.find(targetId) != loadedFamilies.end()) {
    return targetId;
  }

  // Load from SD card
  if (loadFontFamily(familyName, targetId)) {
    return targetId;
  }

  return builtinFontId;
}

int FontManager::generateFontId(const char* familyName) {
  // Simple hash for consistent font IDs
  uint32_t hash = 5381;
  while (*familyName) {
    hash = ((hash << 5) + hash) + static_cast<uint8_t>(*familyName);
    familyName++;
  }
  return static_cast<int>(hash);
}

bool FontManager::isBinFont(const char* familyName) {
  if (!familyName) return false;
  size_t len = strlen(familyName);
  return len > 4 && strcmp(familyName + len - 4, ".bin") == 0;
}

int FontManager::getReaderFontId(const char* familyName, int builtinFontId) {
  if (!familyName || !*familyName) {
    // Using built-in font - unload any custom reader font and external font
    if (_activeReaderFontId != 0 && _activeReaderFontId != builtinFontId) {
      Serial.printf("[FONT] Unloading custom reader font ID %d (switching to built-in)\n", _activeReaderFontId);
      unloadFontFamily(_activeReaderFontId);
      _activeReaderFontId = 0;
    }
    unloadExternalFont();
    return builtinFontId;
  }

  // Handle .bin fonts as external fonts (CJK fallback)
  if (isBinFont(familyName)) {
    // Unload any previous custom .epdfont reader font
    if (_activeReaderFontId != 0 && _activeReaderFontId != builtinFontId) {
      Serial.printf("[FONT] Unloading custom reader font ID %d (switching to .bin)\n", _activeReaderFontId);
      unloadFontFamily(_activeReaderFontId);
      _activeReaderFontId = 0;
    }

    // Load as external font - provides fallback for CJK characters
    if (loadExternalFont(familyName)) {
      Serial.printf("[FONT] Using built-in font + %s for CJK fallback\n", familyName);
    }
    // Return builtin font ID - ASCII uses built-in, CJK falls back to external
    return builtinFontId;
  }

  int targetId = generateFontId(familyName);

  // If switching to a different custom font, unload previous
  if (_activeReaderFontId != 0 && _activeReaderFontId != targetId) {
    Serial.printf("[FONT] Unloading previous reader font ID %d\n", _activeReaderFontId);
    unloadFontFamily(_activeReaderFontId);
  }

  // Load new font if needed
  if (loadedFamilies.find(targetId) == loadedFamilies.end()) {
    if (!loadFontFamily(familyName, targetId)) {
      _activeReaderFontId = 0;
      return builtinFontId;
    }
  }

  _activeReaderFontId = targetId;
  return targetId;
}

bool FontManager::loadExternalFont(const char* filename) {
  if (!renderer || !filename || !*filename) {
    return false;
  }

  char path[80];
  snprintf(path, sizeof(path), "%s/%s", CONFIG_FONTS_DIR, filename);

  // Allocate if needed
  if (!_externalFont) {
    _externalFont = new ExternalFont();
  }

  if (!_externalFont->load(path)) {
    Serial.printf("[FONT] Failed to load external font: %s\n", path);
    delete _externalFont;
    _externalFont = nullptr;
    return false;
  }

  renderer->setExternalFont(_externalFont);
  Serial.printf("[FONT] Loaded external font: %s (CJK fallback)\n", filename);
  return true;
}

void FontManager::unloadExternalFont() {
  if (_externalFont) {
    if (renderer) {
      renderer->setExternalFont(nullptr);
    }
    delete _externalFont;
    _externalFont = nullptr;
    Serial.println("[FONT] Unloaded external font");
  }
}

void FontManager::logFontInfo() const {
  Serial.println("[FONT] === Current Font Configuration ===");

  // Log built-in fonts info
  Serial.println("[FONT] Built-in reader fonts: small/medium/large (FLASH)");
  Serial.println("[FONT] Built-in UI font: ui_12 (FLASH)");

  // Log loaded custom fonts
  for (const auto& entry : loadedFamilies) {
    Serial.printf("[FONT] Custom: ID %d from SD (loaded)\n", entry.first);
  }

  // Log external CJK font
  if (_externalFont && _externalFont->isLoaded()) {
    Serial.printf("[FONT] External CJK: %s_%d_%dx%d.bin from SD\n", _externalFont->getFontName(),
                  _externalFont->getFontSize(), _externalFont->getCharWidth(), _externalFont->getCharHeight());
    _externalFont->logCacheStats();
  }

  Serial.println("[FONT] =====================================");
}
