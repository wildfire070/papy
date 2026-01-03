#pragma once
#include <string>
#include <vector>

struct OpdsServerConfig {
  std::string name;      // User-friendly display name (from INI section)
  std::string url;       // Base URL e.g., "http://192.168.1.100:8080/opds"
  std::string username;  // Basic Auth username (empty = no auth)
  std::string password;  // Basic Auth password
};

/**
 * Singleton class for reading OPDS server configurations from SD card.
 * Servers are stored in /opds.ini as user-editable INI file.
 */
class OpdsServerStore {
 private:
  static OpdsServerStore instance;
  std::vector<OpdsServerConfig> servers;

  static constexpr size_t MAX_SERVERS = 8;

  // Private constructor for singleton
  OpdsServerStore() = default;

  // Create default file with Project Gutenberg
  void createDefaultFile();

 public:
  // Delete copy constructor and assignment
  OpdsServerStore(const OpdsServerStore&) = delete;
  OpdsServerStore& operator=(const OpdsServerStore&) = delete;

  // Get singleton instance
  static OpdsServerStore& getInstance() { return instance; }

  // Load from SD card (creates default if missing)
  bool loadFromFile();

  // Accessors
  const OpdsServerConfig* getServer(size_t index) const;
  const std::vector<OpdsServerConfig>& getServers() const { return servers; }
  size_t getServerCount() const { return servers.size(); }
};

// Helper macro to access server store
#define OPDS_STORE OpdsServerStore::getInstance()
