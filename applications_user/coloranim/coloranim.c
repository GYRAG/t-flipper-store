/*
 * Color Animations — play full-color 320x170 RGB565 animations on the T-Embed
 * LCD, the same way Doom draws: take over the display with gui_direct_draw and
 * blit RGB565 stripes with esp_lcd_panel_draw_bitmap. No 128x64 mono downscale.
 *
 * Packs live under /ext/apps_data/coloranim/<name>/ :
 *   info.txt    -> Width / Height / Frames / Frame rate
 *   frames.bin  -> Frames * (Width*Height*2) bytes, big-endian RGB565 (ST7789)
 * Build packs with tools/compile_animation.py --color.
 */
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_display.h>
#include <furi_hal_spi_bus.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <input/input.h>
#include <esp_lcd_panel_ops.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

#define TAG      "ColorAnim"
#define ANIM_DIR "/ext/apps_data/coloranim"
#define STRIPE_H 16
#define MAX_ANIMS 32

typedef struct {
    char name[64];
    uint16_t width, height, frames, frame_rate;
} AnimInfo;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Storage* storage;
    AnimInfo anims[MAX_ANIMS];
    size_t anim_count;
    int32_t selected; /* -2 pending, -1 back/exit, >=0 chosen index */
    volatile bool stop_playback;
} App;

/* --- info.txt parsing (simple key: value lines) --- */

static uint16_t parse_u16(const char* buf, const char* key) {
    const char* p = strstr(buf, key);
    if(!p) return 0;
    p += strlen(key);
    while(*p == ' ') p++;
    return (uint16_t)atoi(p);
}

static bool load_info(App* app, const char* name, AnimInfo* out) {
    char path[160];
    snprintf(path, sizeof(path), "%s/%s/info.txt", ANIM_DIR, name);
    File* f = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        size_t n = storage_file_read(f, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        out->width = parse_u16(buf, "Width:");
        out->height = parse_u16(buf, "Height:");
        out->frames = parse_u16(buf, "Frames:");
        out->frame_rate = parse_u16(buf, "Frame rate:");
        strncpy(out->name, name, sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        ok = out->width && out->height && out->frames;
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

static void scan_anims(App* app) {
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, ANIM_DIR)) {
        FileInfo fi;
        char name[64];
        while(app->anim_count < MAX_ANIMS &&
              storage_dir_read(dir, &fi, name, sizeof(name))) {
            /* A valid pack is any subfolder whose info.txt parses. */
            AnimInfo info;
            if(load_info(app, name, &info)) {
                app->anims[app->anim_count++] = info;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

/* --- playback (display takeover) --- */

static void input_cb(const void* value, void* ctx) {
    const InputEvent* e = value;
    App* app = ctx;
    if(e->key == InputKeyBack && (e->type == InputTypeShort || e->type == InputTypeLong)) {
        app->stop_playback = true;
    }
}

static void play_anim(App* app, AnimInfo* a) {
    char path[160];
    snprintf(path, sizeof(path), "%s/%s/frames.bin", ANIM_DIR, a->name);
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "open %s failed", path);
        storage_file_free(f);
        return;
    }

    size_t frame_bytes = (size_t)a->width * a->height * 2;
    /* Whole frame in one PSRAM buffer. The S3's GDMA reaches PSRAM, so we blit
     * the entire frame in a single esp_lcd transaction — no inter-stripe tearing
     * (the "scanlines" from blitting 16-row bands while the panel refreshed). */
    uint8_t* frame_buf = heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM);
    if(!frame_buf) {
        FURI_LOG_E(TAG, "frame alloc failed (%zu)", frame_bytes);
        goto cleanup;
    }

    /* Take exclusive display access and grab the raw panel. */
    gui_direct_draw_acquire(app->gui);
    esp_lcd_panel_handle_t panel = furi_hal_display_get_panel_handle();
    uint16_t pw = furi_hal_display_get_h_res();
    uint16_t ph = furi_hal_display_get_v_res();
    int16_t x_off = pw > a->width ? (pw - a->width) / 2 : 0;
    int16_t y_off = ph > a->height ? (ph - a->height) / 2 : 0;

    FuriPubSub* input = furi_record_open(RECORD_INPUT_EVENTS);
    FuriPubSubSubscription* sub = furi_pubsub_subscribe(input, input_cb, app);

    uint32_t delay = a->frame_rate ? (1000u / a->frame_rate) : 66;
    app->stop_playback = false;

    while(!app->stop_playback) {
        storage_file_seek(f, 0, true); /* loop from the first frame */
        for(uint16_t fr = 0; fr < a->frames && !app->stop_playback; fr++) {
            if(storage_file_read(f, frame_buf, frame_bytes) != frame_bytes) break;
            uint32_t t0 = furi_get_tick();
            furi_hal_spi_bus_lock();
            esp_lcd_panel_draw_bitmap(
                panel, x_off, y_off, x_off + a->width, y_off + a->height, frame_buf);
            furi_hal_spi_bus_unlock();
            /* Pace to the target rate, accounting for read+blit time. */
            uint32_t elapsed = (furi_get_tick() - t0) * (1000u / configTICK_RATE_HZ);
            if(elapsed < delay) furi_delay_ms(delay - elapsed);
        }
    }

    furi_pubsub_unsubscribe(input, sub);
    furi_record_close(RECORD_INPUT_EVENTS);
    gui_direct_draw_release(app->gui);

cleanup:
    if(frame_buf) free(frame_buf);
    storage_file_close(f);
    storage_file_free(f);
}

/* --- menu --- */

enum { VIEW_MENU = 0 };

static void submenu_cb(void* context, uint32_t index) {
    App* app = context;
    app->selected = (int32_t)index;
    view_dispatcher_stop(app->view_dispatcher);
}

static bool back_cb(void* context) {
    App* app = context;
    app->selected = -1;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static void build_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Color Animations");
    if(app->anim_count == 0) {
        submenu_add_item(app->submenu, "No packs found", 0, NULL, app);
    } else {
        for(size_t i = 0; i < app->anim_count; i++) {
            submenu_add_item(app->submenu, app->anims[i].name, (uint32_t)i, submenu_cb, app);
        }
    }
}

int32_t coloranim_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    scan_anims(app);

    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    build_menu(app);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_MENU, submenu_get_view(app->submenu));
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    bool running = true;
    while(running) {
        app->selected = -2;
        view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_MENU);
        view_dispatcher_run(app->view_dispatcher);
        if(app->selected >= 0 && (size_t)app->selected < app->anim_count) {
            play_anim(app, &app->anims[app->selected]);
        } else {
            running = false; /* Back on the menu exits */
        }
    }

    view_dispatcher_remove_view(app->view_dispatcher, VIEW_MENU);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
