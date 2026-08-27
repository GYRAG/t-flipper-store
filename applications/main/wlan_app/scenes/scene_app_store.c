#include "../wlan_app.h"
#include <stdio.h>

/* On-device WiFi app store scene. Three states share the existing views:
 *   List    -> submenu of catalog apps (select installs)
 *   Running -> widget with a status line + percent (refreshed on tick)
 *   Done    -> popup naming where the app landed
 *   Error   -> widget with the message + OK
 * The worker (wlan_app_store.*) does all the network + SD work off-thread. */

typedef enum {
    AppStoreStateFetching = 0,
    AppStoreStateList,
    AppStoreStateInstalling,
    AppStoreStateDone,
    AppStoreStateError,
} AppStoreState;

#define APP_STORE_DONE_POPUP_MS 2000

static void app_store_set_state(WlanApp* app, AppStoreState s) {
    scene_manager_set_scene_state(app->scene_manager, WlanAppSceneAppStore, s);
}

static AppStoreState app_store_get_state(WlanApp* app) {
    return (AppStoreState)scene_manager_get_scene_state(app->scene_manager, WlanAppSceneAppStore);
}

static void app_store_submenu_cb(void* context, uint32_t index) {
    WlanApp* app = context;
    /* Encode the entry index into the custom event so on_event can start it. */
    view_dispatcher_send_custom_event(
        app->view_dispatcher, WlanAppCustomEventAppStoreInstall + index);
}

static void app_store_error_ok_cb(GuiButtonType result, InputType type, void* context) {
    WlanApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WlanAppCustomEventAppStoreBack);
    }
}

static void app_store_done_popup_cb(void* context) {
    WlanApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WlanAppCustomEventAppStoreFinished);
}

/* ---- screens ---------------------------------------------------------- */

static void app_store_show_message(WlanApp* app, const char* title, const char* body) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignBottom, FontPrimary, title);
    if(body) {
        widget_add_text_box_element(
            app->widget, 0, 26, 128, 30, AlignCenter, AlignTop, body, false);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewWidget);
}

static void app_store_show_list(WlanApp* app) {
    WlanAppStore* s = app->app_store;
    submenu_reset(app->submenu);
    submenu_set_header_centered(app->submenu, "App Store");
    uint32_t n = wlan_app_store_count(s);
    for(uint32_t i = 0; i < n; i++) {
        char label[64];
        snprintf(
            label, sizeof(label), "%s  %luK", wlan_app_store_name(s, i),
            (unsigned long)((wlan_app_store_size(s, i) + 1023) / 1024));
        submenu_add_item(app->submenu, label, i, app_store_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewSubmenu);
}

static void app_store_show_running(WlanApp* app, const char* title, uint8_t pct, uint32_t kbps) {
    char line[48];
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 22, AlignCenter, AlignBottom, FontPrimary, title);
    if(kbps > 0) {
        snprintf(line, sizeof(line), "%u%%   %lu kB/s", pct, (unsigned long)kbps);
    } else {
        snprintf(line, sizeof(line), "%u%%", pct);
    }
    widget_add_string_element(
        app->widget, 64, 42, AlignCenter, AlignBottom, FontSecondary, line);
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewWidget);
}

static void app_store_show_error(WlanApp* app, const char* msg) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignBottom, FontPrimary, "Store error");
    widget_add_text_box_element(
        app->widget, 0, 22, 128, 26, AlignCenter, AlignTop, msg, false);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "OK", app_store_error_ok_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewWidget);
}

static void app_store_show_done(WlanApp* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Installed", 64, 12, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, wlan_app_store_get_dest(app->app_store), 64, 34, AlignCenter, AlignCenter);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, app_store_done_popup_cb);
    popup_set_timeout(app->popup, APP_STORE_DONE_POPUP_MS);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, WlanAppViewPopup);
}

/* ---- lifecycle -------------------------------------------------------- */

void wlan_app_scene_app_store_on_enter(void* context) {
    WlanApp* app = context;
    app->app_store_flow = false; // flow reached, flag consumed
    app_store_set_state(app, AppStoreStateFetching);
    app_store_show_message(app, "App Store", "Loading catalog...");
    wlan_app_store_fetch(app->app_store);
}

bool wlan_app_scene_app_store_on_event(void* context, SceneManagerEvent event) {
    WlanApp* app = context;
    WlanAppStore* s = app->app_store;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WlanAppCustomEventAppStoreBack ||
           event.event == WlanAppCustomEventAppStoreFinished) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, WlanAppSceneMain);
            consumed = true;
        } else if(event.event >= WlanAppCustomEventAppStoreInstall) {
            uint32_t index = event.event - WlanAppCustomEventAppStoreInstall;
            if(index < wlan_app_store_count(s)) {
                app_store_set_state(app, AppStoreStateInstalling);
                app_store_show_running(app, "Installing...", 0, 0);
                wlan_app_store_install(s, index);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        AppStoreState st = app_store_get_state(app);
        WlanAppStorePhase ph = wlan_app_store_get_phase(s);

        if(st == AppStoreStateFetching) {
            if(ph == WlanAppStoreReady) {
                app_store_set_state(app, AppStoreStateList);
                app_store_show_list(app);
            } else if(ph == WlanAppStoreError) {
                app_store_set_state(app, AppStoreStateError);
                app_store_show_error(app, wlan_app_store_get_error(s));
            }
        } else if(st == AppStoreStateInstalling) {
            if(ph == WlanAppStoreInstalling) {
                app_store_show_running(
                    app, "Installing...", wlan_app_store_get_percent(s),
                    wlan_app_store_get_speed_kbps(s));
            } else if(ph == WlanAppStoreDone) {
                app_store_set_state(app, AppStoreStateDone);
                app_store_show_done(app);
            } else if(ph == WlanAppStoreError) {
                app_store_set_state(app, AppStoreStateError);
                app_store_show_error(app, wlan_app_store_get_error(s));
            }
        }
    }

    return consumed;
}

void wlan_app_scene_app_store_on_exit(void* context) {
    WlanApp* app = context;
    /* However we leave: stop the worker and wait, so no background task touches
     * freed state or the catalog buffer after we're gone. */
    wlan_app_store_cancel(app->app_store);
    popup_reset(app->popup);
    widget_reset(app->widget);
    submenu_reset(app->submenu);
    app_store_set_state(app, AppStoreStateFetching);
}
