/**
 * Markdown.cpp
 *
 * Markdown file handler implementation for Papyrix Reader
 */

#include "Markdown.h"

#include <CoverHelpers.h>
#include <FsHelpers.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>

Markdown::Markdown(std::string filepath, const std::string& cacheDir)
    : filepath(std::move(filepath)), fileSize(0), loaded(false) {
  // Create cache key based on filepath (same as Epub/Xtc/Txt)
  cachePath = cacheDir + "/md_" + std::to_string(std::hash<std::string>{}(this->filepath));

  // Extract title from filename
  size_t lastSlash = this->filepath.find_last_of('/');
  size_t lastDot = this->filepath.find_last_of('.');

  if (lastSlash == std::string::npos) {
    lastSlash = 0;
  } else {
    lastSlash++;
  }

  if (lastDot == std::string::npos || lastDot <= lastSlash) {
    title = this->filepath.substr(lastSlash);
  } else {
    title = this->filepath.substr(lastSlash, lastDot - lastSlash);
  }
}

bool Markdown::load() {
  Serial.printf("[%lu] [MD ] Loading Markdown: %s\n", millis(), filepath.c_str());

  if (!SdMan.exists(filepath.c_str())) {
    Serial.printf("[%lu] [MD ] File does not exist\n", millis());
    return false;
  }

  FsFile file;
  if (!SdMan.openFileForRead("MD ", filepath, file)) {
    Serial.printf("[%lu] [MD ] Failed to open file\n", millis());
    return false;
  }

  fileSize = file.size();
  file.close();

  loaded = true;
  Serial.printf("[%lu] [MD ] Loaded Markdown: %s (%zu bytes)\n", millis(), filepath.c_str(), fileSize);
  return true;
}

bool Markdown::clearCache() const {
  if (!SdMan.exists(cachePath.c_str())) {
    Serial.printf("[%lu] [MD ] Cache does not exist, no action needed\n", millis());
    return true;
  }

  if (!SdMan.removeDir(cachePath.c_str())) {
    Serial.printf("[%lu] [MD ] Failed to clear cache\n", millis());
    return false;
  }

  Serial.printf("[%lu] [MD ] Cache cleared successfully\n", millis());
  return true;
}

void Markdown::setupCacheDir() const {
  if (SdMan.exists(cachePath.c_str())) {
    return;
  }

  // Create directories recursively
  for (size_t i = 1; i < cachePath.length(); i++) {
    if (cachePath[i] == '/') {
      SdMan.mkdir(cachePath.substr(0, i).c_str());
    }
  }
  SdMan.mkdir(cachePath.c_str());
}

std::string Markdown::getCoverBmpPath() const { return cachePath + "/cover.bmp"; }

std::string Markdown::findCoverImage() const {
  // Extract directory path
  size_t lastSlash = filepath.find_last_of('/');
  std::string dirPath = (lastSlash == std::string::npos) ? "/" : filepath.substr(0, lastSlash);
  if (dirPath.empty()) dirPath = "/";

  return CoverHelpers::findCoverImage(dirPath, title);
}

bool Markdown::generateCoverBmp() const {
  // Already generated
  if (SdMan.exists(getCoverBmpPath().c_str())) {
    return true;
  }

  // Find a cover image
  std::string coverImagePath = findCoverImage();
  if (coverImagePath.empty()) {
    Serial.printf("[%lu] [MD ] No cover image found\n", millis());
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Convert to BMP using shared helper
  return CoverHelpers::convertImageToBmp(coverImagePath, getCoverBmpPath(), "MD ");
}

size_t Markdown::readContent(uint8_t* buffer, size_t offset, size_t length) const {
  if (!loaded) {
    return 0;
  }

  FsFile file;
  if (!SdMan.openFileForRead("MD ", filepath, file)) {
    return 0;
  }

  if (offset > 0) {
    file.seek(offset);
  }

  size_t bytesRead = file.read(buffer, length);
  file.close();

  return bytesRead;
}
