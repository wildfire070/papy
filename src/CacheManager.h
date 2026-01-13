#pragma once

namespace CacheManager {

// Clears all book cache directories (epub_*, txt_*, xtc_*)
// Preserves system files (settings.bin, state.bin, wifi.bin)
// Returns number of directories deleted, or -1 on error
int clearAllBookCaches();

}  // namespace CacheManager
