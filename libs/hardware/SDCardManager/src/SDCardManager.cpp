#include "SDCardManager.h"

namespace {
constexpr uint8_t SD_CS = 12;
constexpr uint32_t SPI_FQ = 40000000;
}

SDCardManager SDCardManager::instance;

SDCardManager::SDCardManager() : sd() {}

bool SDCardManager::begin() {
  initialized = sd.begin(SD_CS, SPI_FQ);
  return initialized;
}

bool SDCardManager::ready() const {
  return initialized;
}

std::vector<std::string> SDCardManager::listFiles(const char* path, const int maxFiles) {
  std::vector<std::string> ret;
  if (!initialized) {
    return ret;
  }

  auto root = sd.open(path);
  if (!root || !root.isDirectory()) {
    return ret;
  }

  int count = 0;
  char name[128];
  for (auto f = root.openNextFile(); f && count < maxFiles; f = root.openNextFile()) {
    if (f.isDirectory()) {
      f.close();
      continue;
    }
    f.getName(name, sizeof(name));
    ret.emplace_back(name);
    f.close();
    count++;
  }
  root.close();
  return ret;
}

std::string SDCardManager::readFile(const char* path) {
  if (!initialized) {
    return {};
  }

  FsFile f;
  if (!openFileForRead("SD", path, f)) {
    return {};
  }

  std::string content;
  constexpr size_t maxSize = 50000;  // Limit to 50KB
  content.reserve(static_cast<size_t>(f.fileSize() < maxSize ? f.fileSize() : maxSize));
  size_t readSize = 0;
  while (f.available() && readSize < maxSize) {
    const char c = static_cast<char>(f.read());
    content += c;
    readSize++;
  }
  f.close();
  return content;
}

size_t SDCardManager::readFileToBuffer(const char* path, char* buffer, const size_t bufferSize, const size_t maxBytes) {
  if (!buffer || bufferSize == 0)
    return 0;
  if (!initialized) {
    buffer[0] = '\0';
    return 0;
  }

  FsFile f;
  if (!openFileForRead("SD", path, f)) {
    buffer[0] = '\0';
    return 0;
  }

  const size_t maxToRead = (maxBytes == 0) ? (bufferSize - 1) : min(maxBytes, bufferSize - 1);
  size_t total = 0;

  while (f.available() && total < maxToRead) {
    constexpr size_t chunk = 64;
    const size_t want = maxToRead - total;
    const size_t readLen = (want < chunk) ? want : chunk;
    const int r = f.read(buffer + total, readLen);
    if (r > 0) {
      total += static_cast<size_t>(r);
    } else {
      break;
    }
  }

  buffer[total] = '\0';
  f.close();
  return total;
}

bool SDCardManager::writeFile(const char* path, const std::string& content) {
  if (!initialized) {
    return false;
  }

  // Remove existing file so we perform an overwrite rather than append
  if (sd.exists(path)) {
    sd.remove(path);
  }

  FsFile f;
  if (!openFileForWrite("SD", path, f)) {
    return false;
  }

  const size_t written = f.write(content.data(), content.size());
  f.close();
  return written == content.size();
}

bool SDCardManager::ensureDirectoryExists(const char* path) {
  if (!initialized) {
    return false;
  }

  // Check if directory already exists
  if (sd.exists(path)) {
    FsFile dir = sd.open(path);
    if (dir && dir.isDirectory()) {
      dir.close();
      return true;
    }
    dir.close();
  }

  return sd.mkdir(path);
}

bool SDCardManager::openFileForRead(const char* moduleName, const char* path, FsFile& file) {
  if (!sd.exists(path)) {
    return false;
  }

  file = sd.open(path, O_RDONLY);
  return static_cast<bool>(file);
}

bool SDCardManager::openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const char* path, FsFile& file) {
  file = sd.open(path, O_RDWR | O_CREAT | O_TRUNC);
  return static_cast<bool>(file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool SDCardManager::removeDir(const char* path) {
  // 1. Open the directory
  auto dir = sd.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  auto file = dir.openNextFile();
  char name[128];
  while (file) {
    std::string filePath = path;
    if (filePath.back() != '/') {
      filePath += '/';
    }
    file.getName(name, sizeof(name));
    filePath += name;

    if (file.isDirectory()) {
      if (!removeDir(filePath.c_str())) {
        return false;
      }
    } else {
      if (!sd.remove(filePath.c_str())) {
        return false;
      }
    }
    file = dir.openNextFile();
  }

  return sd.rmdir(path);
}
