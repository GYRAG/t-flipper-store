#include "wlan_app_store.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>

#define TAG "WlanAppStore"

#define STORE_BASE_URL    "https://gyrag.github.io/t-flipper-store"
#define STORE_CATALOG_URL STORE_BASE_URL "/catalog.txt"
#define STORE_APPS_ROOT   "/ext/apps"

#define STORE_MAX_APPS    64u
#define STORE_CATALOG_MAX (16u * 1024u) /* ~64 apps x ~120 chars, with slack */
#define STORE_CHUNK       8192
#define STORE_MAX_RETRY   4

typedef struct {
    const char* id;
    const char* name;
    const char* category;
    const char* path; /* "faps/<id>.fap", relative to STORE_BASE_URL */
    uint32_t size;
} StoreEntry;

struct WlanAppStore {
    TaskHandle_t task;
    volatile bool cancel;
    volatile bool running;
    volatile WlanAppStorePhase phase;
    volatile uint8_t percent;
    volatile uint32_t speed_kbps;
    char error[96];
    char dest[128]; /* where the last install landed */

    char* catalog;  /* raw catalog.txt; entries point into it */
    StoreEntry entries[STORE_MAX_APPS];
    uint32_t count;

    volatile int32_t pending_index; /* >=0 => install that entry, else fetch */
};

static void* store_malloc(size_t n) {
    /* Catalog buffer is chunky and not latency-critical — prefer PSRAM, but
     * fall back to internal RAM rather than failing outright. */
    void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    return p ? p : malloc(n);
}

static void store_set_error(WlanAppStore* s, const char* msg) {
    strncpy(s->error, msg, sizeof(s->error) - 1);
    s->error[sizeof(s->error) - 1] = '\0';
    s->phase = WlanAppStoreError;
}

static void store_http_cfg(esp_http_client_config_t* cfg, const char* url) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->url = url;
    cfg->timeout_ms = 40000;
    cfg->transport_type = HTTP_TRANSPORT_OVER_SSL;
    /* Unlike wlan_sd_update, this VERIFIES the server certificate. That file
     * downloads SD assets; this one downloads executable code the device then
     * runs, so an unauthenticated transport would let anyone on the path serve
     * arbitrary FAPs. CONFIG_MBEDTLS_CERTIFICATE_BUNDLE is enabled (full), so
     * github.io validates against the bundled roots with no extra setup. */
    cfg->crt_bundle_attach = esp_crt_bundle_attach;
    cfg->buffer_size = STORE_CHUNK;
    cfg->buffer_size_tx = 1024;
    /* Reuse the TLS session across catalog + file: a fresh handshake per
     * request is by far the slowest part of a small download. */
    cfg->keep_alive_enable = true;
}

/* ---- catalog ---------------------------------------------------------- */

/* One line: id|name|category|size|fw|path
 * Parsed in place — fields stay pointers into s->catalog, like the files.txt
 * parser in wlan_sd_update.c. The `fw` column is read past deliberately: this
 * firmware runs both stock and modified apps, so it is informational only. */
static bool store_parse_line(char* line, StoreEntry* e) {
    if(*line == '#' || *line == '\0') return false;

    char* save = NULL;
    char* id = strtok_r(line, "|", &save);
    char* name = strtok_r(NULL, "|", &save);
    char* category = strtok_r(NULL, "|", &save);
    char* size = strtok_r(NULL, "|", &save);
    char* fw = strtok_r(NULL, "|", &save);
    char* path = strtok_r(NULL, "|", &save);
    UNUSED(fw);

    if(!id || !name || !category || !size || !fw || !path) return false;
    if(!*id || !*name || !*category || !*path) return false;

    e->id = id;
    e->name = name;
    e->category = category;
    e->path = path;
    e->size = (uint32_t)strtoul(size, NULL, 10);
    return true;
}

static bool store_fetch_catalog(WlanAppStore* s) {
    s->count = 0;

    esp_http_client_config_t cfg;
    store_http_cfg(&cfg, STORE_CATALOG_URL);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if(!client) {
        store_set_error(s, "Out of memory");
        return false;
    }

    bool ok = false;
    size_t len = 0;
    if(esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        if(status == 200) {
            while(len + 1 < STORE_CATALOG_MAX && !s->cancel) {
                int r = esp_http_client_read(client, s->catalog + len, STORE_CATALOG_MAX - 1 - len);
                if(r < 0) {
                    len = 0;
                    break;
                }
                if(r == 0) break;
                len += (size_t)r;
            }
            s->catalog[len] = '\0';
            ok = len > 0;
            if(!ok) store_set_error(s, "Empty catalog");
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "Catalog HTTP %d", status);
            store_set_error(s, msg);
        }
    } else {
        store_set_error(s, "Cannot reach store");
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if(!ok) return false;

    char* save = NULL;
    for(char* line = strtok_r(s->catalog, "\n", &save);
        line && s->count < STORE_MAX_APPS;
        line = strtok_r(NULL, "\n", &save)) {
        /* Tolerate CRLF even though we publish LF, so a re-hosted catalog
         * edited on Windows does not silently break every last field. */
        size_t n = strlen(line);
        while(n && (line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = '\0';
        if(store_parse_line(line, &s->entries[s->count])) s->count++;
    }

    if(s->count == 0) {
        store_set_error(s, "No apps in catalog");
        return false;
    }
    FURI_LOG_I(TAG, "catalog: %lu apps", (unsigned long)s->count);
    return true;
}

/* ---- install ---------------------------------------------------------- */

/* Download to <dest>.part and rename on success, so an interrupted transfer
 * can never leave a truncated .fap that the loader would try to execute. */
static bool store_download(WlanAppStore* s, Storage* storage, const StoreEntry* e) {
    char url[256];
    char dir[128];
    char dest[160];
    char part[176];
    snprintf(url, sizeof(url), "%s/%s", STORE_BASE_URL, e->path);
    snprintf(dir, sizeof(dir), "%s/%s", STORE_APPS_ROOT, e->category);
    snprintf(dest, sizeof(dest), "%s/%s.fap", dir, e->id);
    snprintf(part, sizeof(part), "%s.part", dest);

    /* Category dir may not exist yet — same lesson the USB store learned. */
    storage_common_mkdir(storage, STORE_APPS_ROOT);
    storage_common_mkdir(storage, dir);
    storage_common_remove(storage, part);

    esp_http_client_config_t cfg;
    store_http_cfg(&cfg, url);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if(!client) {
        store_set_error(s, "Out of memory");
        return false;
    }

    bool ok = false;
    uint8_t* chunk = malloc(STORE_CHUNK);
    File* f = chunk ? storage_file_alloc(storage) : NULL;

    if(f && storage_file_open(f, part, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(esp_http_client_open(client, 0) == ESP_OK) {
            esp_http_client_fetch_headers(client);
            int status = esp_http_client_get_status_code(client);
            if(status == 200) {
                ok = true;
                uint32_t got = 0;
                uint32_t t0 = furi_get_tick();
                while(!s->cancel) {
                    int r = esp_http_client_read(client, (char*)chunk, STORE_CHUNK);
                    if(r < 0) {
                        ok = false;
                        break;
                    }
                    if(r == 0) break;
                    if(storage_file_write(f, chunk, (size_t)r) != (size_t)r) {
                        store_set_error(s, "SD write failed");
                        ok = false;
                        break;
                    }
                    got += (uint32_t)r;
                    if(e->size) {
                        uint32_t pct = (uint32_t)((uint64_t)got * 100u / e->size);
                        s->percent = (uint8_t)(pct > 100 ? 100 : pct);
                    }
                    uint32_t dt = furi_get_tick() - t0;
                    if(dt >= 200) s->speed_kbps = (uint32_t)((uint64_t)got * 1000u / 1024u / dt);
                }
                if(s->cancel) ok = false;
                /* A short read that ends cleanly is still a truncated file. */
                if(ok && e->size && got != e->size) {
                    store_set_error(s, "Truncated download");
                    ok = false;
                }
            } else {
                char msg[64];
                snprintf(msg, sizeof(msg), "Download HTTP %d", status);
                store_set_error(s, msg);
            }
        } else {
            store_set_error(s, "Cannot reach store");
        }
    } else if(!chunk) {
        store_set_error(s, "Out of memory");
    } else {
        store_set_error(s, "Cannot write to SD");
    }

    if(f) {
        storage_file_close(f);
        storage_file_free(f);
    }
    if(chunk) free(chunk);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if(!ok) {
        storage_common_remove(storage, part); /* never leave a stray .part */
        return false;
    }

    storage_common_remove(storage, dest); /* rename won't overwrite */
    if(storage_common_rename(storage, part, dest) != FSE_OK) {
        storage_common_remove(storage, part);
        store_set_error(s, "Cannot finalise file");
        return false;
    }

    snprintf(s->dest, sizeof(s->dest), "Apps > %s > %s", e->category, e->name);
    FURI_LOG_I(TAG, "installed %s", dest);
    return true;
}

/* ---- worker ----------------------------------------------------------- */

static void store_task(void* ctx) {
    WlanAppStore* s = ctx;
    Storage* storage = furi_record_open(RECORD_STORAGE);

    int32_t idx = s->pending_index;
    if(idx < 0) {
        s->phase = WlanAppStoreFetching;
        if(store_fetch_catalog(s)) s->phase = WlanAppStoreReady;
    } else if((uint32_t)idx < s->count) {
        s->phase = WlanAppStoreInstalling;
        s->percent = 0;
        if(store_download(s, storage, &s->entries[idx])) s->phase = WlanAppStoreDone;
    } else {
        store_set_error(s, "No such app");
    }

    if(s->cancel && s->phase != WlanAppStoreError) store_set_error(s, "Cancelled");

    furi_record_close(RECORD_STORAGE);
    s->speed_kbps = 0;
    s->running = false;
    s->task = NULL;
    vTaskDelete(NULL);
}

static void store_start(WlanAppStore* s, int32_t index) {
    furi_check(s);
    if(s->running) return;
    s->cancel = false;
    s->error[0] = '\0';
    s->percent = 0;
    s->speed_kbps = 0;
    s->pending_index = index;
    s->running = true;
    /* xTaskCreate, not furi_thread_alloc: esp_http_client drives lwIP sockets,
     * which must not be driven from a FuriThread (see wlan_sd_update.c). */
    if(xTaskCreate(store_task, "WlanStore", 8192, s, 4, &s->task) != pdPASS) {
        s->running = false;
        store_set_error(s, "Cannot start worker");
    }
}

WlanAppStore* wlan_app_store_alloc(void) {
    WlanAppStore* s = malloc(sizeof(WlanAppStore));
    memset(s, 0, sizeof(*s));
    s->catalog = store_malloc(STORE_CATALOG_MAX);
    s->pending_index = -1;
    return s;
}

void wlan_app_store_free(WlanAppStore* s) {
    furi_check(s);
    wlan_app_store_cancel(s);
    if(s->catalog) free(s->catalog);
    free(s);
}

void wlan_app_store_fetch(WlanAppStore* s) {
    store_start(s, -1);
}

void wlan_app_store_install(WlanAppStore* s, uint32_t index) {
    store_start(s, (int32_t)index);
}

void wlan_app_store_cancel(WlanAppStore* s) {
    furi_check(s);
    if(!s->running) return;
    s->cancel = true;
    for(uint32_t i = 0; i < 100 && s->running; i++) {
        furi_delay_ms(50);
    }
}

WlanAppStorePhase wlan_app_store_get_phase(const WlanAppStore* s) {
    return s->phase;
}
uint8_t wlan_app_store_get_percent(const WlanAppStore* s) {
    return s->percent;
}
uint32_t wlan_app_store_get_speed_kbps(const WlanAppStore* s) {
    return s->speed_kbps;
}
const char* wlan_app_store_get_error(const WlanAppStore* s) {
    return s->error;
}
bool wlan_app_store_is_running(const WlanAppStore* s) {
    return s->running;
}
uint32_t wlan_app_store_count(const WlanAppStore* s) {
    return s->count;
}
const char* wlan_app_store_name(const WlanAppStore* s, uint32_t i) {
    return (i < s->count) ? s->entries[i].name : "";
}
const char* wlan_app_store_category(const WlanAppStore* s, uint32_t i) {
    return (i < s->count) ? s->entries[i].category : "";
}
uint32_t wlan_app_store_size(const WlanAppStore* s, uint32_t i) {
    return (i < s->count) ? s->entries[i].size : 0;
}
const char* wlan_app_store_get_dest(const WlanAppStore* s) {
    return s->dest;
}
