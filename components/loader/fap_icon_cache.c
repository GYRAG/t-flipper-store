#include "fap_icon_cache.h"
#include <flipper_application/application_manifest.h> /* FAP_MANIFEST_MAX_ICON_SIZE */
#include <string.h>

#define TAG        "FapIconCache"
#define CACHE_PATH "/ext/.fap_icon_cache" /* dotfile at /ext root, outside the /ext/apps scan */
#define CACHE_MAGIC 0x46414332u /* 'FAC2' — bump if the entry layout changes */
#define MAX_ENTRIES 128
#define MAX_PATH_LEN 128
#define MAX_NAME_LEN 32

typedef struct {
    char path[MAX_PATH_LEN];
    uint64_t size;
    char name[MAX_NAME_LEN];
    uint8_t icon[FAP_MANIFEST_MAX_ICON_SIZE];
} FapCacheEntry;

static struct {
    FapCacheEntry* entries;
    uint16_t count;
    bool dirty;
    bool open;
} s_cache;

void fap_icon_cache_open(Storage* storage) {
    if(s_cache.open) return;
    s_cache.entries = malloc(sizeof(FapCacheEntry) * MAX_ENTRIES);
    s_cache.count = 0;
    s_cache.dirty = false;
    s_cache.open = true;

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CACHE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0, count = 0;
        if(storage_file_read(f, &magic, sizeof(magic)) == sizeof(magic) && magic == CACHE_MAGIC &&
           storage_file_read(f, &count, sizeof(count)) == sizeof(count) && count <= MAX_ENTRIES) {
            size_t want = sizeof(FapCacheEntry) * count;
            if(count == 0 || storage_file_read(f, s_cache.entries, want) == want) {
                s_cache.count = (uint16_t)count;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

static FapCacheEntry* fap_icon_cache_find(const char* path) {
    for(uint16_t i = 0; i < s_cache.count; i++) {
        if(strcmp(s_cache.entries[i].path, path) == 0) return &s_cache.entries[i];
    }
    return NULL;
}

bool fap_icon_cache_get(Storage* storage, FuriString* path, uint8_t* icon, FuriString* name) {
    if(!s_cache.open) return false;
    const char* p = furi_string_get_cstr(path);
    FapCacheEntry* e = fap_icon_cache_find(p);
    if(!e) return false;

    /* Validate against current file size — a rebuilt/replaced FAP invalidates.
     * ponytail: size-only key (FileInfo has no mtime); a same-size replacement
     * would serve a stale icon — vanishingly rare, and self-heals on next change. */
    FileInfo fi;
    if(storage_common_stat(storage, p, &fi) != FSE_OK || fi.size != e->size) return false;

    memcpy(icon, e->icon, FAP_MANIFEST_MAX_ICON_SIZE);
    furi_string_set_str(name, e->name);
    return true;
}

void fap_icon_cache_put(FuriString* path, uint64_t size, const uint8_t* icon, const char* name) {
    if(!s_cache.open) return;
    const char* p = furi_string_get_cstr(path);
    if(strlen(p) >= MAX_PATH_LEN) return; /* don't cache absurd paths */

    FapCacheEntry* e = fap_icon_cache_find(p);
    if(!e) {
        if(s_cache.count >= MAX_ENTRIES) return; /* full — stay uncached, still correct */
        e = &s_cache.entries[s_cache.count++];
        strncpy(e->path, p, MAX_PATH_LEN - 1);
        e->path[MAX_PATH_LEN - 1] = '\0';
    }
    e->size = size;
    strncpy(e->name, name ? name : "", MAX_NAME_LEN - 1);
    e->name[MAX_NAME_LEN - 1] = '\0';
    memcpy(e->icon, icon, FAP_MANIFEST_MAX_ICON_SIZE);
    s_cache.dirty = true;
}

void fap_icon_cache_close(Storage* storage) {
    if(!s_cache.open) return;
    if(s_cache.dirty) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, CACHE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            uint32_t magic = CACHE_MAGIC, count = s_cache.count;
            storage_file_write(f, &magic, sizeof(magic));
            storage_file_write(f, &count, sizeof(count));
            storage_file_write(f, s_cache.entries, sizeof(FapCacheEntry) * s_cache.count);
        }
        storage_file_close(f);
        storage_file_free(f);
    }
    free(s_cache.entries);
    memset(&s_cache, 0, sizeof(s_cache));
}
