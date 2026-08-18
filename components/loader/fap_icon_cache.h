#pragma once
/*
 * Cache of a .fap's display name + 10x10 icon, keyed by path and file size, so
 * the app browser doesn't re-open and re-parse every FAP each time it's shown
 * (the per-FAP manifest read + icon decode is what made the app list take
 * seconds with many FAPs installed). Persisted to /ext/.fap_icon_cache.
 */
#include <furi.h>
#include <storage/storage.h>

/* Load the cache file into memory. Call once before a scan. */
void fap_icon_cache_open(Storage* storage);

/* Cache hit: if `path` is cached and its on-disk size is unchanged, fill `icon`
 * (FAP_MANIFEST_MAX_ICON_SIZE bytes) and `name`, and return true. Else false. */
bool fap_icon_cache_get(Storage* storage, FuriString* path, uint8_t* icon, FuriString* name);

/* Record a freshly-parsed entry after a miss. */
void fap_icon_cache_put(FuriString* path, uint64_t size, const uint8_t* icon, const char* name);

/* Flush to disk if changed, and free. Call once after a scan. */
void fap_icon_cache_close(Storage* storage);
