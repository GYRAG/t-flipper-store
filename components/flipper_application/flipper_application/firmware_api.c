    /**
 * @brief Firmware API interface for ESP32 port.
 * Exposes firmware functions to dynamically loaded FAP applications.
 *
 * To regenerate after adding new APIs:
 *   xtensa-esp32s3-elf-nm -u app.fap | grep "^         U " | sed 's/^         U //' | \
 *     grep -v '^__\|^I_' | sort > /tmp/syms.txt
 *   python3 tools/gen_api_table.py -f /tmp/syms.txt
 *
 * The table MUST be sorted by hash value.
 */
#include "api_hashtable/api_hashtable.h"
#include <gui/modules/dialog_ex.h>
#include <notification/notification.h>
#include <toolbox/saved_struct.h>
#include <furi_hal_infrared.h>
#include "../../../lib/infrared/worker/infrared_worker.h"
#include "../../../lib/infrared/worker/infrared_transmit.h"
#include <assets_icons.h>
#include <errno.h>
#include <gui/icon.h>
#include <gui/icon_i.h>
#include <furi.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/byte_input.h>
#include <gui/icon.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <bt/bt_service/bt.h>
#include <ble_profile/extra_profiles/serial_profile.h>
#include <dialogs/dialogs.h>
#include <dolphin/dolphin.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/hex.h>
#include <toolbox/manchester_decoder.h>
#include <toolbox/path.h>
#include <furi_hal_gpio.h>
#include <furi_hal_rtc.h>
#include <esp_mac.h>
#include <toolbox/dir_walk.h>
#include <furi_hal_version.h>
#include <furi_hal_random.h>
#include <furi_hal_crypto.h>
#include <furi_hal_spi_bus.h>
#include <furi_hal_speaker.h>
#include <furi_hal_display.h>
#include <furi_hal_bt.h>
#include <locale/locale.h>
/* NFC supported_cards parser plugin API symbols. The `nfc` component exports
 * components/nfc ("." ) so the transitive <protocols/...> includes resolve. */
 #include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <protocols/iso14443_4a/iso14443_4a_poller.h>
#include <protocols/iso14443_4b/iso14443_4b_poller.h>
#include <toolbox/bit_buffer.h>
#include <gui/modules/number_input.h>
#include <mbedtls/des.h>
#include <mbedtls/sha1.h>
#include <assets_icons.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_desfire/mf_desfire.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
/* NFC listener/poller + iso14443_3a + helpers — for the mifare_fuzzer and
 * nfc_magic FAPs (T-Embed has an onboard PN532). */
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/helpers/iso14443_crc.h>
#include <nfc/helpers/nfc_data_generator.h>
#include <nfc/helpers/nfc_util.h>
#include <bit_lib.h>
#include <datetime.h>
#include <toolbox/pretty_format.h>
#include <toolbox/simple_array.h>
#include <toolbox/strint.h>
/* API symbols used by external apps (e.g. the authenticator TOTP app): BLE HID
 * profile, USB HID/config, embedded-plugin resolver, CLI registry, arg parsing
 * and pipes, and the flipper_application loader API. */
#include <ble_profile/extra_profiles/hid_profile.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/composite_resolver.h>
#include <flipper_application/plugins/plugin_manager.h> /* protopirate loads protocol plugins */
#include <toolbox/args.h>
#include <toolbox/pipe.h>
#include <toolbox/cli/cli_registry.h>
#include <toolbox/cli/cli_command.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <mbedtls/sha1.h>
/* libgcc soft-float / 64-bit helpers pulled in by parsers doing double or
 * 64-bit arithmetic. No header — declare for the API-table address-of. */
extern unsigned long long __fixunsdfdi(double);
extern double __floatundidf(unsigned long long);
extern unsigned long long __umoddi3(unsigned long long, unsigned long long);
/* <math.h> doesn't declare these here (newlib config) — declare explicitly. */
extern double pow(double, double);
extern double floor(double);
/* subghz headers - resolved via subghz component INCLUDE_DIRS */
#include "devices.h"
#include "receiver.h"
#include "transmitter.h"
#include "environment.h"
#include "subghz_setting.h"
#include "subghz_worker.h"
#include "subghz_keystore.h"
#include "base.h"
#include "blocks/generic.h"
#include "protocol_items.h"
#include "subghz_protocol_registry.h"
#include "blocks/math.h"
#include "blocks/decoder.h"

#include <mjs_core_public.h>
#include <mjs_exec_public.h>
#include <mjs_object_public.h>
#include <mjs_string_public.h>
#include <mjs_array_public.h>
#include <mjs_primitive_public.h>
#include <mjs_util_public.h>
#include <mjs_array_buf_public.h>
#include <mjs_ffi_public.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Direkte extern-Deklarationen statt <ctype.h>/<strings.h>/<sys/stat.h>:
 * diese System-Header ziehen weitere Header rein, die die Sichtbarkeit
 * von subghz_protocol_registry / ble_profile_serial verändern (sie würden
 * dann als komplette Strukturen statt als Forward-Declared-Pointer auftauchen
 * und (uint32_t)var wäre kein constant initializer mehr). Außerdem versteckt
 * <sys/stat.h> indirekt das fabs-builtin in math.h. */
extern const char _ctype_[];
extern int strcasecmp(const char*, const char*);
extern int strncasecmp(const char*, const char*, size_t);
extern int mkdir(const char*, unsigned int);
extern double fabs(double);

/* libgcc soft-float helper used by FAPs that touch doubles */
extern int __ltdf2(double, double);
extern int __gedf2(double, double);

/* math functions needed by FAPs but not declared in ESP-IDF newlib math.h with -std=gnu17 */
extern float cosf(float);
extern float sinf(float);
extern float log10f(float);
extern float log2f(float);
extern float fmaxf(float, float);
extern double atan(double);
extern double atan2(double, double);
extern float  atan2f(float, float);
extern double tan(double);

/* setjmp/longjmp aus newlib (kein <setjmp.h>-Include weil wir uint32_t-cast wollen) */
#include <setjmp.h>

/* esp_lcd panel ops für FAP display takeover */
#include <esp_lcd_panel_ops.h>

/* BT controller release (for FAP games that need extra DRAM) */
#include <esp_bt.h>

/* I2S für FAP-Audio (wolf3d, doom-music, ...) */
#include <driver/i2s_std.h>
#include <driver/gpio.h>

/* WiFi / networking / legacy-I2C / logging / socket symbols exported for user FAPs
 * (reverse_shell: esp_wifi + esp_netif + esp_event + lwip sockets; rfid2_reader:
 * legacy i2c driver + esp_log/esp_rom). Declared as bare externs ON PURPOSE — the
 * real headers (esp_wifi.h, lwip sockets/netdb, unistd.h, ...) transitively pull sys/stat.h,
 * which conflicts with the hand-rolled mkdir() above AND turns
 * subghz_protocol_registry / ble_profile_serial into full structs, breaking the
 * (uint32_t)&var constant-initializers (see the note at line 143). We only take
 * addresses here, so unprototyped signatures suffice. FreeRTOS (vTaskDelay/…) and
 * strtok_r are already declared via furi.h / <string.h> — do not re-declare them. */
extern int close();
extern int i2c_driver_install();
extern int i2c_param_config();
extern int i2c_master_write_to_device();
extern int i2c_master_write_read_device();
extern void esp_rom_delay_us();
extern void esp_log_write();
extern uint32_t esp_log_timestamp();
extern size_t heap_caps_get_largest_free_block();
extern int esp_wifi_init();
extern int esp_wifi_deinit();
extern int esp_wifi_start();
extern int esp_wifi_stop();
extern int esp_wifi_connect();
extern int esp_wifi_disconnect();
extern int esp_wifi_set_mode();
extern int esp_wifi_set_config();
extern int esp_wifi_set_storage();
extern int esp_wifi_set_max_tx_power();
extern int esp_netif_init();
extern void* esp_netif_create_default_wifi_sta();
extern void* esp_netif_get_handle_from_ifkey();
extern int esp_event_loop_create_default();
extern int esp_event_handler_register();
extern int esp_event_handler_unregister();
extern int lwip_socket();
extern int lwip_connect();
extern int lwip_recv();
extern int lwip_send();
extern int lwip_setsockopt();
extern void* lwip_gethostbyname();
extern uint32_t ipaddr_addr();
/* data symbols — address is taken with & in the table below */
extern char WIFI_EVENT;
extern char IP_EVENT;
extern char g_wifi_osi_funcs;
extern char g_wifi_default_wpa_crypto_funcs;
/* storage_file_sync is defined in storage.c but not declared in storage.h */
extern bool storage_file_sync(File* file);

/* subghz functions provided via blocks/decoder.h above */

/* GCC runtime helpers (from libgcc, linked into firmware) */
extern long long __udivdi3(long long, long long);
extern long long __divdi3(long long, long long);
extern long long __moddi3(long long, long long);
extern double __divdf3(double, double);
extern float __divsf3(float, float);
extern double __muldf3(double, double);
extern long long __ashldi3(long long, int);
extern long long __lshrdi3(long long, int);
extern double __floatsidf(int);
extern float __truncdfsf2(double);
extern unsigned int __bswapsi2(unsigned int);
extern unsigned long long __bswapdi2(unsigned long long);
extern double __extendsfdf2(float);
extern float __floatundisf(unsigned long long);
extern double __adddf3(double, double);
extern int __fixdfsi(double);
extern unsigned int __fixunsdfsi(double);
extern double __floatunsidf(unsigned int);
extern int __paritysi2(unsigned int);

/* Newlib runtime */
#include <sys/reent.h>

/* ROM functions */
extern int esp_rom_printf(const char* fmt, ...);

/* Icon assets (global variables in firmware) */
#include <assets_icons.h>

/* Added by the API export audit: headers whose public functions are now
 * exported to FAPs (previously the table referenced only a subset). */
#include <dialogs/dialogs/dialogs_module_file_browser.h>
#include <dialogs/dialogs/dialogs_module_message.h>
#include <flipper_format/flipper_format_stream.h>
#include <furi/core/event_loop_thread_flag_interface.h>
#include <furi/flipper.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/button_panel.h>
#include <gui/modules/empty_screen.h>
#include <gui/modules/file_browser_worker.h>
#include <gui/modules/menu.h>
#include <gui/view_holder.h>
#include <gui/view_stack.h>
#include <notification/notification_app.h>
#include <toolbox/cli/cli_ansi.h>
#include <toolbox/cli/shell/cli_shell.h>
#include <toolbox/cli/shell/cli_shell_completions.h>
#include <toolbox/cli/shell/cli_shell_line.h>
#include <toolbox/compress.h>
#include <toolbox/float_tools.h>
#include <toolbox/keys_dict.h>
#include <toolbox/manchester_encoder.h>
#include <toolbox/md5_calc.h>
#include <toolbox/name_generator.h>
#include <toolbox/settings_helpers/submenu_based.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <toolbox/stream/stream_cache.h>
#include <toolbox/stream/string_stream.h>
#include <toolbox/value_index.h>

/* Defined at the bottom of this file but referenced in the table below. Apps
 * (e.g. the authenticator) use it to build a composite resolver combining the
 * firmware API with their own embedded-plugin API. */
extern const ElfApiInterface* const firmware_api_interface;

/* clang-format off */
static const struct sym_entry firmware_api_table[] = {
    { .hash = 0x0020e7b6, .address = (uint32_t)flipper_format_update_float }, /* flipper_format_update_float */
    { .hash = 0x00454575, .address = (uint32_t)variable_item_list_set_selected_item }, /* variable_item_list_set_selected_item */
    { .hash = 0x0052f904, .address = (uint32_t)scene_manager_search_and_switch_to_another_scene }, /* scene_manager_search_and_switch_to_another_scene */
    { .hash = 0x00acf266, .address = (uint32_t)pretty_format_bytes_hex_canonical }, /* pretty_format_bytes_hex_canonical */
    { .hash = 0x00cba46b, .address = (uint32_t)mjs_prepend_errorf }, /* mjs_prepend_errorf */
    { .hash = 0x00dd3524, .address = (uint32_t)furi_string_replace_str }, /* furi_string_replace_str */
    { .hash = 0x00eaa80e, .address = (uint32_t)furi_hal_bt_extra_beacon_set_config }, /* furi_hal_bt_extra_beacon_set_config */
    { .hash = 0x013a57de, .address = (uint32_t)cli_shell_alloc }, /* cli_shell_alloc */
    { .hash = 0x017acb10, .address = (uint32_t)submenu_settings_helpers_scene_enter }, /* submenu_settings_helpers_scene_enter */
    { .hash = 0x017eef74, .address = (uint32_t)submenu_settings_helpers_scene_event }, /* submenu_settings_helpers_scene_event */
    { .hash = 0x0187d19c, .address = (uint32_t)input_get_key_name }, /* input_get_key_name */
    { .hash = 0x020f94cc, .address = (uint32_t)furi_hal_infrared_detect_tx_output }, /* furi_hal_infrared_detect_tx_output */
    { .hash = 0x0221ca54, .address = (uint32_t)view_allocate_model }, /* view_allocate_model */
    { .hash = 0x028445a1, .address = (uint32_t)cli_shell_start }, /* cli_shell_start */
    { .hash = 0x028d40d4, .address = (uint32_t)mjs_sprintf }, /* mjs_sprintf */
    { .hash = 0x02f86a97, .address = (uint32_t)mjs_set_exec_flags_poller }, /* mjs_set_exec_flags_poller */
    { .hash = 0x0300bcd5, .address = (uint32_t)subghz_transmitter_free }, /* subghz_transmitter_free */
    { .hash = 0x0307e799, .address = (uint32_t)subghz_transmitter_stop }, /* subghz_transmitter_stop */
    { .hash = 0x031b8511, .address = (uint32_t)subghz_block_generic_deserialize_check_count_bit }, /* subghz_block_generic_deserialize_check_count_bit */
    { .hash = 0x035bd872, .address = (uint32_t)md5_string_calc_file }, /* md5_string_calc_file */
    { .hash = 0x0364d57c, .address = (uint32_t)cli_shell_line_free }, /* cli_shell_line_free */
    { .hash = 0x0406a3f5, .address = (uint32_t)gui_remove_view_port }, /* gui_remove_view_port */
    { .hash = 0x043e5019, .address = (uint32_t)&furi_hal_spi_bus_handle_nrf24 }, /* furi_hal_spi_bus_handle_nrf24 */
    { .hash = 0x04a1a19a, .address = (uint32_t)furi_string_cat_printf }, /* furi_string_cat_printf */
    { .hash = 0x04af1dec, .address = (uint32_t)subghz_protocol_blocks_add_bit }, /* subghz_protocol_blocks_add_bit */
    { .hash = 0x04ff5882, .address = (uint32_t)canvas_set_font }, /* canvas_set_font */
    { .hash = 0x0542ffa8, .address = (uint32_t)furi_hal_bt_stop_advertising }, /* furi_hal_bt_stop_advertising */
    { .hash = 0x05466acc, .address = (uint32_t)furi_record_init }, /* furi_record_init */
    { .hash = 0x0549bd0a, .address = (uint32_t)furi_record_open }, /* furi_record_open */
    { .hash = 0x05b9c541, .address = (uint32_t)view_free }, /* view_free */
    { .hash = 0x06716444, .address = (uint32_t)mjs_is_null }, /* mjs_is_null */
    { .hash = 0x0674f232, .address = (uint32_t)&sequence_blink_stop }, /* sequence_blink_stop */
    { .hash = 0x0688626a, .address = (uint32_t)mjs_call }, /* mjs_call */
    { .hash = 0x0689dc13, .address = (uint32_t)mjs_exec }, /* mjs_exec */
    { .hash = 0x0689dca8, .address = (uint32_t)mjs_exit }, /* mjs_exit */
    { .hash = 0x068e7d2d, .address = (uint32_t)mjs_next }, /* mjs_next */
    { .hash = 0x06cd2ea2, .address = (uint32_t)furi_hal_speaker_acquire }, /* furi_hal_speaker_acquire */
    { .hash = 0x070f1ab9, .address = (uint32_t)furi_event_loop_unsubscribe }, /* furi_event_loop_unsubscribe */
    { .hash = 0x0726391d, .address = (uint32_t)elements_button_center }, /* elements_button_center */
    { .hash = 0x074563d8, .address = (uint32_t)esp_log_write }, /* esp_log_write */
    { .hash = 0x07702a63, .address = (uint32_t)compress_icon_alloc }, /* compress_icon_alloc */
    { .hash = 0x0778b100, .address = (uint32_t)infrared_get_protocol_name }, /* infrared_get_protocol_name */
    { .hash = 0x0781e22e, .address = (uint32_t)furi_hal_infrared_async_tx_start }, /* furi_hal_infrared_async_tx_start */
    { .hash = 0x07be22a9, .address = (uint32_t)elements_progress_bar_with_text }, /* elements_progress_bar_with_text */
    { .hash = 0x07ef7708, .address = (uint32_t)mf_ultralight_get_pages_total }, /* mf_ultralight_get_pages_total */
    { .hash = 0x08d63d78, .address = (uint32_t)flipper_format_file_open_new }, /* flipper_format_file_open_new */
    { .hash = 0x0911e126, .address = (uint32_t)furi_thread_set_signal_callback }, /* furi_thread_set_signal_callback */
    { .hash = 0x0913e1dd, .address = (uint32_t)stream_copy_full }, /* stream_copy_full */
    { .hash = 0x0922fe88, .address = (uint32_t)view_dispatcher_add_view }, /* view_dispatcher_add_view */
    { .hash = 0x093be3cf, .address = (uint32_t)storage_common_remove }, /* storage_common_remove */
    { .hash = 0x093c3379, .address = (uint32_t)storage_common_rename }, /* storage_common_rename */
    { .hash = 0x095e34aa, .address = (uint32_t)furi_hal_subghz_rx }, /* furi_hal_subghz_rx */
    { .hash = 0x0976df4c, .address = (uint32_t)furi_hal_usb_get_config }, /* furi_hal_usb_get_config */
    { .hash = 0x098da528, .address = (uint32_t)mf_classic_is_card_read }, /* mf_classic_is_card_read */
    { .hash = 0x099ec15c, .address = (uint32_t)&I_Lock_7x8 }, /* I_Lock_7x8 */
    { .hash = 0x09a70073, .address = (uint32_t)submenu_settings_helpers_assign_objects }, /* submenu_settings_helpers_assign_objects */
    { .hash = 0x09b86ac7, .address = (uint32_t)flipper_format_rewind }, /* flipper_format_rewind */
    { .hash = 0x09d161df, .address = (uint32_t)mbedtls_des3_free }, /* mbedtls_des3_free */
    { .hash = 0x09d2f691, .address = (uint32_t)mbedtls_des3_init }, /* mbedtls_des3_init */
    { .hash = 0x09f02023, .address = (uint32_t)furi_hal_usb_unlock }, /* furi_hal_usb_unlock */
    { .hash = 0x0a06dba0, .address = (uint32_t)bit_buffer_write_bytes_with_parity }, /* bit_buffer_write_bytes_with_parity */
    { .hash = 0x0a55a9de, .address = (uint32_t)furi_hal_infrared_async_tx_set_data_isr_callback }, /* furi_hal_infrared_async_tx_set_data_isr_callback */
    { .hash = 0x0a669169, .address = (uint32_t)path_append }, /* path_append */
    { .hash = 0x0b2dd07d, .address = (uint32_t)mjs_array_buf_get_ptr }, /* mjs_array_buf_get_ptr */
    { .hash = 0x0b2e586c, .address = (uint32_t)mbedtls_des_free }, /* mbedtls_des_free */
    { .hash = 0x0b2fed1e, .address = (uint32_t)mbedtls_des_init }, /* mbedtls_des_init */
    { .hash = 0x0b885c9b, .address = (uint32_t)abs }, /* abs */
    { .hash = 0x0b889e1b, .address = (uint32_t)pow }, /* pow */
    { .hash = 0x0b88ad48, .address = (uint32_t)tan }, /* tan */
    { .hash = 0x0b9a71c7, .address = (uint32_t)gui_remove_framebuffer_callback }, /* gui_remove_framebuffer_callback */
    { .hash = 0x0bac2e75, .address = (uint32_t)furi_kernel_get_tick_frequency }, /* furi_kernel_get_tick_frequency */
    { .hash = 0x0bde1aae, .address = (uint32_t)log10f }, /* log10f */
    { .hash = 0x0bfc4843, .address = (uint32_t)furi_string_end_withi_str }, /* furi_string_end_withi_str */
    { .hash = 0x0c3ae7d9, .address = (uint32_t)locale_get_date_format }, /* locale_get_date_format */
    { .hash = 0x0c3bc234, .address = (uint32_t)elements_button_down }, /* elements_button_down */
    { .hash = 0x0c3ff887, .address = (uint32_t)elements_button_left }, /* elements_button_left */
    { .hash = 0x0cb05ef6, .address = (uint32_t)&message_note_c5 }, /* message_note_c5 */
    { .hash = 0x0cb05ef7, .address = (uint32_t)&message_note_c6 }, /* message_note_c6 */
    { .hash = 0x0cb05f17, .address = (uint32_t)&message_note_d5 }, /* message_note_d5 */
    { .hash = 0x0cb05f37, .address = (uint32_t)&message_note_e4 }, /* message_note_e4 */
    { .hash = 0x0cb05f57, .address = (uint32_t)&message_note_f3 }, /* message_note_f3 */
    { .hash = 0x0d175eee, .address = (uint32_t)notification_message_save_settings }, /* notification_message_save_settings */
    { .hash = 0x0d39ad3d, .address = (uint32_t)malloc }, /* malloc */
    { .hash = 0x0d67f2c3, .address = (uint32_t)composite_api_resolver_free }, /* composite_api_resolver_free */
    { .hash = 0x0d69b738, .address = (uint32_t)__umoddi3 }, /* __umoddi3 */
    { .hash = 0x0d7c905a, .address = (uint32_t)view_port_set_orientation }, /* view_port_set_orientation */
    { .hash = 0x0d827481, .address = (uint32_t)memchr }, /* memchr */
    { .hash = 0x0d827524, .address = (uint32_t)memcmp }, /* memcmp */
    { .hash = 0x0d827590, .address = (uint32_t)memcpy }, /* memcpy */
    { .hash = 0x0d82b830, .address = (uint32_t)memset }, /* memset */
    { .hash = 0x0d861c95, .address = (uint32_t)stream_cache_pos }, /* stream_cache_pos */
    { .hash = 0x0dab71d0, .address = (uint32_t)submenu_settings_helpers_alloc }, /* submenu_settings_helpers_alloc */
    { .hash = 0x0ddbbb90, .address = (uint32_t)scene_manager_search_and_switch_to_previous_scene }, /* scene_manager_search_and_switch_to_previous_scene */
    { .hash = 0x0ddfc2b8, .address = (uint32_t)plugin_manager_alloc }, /* plugin_manager_alloc */
    { .hash = 0x0de49867, .address = (uint32_t)&message_vibro_on }, /* message_vibro_on */
    { .hash = 0x0e060515, .address = (uint32_t)&message_green_255 }, /* message_green_255 */
    { .hash = 0x0e4bea15, .address = (uint32_t)&sequence_blink_start_yellow }, /* sequence_blink_start_yellow */
    { .hash = 0x0e4dce53, .address = (uint32_t)view_set_previous_callback }, /* view_set_previous_callback */
    { .hash = 0x0e51614d, .address = (uint32_t)name_generator_make_auto_datetime }, /* name_generator_make_auto_datetime */
    { .hash = 0x0e531461, .address = (uint32_t)cli_shell_line_set_line_position }, /* cli_shell_line_set_line_position */
    { .hash = 0x0eab4808, .address = (uint32_t)furi_string_cat }, /* furi_string_cat */
    { .hash = 0x0eab4990, .address = (uint32_t)furi_string_cmp }, /* furi_string_cmp */
    { .hash = 0x0eab738a, .address = (uint32_t)furi_string_mid }, /* furi_string_mid */
    { .hash = 0x0eab8c9c, .address = (uint32_t)furi_string_set }, /* furi_string_set */
    { .hash = 0x0efda749, .address = (uint32_t)path_concat }, /* path_concat */
    { .hash = 0x0f11ed7d, .address = (uint32_t)abort }, /* abort */
    { .hash = 0x0f3b9a5b, .address = (uint32_t)close }, /* close */
    { .hash = 0x0f71e367, .address = (uint32_t)floor }, /* floor */
    { .hash = 0x0f723557, .address = (uint32_t)fmaxf }, /* fmaxf */
    { .hash = 0x0f738b7d, .address = (uint32_t)fopen }, /* fopen */
    { .hash = 0x0f750147, .address = (uint32_t)fread }, /* fread */
    { .hash = 0x0f758e33, .address = (uint32_t)fseek }, /* fseek */
    { .hash = 0x0f761b7c, .address = (uint32_t)ftell }, /* ftell */
    { .hash = 0x0f95269b, .address = (uint32_t)dialog_ex_set_center_button_text }, /* dialog_ex_set_center_button_text */
    { .hash = 0x0fa010db, .address = (uint32_t)subghz_worker_alloc }, /* subghz_worker_alloc */
    { .hash = 0x0fc57c2c, .address = (uint32_t)furi_string_reserve }, /* furi_string_reserve */
    { .hash = 0x0fdff19f, .address = (uint32_t)log2f }, /* log2f */
    { .hash = 0x0fe7c75a, .address = (uint32_t)file_stream_alloc }, /* file_stream_alloc */
    { .hash = 0x0fefd2fc, .address = (uint32_t)mkdir }, /* mkdir */
    { .hash = 0x0ff20411, .address = (uint32_t)modff }, /* modff */
    { .hash = 0x10070181, .address = (uint32_t)file_stream_open }, /* file_stream_open */
    { .hash = 0x100c05a5, .address = (uint32_t)file_stream_close }, /* file_stream_close */
    { .hash = 0x1022b32e, .address = (uint32_t)version_get_dirty_flag }, /* version_get_dirty_flag */
    { .hash = 0x1039c338, .address = (uint32_t)bt_profile_start }, /* bt_profile_start */
    { .hash = 0x103cc7fe, .address = (uint32_t)qsort }, /* qsort */
    { .hash = 0x1060307d, .address = (uint32_t)srand }, /* srand */
    { .hash = 0x109f2fd8, .address = (uint32_t)&sequence_display_backlight_enforce_on }, /* sequence_display_backlight_enforce_on */
    { .hash = 0x10e9fe9e, .address = (uint32_t)subghz_worker_start }, /* subghz_worker_start */
    { .hash = 0x10ec782d, .address = (uint32_t)&I_DolphinDone_80x58 }, /* I_DolphinDone_80x58 */
    { .hash = 0x10fdc972, .address = (uint32_t)byte_input_alloc }, /* byte_input_alloc */
    { .hash = 0x11115707, .address = (uint32_t)widget_add_rect_element }, /* widget_add_rect_element */
    { .hash = 0x117677e3, .address = (uint32_t)furi_hal_subghz_reset }, /* furi_hal_subghz_reset */
    { .hash = 0x118c2b79, .address = (uint32_t)furi_hal_subghz_sleep }, /* furi_hal_subghz_sleep */
    { .hash = 0x11f250cb, .address = (uint32_t)furi_hal_spi_bus_lock }, /* furi_hal_spi_bus_lock */
    { .hash = 0x1214bfff, .address = (uint32_t)__bswapdi2 }, /* __bswapdi2 */
    { .hash = 0x1214ffce, .address = (uint32_t)__bswapsi2 }, /* __bswapsi2 */
    { .hash = 0x128c39a3, .address = (uint32_t)memmgr_pool_get_free }, /* memmgr_pool_get_free */
    { .hash = 0x12c73778, .address = (uint32_t)locale_get_measurement_unit }, /* locale_get_measurement_unit */
    { .hash = 0x12e69e97, .address = (uint32_t)iso14443_3a_poller_txrx_custom_parity }, /* iso14443_3a_poller_txrx_custom_parity */
    { .hash = 0x135baa10, .address = (uint32_t)notification_message }, /* notification_message */
    { .hash = 0x13a0031a, .address = (uint32_t)popup_free }, /* popup_free */
    { .hash = 0x13b426d3, .address = (uint32_t)uint8_to_hex_chars }, /* uint8_to_hex_chars */
    { .hash = 0x14aaf82f, .address = (uint32_t)simple_array_cget }, /* simple_array_cget */
    { .hash = 0x14ab1ba7, .address = (uint32_t)simple_array_copy }, /* simple_array_copy */
    { .hash = 0x14accc0e, .address = (uint32_t)simple_array_free }, /* simple_array_free */
    { .hash = 0x14ae60c0, .address = (uint32_t)simple_array_init }, /* simple_array_init */
    { .hash = 0x152fa781, .address = (uint32_t)ble_profile_serial_notify_buffer_is_empty }, /* ble_profile_serial_notify_buffer_is_empty */
    { .hash = 0x156b2bb8, .address = (uint32_t)printf }, /* printf */
    { .hash = 0x15bfef3f, .address = (uint32_t)view_stack_alloc }, /* view_stack_alloc */
    { .hash = 0x15db55f6, .address = (uint32_t)memmgr_heap_get_max_free_block }, /* memmgr_heap_get_max_free_block */
    { .hash = 0x16464ad9, .address = (uint32_t)elf_resolve_from_hashtable }, /* elf_resolve_from_hashtable */
    { .hash = 0x164d8c44, .address = (uint32_t)icon_animation_get_height }, /* icon_animation_get_height */
    { .hash = 0x165864ee, .address = (uint32_t)manchester_encoder_finish }, /* manchester_encoder_finish */
    { .hash = 0x1688c9d5, .address = (uint32_t)pipe_receive }, /* pipe_receive */
    { .hash = 0x16ba1f63, .address = (uint32_t)furi_thread_enable_heap_trace }, /* furi_thread_enable_heap_trace */
    { .hash = 0x17481073, .address = (uint32_t)furi_hal_crypto_encrypt }, /* furi_hal_crypto_encrypt */
    { .hash = 0x1761a36b, .address = (uint32_t)__ashldi3 }, /* __ashldi3 */
    { .hash = 0x177a11ff, .address = (uint32_t)subghz_protocol_decoder_base_serialize }, /* subghz_protocol_decoder_base_serialize */
    { .hash = 0x17e06ddf, .address = (uint32_t)nfc_util_odd_parity8 }, /* nfc_util_odd_parity8 */
    { .hash = 0x182e12dc, .address = (uint32_t)text_box_alloc }, /* text_box_alloc */
    { .hash = 0x182f2f55, .address = (uint32_t)bt_stop_stack }, /* bt_stop_stack */
    { .hash = 0x1830f075, .address = (uint32_t)input_get_type_name }, /* input_get_type_name */
    { .hash = 0x1852118a, .address = (uint32_t)lwip_connect }, /* lwip_connect */
    { .hash = 0x187a1895, .address = (uint32_t)furi_string_search_rchar }, /* furi_string_search_rchar */
    { .hash = 0x18a5ff02, .address = (uint32_t)memmgr_get_minimum_free_heap }, /* memmgr_get_minimum_free_heap */
    { .hash = 0x18b0bc19, .address = (uint32_t)furi_event_loop_subscribe_semaphore }, /* furi_event_loop_subscribe_semaphore */
    { .hash = 0x18e46f26, .address = (uint32_t)random }, /* random */
    { .hash = 0x19043ee7, .address = (uint32_t)furi_event_loop_pend_callback }, /* furi_event_loop_pend_callback */
    { .hash = 0x191d9eb3, .address = (uint32_t)scene_manager_handle_custom_event }, /* scene_manager_handle_custom_event */
    { .hash = 0x192c7473, .address = (uint32_t)remove }, /* remove */
    { .hash = 0x192cc41d, .address = (uint32_t)rename }, /* rename */
    { .hash = 0x195df954, .address = (uint32_t)text_box_reset }, /* text_box_reset */
    { .hash = 0x19770b31, .address = (uint32_t)mjs_get_stack_trace }, /* mjs_get_stack_trace */
    { .hash = 0x19d82b07, .address = (uint32_t)mjs_fprintf }, /* mjs_fprintf */
    { .hash = 0x1a040fe3, .address = (uint32_t)nfc_device_get_data }, /* nfc_device_get_data */
    { .hash = 0x1a098aca, .address = (uint32_t)nfc_device_get_name }, /* nfc_device_get_name */
    { .hash = 0x1a12d6a6, .address = (uint32_t)variable_item_set_current_value_text }, /* variable_item_set_current_value_text */
    { .hash = 0x1af7372b, .address = (uint32_t)flipper_application_free }, /* flipper_application_free */
    { .hash = 0x1b4bc594, .address = (uint32_t)vfprintf }, /* vfprintf */
    { .hash = 0x1b855d58, .address = (uint32_t)setjmp }, /* setjmp */
    { .hash = 0x1c270cad, .address = (uint32_t)subghz_receiver_free }, /* subghz_receiver_free */
    { .hash = 0x1c6ce60d, .address = (uint32_t)text_input_get_validator_callback_context }, /* text_input_get_validator_callback_context */
    { .hash = 0x1c793bc3, .address = (uint32_t)sscanf }, /* sscanf */
    { .hash = 0x1c9394d6, .address = (uint32_t)strcat }, /* strcat */
    { .hash = 0x1c9395bb, .address = (uint32_t)strchr }, /* strchr */
    { .hash = 0x1c93965e, .address = (uint32_t)strcmp }, /* strcmp */
    { .hash = 0x1c9396ca, .address = (uint32_t)strcpy }, /* strcpy */
    { .hash = 0x1c939ba7, .address = (uint32_t)strdup }, /* strdup */
    { .hash = 0x1c93bb9d, .address = (uint32_t)strlen }, /* strlen */
    { .hash = 0x1c93db57, .address = (uint32_t)strstr }, /* strstr */
    { .hash = 0x1c93dee7, .address = (uint32_t)strtof }, /* strtof */
    { .hash = 0x1c93deec, .address = (uint32_t)strtok }, /* strtok */
    { .hash = 0x1c93deed, .address = (uint32_t)strtol }, /* strtol */
    { .hash = 0x1ce68f24, .address = (uint32_t)mjs_mk_array }, /* mjs_mk_array */
    { .hash = 0x1ceee48a, .address = (uint32_t)system }, /* system */
    { .hash = 0x1d24809b, .address = (uint32_t)variable_item_set_values_count }, /* variable_item_set_values_count */
    { .hash = 0x1d7e711b, .address = (uint32_t)mf_classic_get_first_block_num_of_sector }, /* mf_classic_get_first_block_num_of_sector */
    { .hash = 0x1d863265, .address = (uint32_t)simple_array_get_data }, /* simple_array_get_data */
    { .hash = 0x1d91dfbd, .address = (uint32_t)empty_screen_alloc }, /* empty_screen_alloc */
    { .hash = 0x1daf9a69, .address = (uint32_t)furi_thread_set_callback }, /* furi_thread_set_callback */
    { .hash = 0x1e84d649, .address = (uint32_t)mf_classic_set_block_read }, /* mf_classic_set_block_read */
    { .hash = 0x1f2c5a82, .address = (uint32_t)nfc_set_fdt_poll_fc }, /* nfc_set_fdt_poll_fc */
    { .hash = 0x1fba7776, .address = (uint32_t)submenu_change_item_label }, /* submenu_change_item_label */
    { .hash = 0x1fdf4f77, .address = (uint32_t)&I_WarningDolphin_45x42 }, /* I_WarningDolphin_45x42 */
    { .hash = 0x203b3559, .address = (uint32_t)furi_string_start_with }, /* furi_string_start_with */
    { .hash = 0x207c260c, .address = (uint32_t)notification_internal_message }, /* notification_internal_message */
    { .hash = 0x209b1679, .address = (uint32_t)canvas_get_font_params }, /* canvas_get_font_params */
    { .hash = 0x20cbcf85, .address = (uint32_t)scene_manager_search_and_switch_to_previous_scene_one_of }, /* scene_manager_search_and_switch_to_previous_scene_one_of */
    { .hash = 0x217d2851, .address = (uint32_t)version_get_githash }, /* version_get_githash */
    { .hash = 0x218d5b0a, .address = (uint32_t)furi_event_loop_tick_set }, /* furi_event_loop_tick_set */
    { .hash = 0x21ce0da1, .address = (uint32_t)&message_display_backlight_off }, /* message_display_backlight_off */
    { .hash = 0x226cd7ee, .address = (uint32_t)&message_do_not_reset }, /* message_do_not_reset */
    { .hash = 0x2278387c, .address = (uint32_t)buffered_file_stream_alloc }, /* buffered_file_stream_alloc */
    { .hash = 0x2291d3c8, .address = (uint32_t)subghz_protocol_blocks_add_to_128_bit }, /* subghz_protocol_blocks_add_to_128_bit */
    { .hash = 0x229c76c7, .address = (uint32_t)buffered_file_stream_close }, /* buffered_file_stream_close */
    { .hash = 0x229d01d5, .address = (uint32_t)cli_sleep }, /* cli_sleep */
    { .hash = 0x2314be30, .address = (uint32_t)furi_hal_gpio_init }, /* furi_hal_gpio_init */
    { .hash = 0x233cf996, .address = (uint32_t)furi_hal_crypto_enclave_unload_key }, /* furi_hal_crypto_enclave_unload_key */
    { .hash = 0x234101f6, .address = (uint32_t)submenu_add_item_ex }, /* submenu_add_item_ex */
    { .hash = 0x238a17a2, .address = (uint32_t)file_browser_worker_free }, /* file_browser_worker_free */
    { .hash = 0x238d54a0, .address = (uint32_t)file_browser_worker_load }, /* file_browser_worker_load */
    { .hash = 0x23b27c27, .address = (uint32_t)bit_buffer_append_byte }, /* bit_buffer_append_byte */
    { .hash = 0x23d393d7, .address = (uint32_t)storage_dir_exists }, /* storage_dir_exists */
    { .hash = 0x244617b6, .address = (uint32_t)submenu_add_lockable_item }, /* submenu_add_lockable_item */
    { .hash = 0x245caa0e, .address = (uint32_t)subghz_block_generic_deserialize }, /* subghz_block_generic_deserialize */
    { .hash = 0x24a9aef1, .address = (uint32_t)canvas_current_font_height }, /* canvas_current_font_height */
    { .hash = 0x24c4efe5, .address = (uint32_t)popup_enable_timeout }, /* popup_enable_timeout */
    { .hash = 0x256bd6ca, .address = (uint32_t)heap_caps_free }, /* heap_caps_free */
    { .hash = 0x25ca24fe, .address = (uint32_t)flipper_format_read_hex }, /* flipper_format_read_hex */
    { .hash = 0x25dec19e, .address = (uint32_t)furi_string_search_str }, /* furi_string_search_str */
    { .hash = 0x25f7575f, .address = (uint32_t)ble_profile_serial_set_event_callback }, /* ble_profile_serial_set_event_callback */
    { .hash = 0x26168cc1, .address = (uint32_t)nfc_poller_detect }, /* nfc_poller_detect */
    { .hash = 0x2649f22e, .address = (uint32_t)furi_mutex_free }, /* furi_mutex_free */
    { .hash = 0x2688dd53, .address = (uint32_t)furi_delay_tick }, /* furi_delay_tick */
    { .hash = 0x26aafac2, .address = (uint32_t)&g_wifi_osi_funcs }, /* g_wifi_osi_funcs */
    { .hash = 0x26c9c382, .address = (uint32_t)gui_add_framebuffer_callback }, /* gui_add_framebuffer_callback */
    { .hash = 0x26d06420, .address = (uint32_t)furi_string_replace_at }, /* furi_string_replace_at */
    { .hash = 0x26d193ca, .address = (uint32_t)dialog_ex_get_view }, /* dialog_ex_get_view */
    { .hash = 0x26f1264d, .address = (uint32_t)furi_message_queue_alloc }, /* furi_message_queue_alloc */
    { .hash = 0x2704b2a5, .address = (uint32_t)flipper_format_update_uint32 }, /* flipper_format_update_uint32 */
    { .hash = 0x272aec39, .address = (uint32_t)path_contains_only_ascii }, /* path_contains_only_ascii */
    { .hash = 0x278d99f6, .address = (uint32_t)heap_caps_calloc }, /* heap_caps_calloc */
    { .hash = 0x27a67566, .address = (uint32_t)__furi_halt_implementation }, /* __furi_halt_implementation */
    { .hash = 0x27b11240, .address = (uint32_t)mjs_mk_null }, /* mjs_mk_null */
    { .hash = 0x27b22639, .address = (uint32_t)furi_hal_speaker_release }, /* furi_hal_speaker_release */
    { .hash = 0x27b685e7, .address = (uint32_t)&ble_profile_serial }, /* ble_profile_serial */
    { .hash = 0x27dcd663, .address = (uint32_t)buffered_file_stream_open }, /* buffered_file_stream_open */
    { .hash = 0x27df2f4e, .address = (uint32_t)buffered_file_stream_sync }, /* buffered_file_stream_sync */
    { .hash = 0x282023ff, .address = (uint32_t)&message_red_255 }, /* message_red_255 */
    { .hash = 0x2849ceda, .address = (uint32_t)locale_format_date }, /* locale_format_date */
    { .hash = 0x28501fb7, .address = (uint32_t)infrared_worker_rx_stop }, /* infrared_worker_rx_stop */
    { .hash = 0x2852b60b, .address = (uint32_t)locale_format_time }, /* locale_format_time */
    { .hash = 0x2860ea96, .address = (uint32_t)dialog_message_show_storage_error }, /* dialog_message_show_storage_error */
    { .hash = 0x28721e1e, .address = (uint32_t)furi_timer_set_thread_priority }, /* furi_timer_set_thread_priority */
    { .hash = 0x292f3238, .address = (uint32_t)dialog_ex_set_right_button_text }, /* dialog_ex_set_right_button_text */
    { .hash = 0x2a5091ac, .address = (uint32_t)subghz_receiver_set_rx_callback }, /* subghz_receiver_set_rx_callback */
    { .hash = 0x2aaa426e, .address = (uint32_t)text_input_set_result_callback }, /* text_input_set_result_callback */
    { .hash = 0x2abe3c57, .address = (uint32_t)view_holder_attach_to_gui }, /* view_holder_attach_to_gui */
    { .hash = 0x2ad352a8, .address = (uint32_t)variable_item_list_set_enter_callback }, /* variable_item_list_set_enter_callback */
    { .hash = 0x2b03fe22, .address = (uint32_t)view_dispatcher_set_event_callback_context }, /* view_dispatcher_set_event_callback_context */
    { .hash = 0x2b16b9d1, .address = (uint32_t)memmgr_pool_get_max_block }, /* memmgr_pool_get_max_block */
    { .hash = 0x2b1cfae1, .address = (uint32_t)name_generator_make_auto }, /* name_generator_make_auto */
    { .hash = 0x2b64931f, .address = (uint32_t)&I_Unlock_7x8 }, /* I_Unlock_7x8 */
    { .hash = 0x2be2a572, .address = (uint32_t)flipper_format_get_raw_stream }, /* flipper_format_get_raw_stream */
    { .hash = 0x2be5cfff, .address = (uint32_t)cli_registry_alloc }, /* cli_registry_alloc */
    { .hash = 0x2c1c7c55, .address = (uint32_t)cli_shell_line_get_line_position }, /* cli_shell_line_get_line_position */
    { .hash = 0x2c63ca4c, .address = (uint32_t)&I_Nfc_10px }, /* I_Nfc_10px */
    { .hash = 0x2c940f5a, .address = (uint32_t)text_input_free }, /* text_input_free */
    { .hash = 0x2ce466b9, .address = (uint32_t)flipper_format_delete_key }, /* flipper_format_delete_key */
    { .hash = 0x2d1a0a6f, .address = (uint32_t)nfc_device_set_data }, /* nfc_device_set_data */
    { .hash = 0x2dc676a1, .address = (uint32_t)canvas_set_custom_u8g2_font }, /* canvas_set_custom_u8g2_font */
    { .hash = 0x2df5d98e, .address = (uint32_t)plugin_manager_load_single }, /* plugin_manager_load_single */
    { .hash = 0x2e30c31e, .address = (uint32_t)&I_settings }, /* I_settings */
    { .hash = 0x2e42fe69, .address = (uint32_t)furi_hal_power_is_otg_enabled }, /* furi_hal_power_is_otg_enabled */
    { .hash = 0x2e43532c, .address = (uint32_t)elements_frame }, /* elements_frame */
    { .hash = 0x2e73b6a8, .address = (uint32_t)popup_set_context }, /* popup_set_context */
    { .hash = 0x2f3a7d1d, .address = (uint32_t)cli_ansi_parser_alloc }, /* cli_ansi_parser_alloc */
    { .hash = 0x2fed55ff, .address = (uint32_t)view_holder_update }, /* view_holder_update */
    { .hash = 0x30416817, .address = (uint32_t)furi_hal_rtc_get_locale_units }, /* furi_hal_rtc_get_locale_units */
    { .hash = 0x30aa559e, .address = (uint32_t)furi_string_alloc_set_str }, /* furi_string_alloc_set_str */
    { .hash = 0x31116c24, .address = (uint32_t)dialog_ex_set_header }, /* dialog_ex_set_header */
    { .hash = 0x31230043, .address = (uint32_t)furi_string_alloc_vprintf }, /* furi_string_alloc_vprintf */
    { .hash = 0x3177bc39, .address = (uint32_t)scene_manager_get_scene_state }, /* scene_manager_get_scene_state */
    { .hash = 0x31c7dbd4, .address = (uint32_t)&gpio_nrf24_cs }, /* gpio_nrf24_cs */
    { .hash = 0x3253dbbf, .address = (uint32_t)infrared_worker_rx_start }, /* infrared_worker_rx_start */
    { .hash = 0x325b5bb6, .address = (uint32_t)&sequence_blink_start_magenta }, /* sequence_blink_start_magenta */
    { .hash = 0x32e39619, .address = (uint32_t)mf_classic_get_uid }, /* mf_classic_get_uid */
    { .hash = 0x32e52c9e, .address = (uint32_t)furi_thread_set_priority }, /* furi_thread_set_priority */
    { .hash = 0x334d01a9, .address = (uint32_t)storage_dir_open }, /* storage_dir_open */
    { .hash = 0x334e7773, .address = (uint32_t)storage_dir_read }, /* storage_dir_read */
    { .hash = 0x33532f09, .address = (uint32_t)furi_event_loop_run }, /* furi_event_loop_run */
    { .hash = 0x337e4ce1, .address = (uint32_t)elements_button_up }, /* elements_button_up */
    { .hash = 0x339ee8bd, .address = (uint32_t)mbedtls_sha1_free }, /* mbedtls_sha1_free */
    { .hash = 0x33a07d6f, .address = (uint32_t)mbedtls_sha1_init }, /* mbedtls_sha1_init */
    { .hash = 0x33d6fa21, .address = (uint32_t)furi_pubsub_publish }, /* furi_pubsub_publish */
    { .hash = 0x33ea609e, .address = (uint32_t)bt_set_status_changed_callback }, /* bt_set_status_changed_callback */
    { .hash = 0x34373d66, .address = (uint32_t)file_browser_worker_set_item_callback }, /* file_browser_worker_set_item_callback */
    { .hash = 0x34a48004, .address = (uint32_t)furi_get_tick }, /* furi_get_tick */
    { .hash = 0x34ae9bca, .address = (uint32_t)canvas_draw_bitmap }, /* canvas_draw_bitmap */
    { .hash = 0x35257655, .address = (uint32_t)&sequence_display_backlight_off }, /* sequence_display_backlight_off */
    { .hash = 0x358068e2, .address = (uint32_t)widget_add_string_multiline_element }, /* widget_add_string_multiline_element */
    { .hash = 0x358932d0, .address = (uint32_t)furi_log_print_format }, /* furi_log_print_format */
    { .hash = 0x3595bd57, .address = (uint32_t)furi_hal_power_disable_otg }, /* furi_hal_power_disable_otg */
    { .hash = 0x35c49b3f, .address = (uint32_t)i2s_del_channel }, /* i2s_del_channel */
    { .hash = 0x362665b7, .address = (uint32_t)button_panel_add_label }, /* button_panel_add_label */
    { .hash = 0x3654c449, .address = (uint32_t)view_port_is_enabled }, /* view_port_is_enabled */
    { .hash = 0x36912185, .address = (uint32_t)mjs_mk_boolean }, /* mjs_mk_boolean */
    { .hash = 0x36d439a9, .address = (uint32_t)byte_input_free }, /* byte_input_free */
    { .hash = 0x3702827f, .address = (uint32_t)canvas_draw_circle }, /* canvas_draw_circle */
    { .hash = 0x370bfdd3, .address = (uint32_t)text_box_free }, /* text_box_free */
    { .hash = 0x3753d1e3, .address = (uint32_t)&message_display_backlight_on }, /* message_display_backlight_on */
    { .hash = 0x37d1f510, .address = (uint32_t)manchester_encoder_reset }, /* manchester_encoder_reset */
    { .hash = 0x37e825d8, .address = (uint32_t)subghz_worker_set_overrun_callback }, /* subghz_worker_set_overrun_callback */
    { .hash = 0x3805a904, .address = (uint32_t)iso14443_4a_poller_send_block }, /* iso14443_4a_poller_send_block */
    { .hash = 0x38064b71, .address = (uint32_t)esp_restart }, /* esp_restart */
    { .hash = 0x39248831, .address = (uint32_t)nfc_config }, /* nfc_config */
    { .hash = 0x397b92e0, .address = (uint32_t)stream_save_to_file }, /* stream_save_to_file */
    { .hash = 0x398b9a8c, .address = (uint32_t)nfc_device_free }, /* nfc_device_free */
    { .hash = 0x398ed78a, .address = (uint32_t)nfc_device_load }, /* nfc_device_load */
    { .hash = 0x39927559, .address = (uint32_t)nfc_device_save }, /* nfc_device_save */
    { .hash = 0x39b40924, .address = (uint32_t)furi_hal_bt_extra_beacon_is_active }, /* furi_hal_bt_extra_beacon_is_active */
    { .hash = 0x39de934f, .address = (uint32_t)dialogs_app_process_module_message }, /* dialogs_app_process_module_message */
    { .hash = 0x39e05524, .address = (uint32_t)dialog_ex_set_icon }, /* dialog_ex_set_icon */
    { .hash = 0x39e66700, .address = (uint32_t)dialog_ex_set_text }, /* dialog_ex_set_text */
    { .hash = 0x39f5acd8, .address = (uint32_t)furi_hal_usb_set_config }, /* furi_hal_usb_set_config */
    { .hash = 0x3af87667, .address = (uint32_t)canvas_clear }, /* canvas_clear */
    { .hash = 0x3b0aa073, .address = (uint32_t)widget_alloc }, /* widget_alloc */
    { .hash = 0x3b2ba133, .address = (uint32_t)subghz_devices_get_by_name }, /* subghz_devices_get_by_name */
    { .hash = 0x3b5fe363, .address = (uint32_t)pipe_attach_to_event_loop }, /* pipe_attach_to_event_loop */
    { .hash = 0x3b73a123, .address = (uint32_t)&g_wifi_default_wpa_crypto_funcs }, /* g_wifi_default_wpa_crypto_funcs */
    { .hash = 0x3c044b63, .address = (uint32_t)canvas_reset }, /* canvas_reset */
    { .hash = 0x3c3a86eb, .address = (uint32_t)widget_reset }, /* widget_reset */
    { .hash = 0x3c60b980, .address = (uint32_t)canvas_width }, /* canvas_width */
    { .hash = 0x3ca1daa3, .address = (uint32_t)submenu_add_item_centered }, /* submenu_add_item_centered */
    { .hash = 0x3cc7fa55, .address = (uint32_t)&WIFI_EVENT }, /* WIFI_EVENT */
    { .hash = 0x3cc948b7, .address = (uint32_t)datetime_get_days_per_year }, /* datetime_get_days_per_year */
    { .hash = 0x3cd7f24c, .address = (uint32_t)notification_apply_ui_color }, /* notification_apply_ui_color */
    { .hash = 0x3ce1e7ed, .address = (uint32_t)file_browser_set_item_callback }, /* file_browser_set_item_callback */
    { .hash = 0x3cf8a3aa, .address = (uint32_t)popup_set_timeout }, /* popup_set_timeout */
    { .hash = 0x3d3f3fbd, .address = (uint32_t)furi_thread_set_name }, /* furi_thread_set_name */
    { .hash = 0x3d6756ba, .address = (uint32_t)file_browser_worker_is_in_start_folder }, /* file_browser_worker_is_in_start_folder */
    { .hash = 0x3d880a9f, .address = (uint32_t)furi_event_flag_alloc }, /* furi_event_flag_alloc */
    { .hash = 0x3d8f9026, .address = (uint32_t)subghz_worker_rx_callback }, /* subghz_worker_rx_callback */
    { .hash = 0x3dac1c1b, .address = (uint32_t)furi_event_flag_clear }, /* furi_event_flag_clear */
    { .hash = 0x3dc68bc3, .address = (uint32_t)scene_manager_handle_tick_event }, /* scene_manager_handle_tick_event */
    { .hash = 0x3dc9f1f6, .address = (uint32_t)furi_timer_is_running }, /* furi_timer_is_running */
    { .hash = 0x3de00ec7, .address = (uint32_t)realloc }, /* realloc */
    { .hash = 0x3e0dd541, .address = (uint32_t)night_shift_timer_stop }, /* night_shift_timer_stop */
    { .hash = 0x3e1255ee, .address = (uint32_t)__furi_crash_implementation }, /* __furi_crash_implementation */
    { .hash = 0x3e574980, .address = (uint32_t)mjs_mk_array_buf }, /* mjs_mk_array_buf */
    { .hash = 0x3e58ac61, .address = (uint32_t)keys_dict_get_total_keys }, /* keys_dict_get_total_keys */
    { .hash = 0x3e789d89, .address = (uint32_t)&sequence_blink_magenta_10 }, /* sequence_blink_magenta_10 */
    { .hash = 0x3ee13040, .address = (uint32_t)heap_caps_malloc }, /* heap_caps_malloc */
    { .hash = 0x3ef73dca, .address = (uint32_t)pipe_set_broken_callback }, /* pipe_set_broken_callback */
    { .hash = 0x3f1c02c8, .address = (uint32_t)esp_wifi_start }, /* esp_wifi_start */
    { .hash = 0x3f21a008, .address = (uint32_t)lock_screen_set_style }, /* lock_screen_set_style */
    { .hash = 0x407bcb9f, .address = (uint32_t)infrared_get_protocol_command_length }, /* infrared_get_protocol_command_length */
    { .hash = 0x40bf2cdb, .address = (uint32_t)stream_insert_string }, /* stream_insert_string */
    { .hash = 0x415599b6, .address = (uint32_t)number_input_get_view }, /* number_input_get_view */
    { .hash = 0x417e2099, .address = (uint32_t)mf_classic_is_block_read }, /* mf_classic_is_block_read */
    { .hash = 0x41b23ff7, .address = (uint32_t)furi_thread_enumerate }, /* furi_thread_enumerate */
    { .hash = 0x41cafc30, .address = (uint32_t)furi_event_loop_subscribe_event_flag }, /* furi_event_loop_subscribe_event_flag */
    { .hash = 0x41db7f3d, .address = (uint32_t)furi_stream_buffer_spaces_available }, /* furi_stream_buffer_spaces_available */
    { .hash = 0x41f47c9b, .address = (uint32_t)furi_hal_rfid_field_detect_start }, /* furi_hal_rfid_field_detect_start */
    { .hash = 0x41fb8b83, .address = (uint32_t)mbedtls_des_crypt_cbc }, /* mbedtls_des_crypt_cbc */
    { .hash = 0x41fb9425, .address = (uint32_t)mbedtls_des_crypt_ecb }, /* mbedtls_des_crypt_ecb */
    { .hash = 0x420dff73, .address = (uint32_t)mf_classic_poller_sync_auth }, /* mf_classic_poller_sync_auth */
    { .hash = 0x42170b5d, .address = (uint32_t)mf_classic_poller_sync_read }, /* mf_classic_poller_sync_read */
    { .hash = 0x42200e72, .address = (uint32_t)popup_get_view }, /* popup_get_view */
    { .hash = 0x42714df3, .address = (uint32_t)furi_hal_power_get_battery_full_capacity }, /* furi_hal_power_get_battery_full_capacity */
    { .hash = 0x4274f145, .address = (uint32_t)dialog_ex_set_left_button_text }, /* dialog_ex_set_left_button_text */
    { .hash = 0x427564a7, .address = (uint32_t)furi_log_tx }, /* furi_log_tx */
    { .hash = 0x4288e0a0, .address = (uint32_t)furi_hal_subghz_get_rssi }, /* furi_hal_subghz_get_rssi */
    { .hash = 0x42943d48, .address = (uint32_t)view_port_draw_callback_set }, /* view_port_draw_callback_set */
    { .hash = 0x42d0c164, .address = (uint32_t)view_set_draw_callback }, /* view_set_draw_callback */
    { .hash = 0x42d9075a, .address = (uint32_t)flipper_format_write_string_cstr }, /* flipper_format_write_string_cstr */
    { .hash = 0x43492935, .address = (uint32_t)esp_netif_init }, /* esp_netif_init */
    { .hash = 0x43aa7f43, .address = (uint32_t)furi_hal_spi_acquire }, /* furi_hal_spi_acquire */
    { .hash = 0x43e3a6c1, .address = (uint32_t)subghz_protocol_blocks_get_hash_data }, /* subghz_protocol_blocks_get_hash_data */
    { .hash = 0x43fd92cb, .address = (uint32_t)version_get_gitbranchnum }, /* version_get_gitbranchnum */
    { .hash = 0x44dfe27b, .address = (uint32_t)furi_stream_set_trigger_level }, /* furi_stream_set_trigger_level */
    { .hash = 0x44e30f66, .address = (uint32_t)flipper_format_set_strict_mode }, /* flipper_format_set_strict_mode */
    { .hash = 0x4505c17a, .address = (uint32_t)lwip_gethostbyname }, /* lwip_gethostbyname */
    { .hash = 0x4581e3a3, .address = (uint32_t)&usb_hid }, /* usb_hid */
    { .hash = 0x45aa855b, .address = (uint32_t)dialog_ex_alloc }, /* dialog_ex_alloc */
    { .hash = 0x45dd180c, .address = (uint32_t)submenu_settings_helpers_scene_exit }, /* submenu_settings_helpers_scene_exit */
    { .hash = 0x46453b24, .address = (uint32_t)heap_caps_get_largest_free_block }, /* heap_caps_get_largest_free_block */
    { .hash = 0x46da6bd3, .address = (uint32_t)dialog_ex_reset }, /* dialog_ex_reset */
    { .hash = 0x47b5738e, .address = (uint32_t)esp_wifi_init }, /* esp_wifi_init */
    { .hash = 0x47bb09a0, .address = (uint32_t)esp_wifi_stop }, /* esp_wifi_stop */
    { .hash = 0x485c7430, .address = (uint32_t)mf_desfire_get_file_data }, /* mf_desfire_get_file_data */
    { .hash = 0x48a33f15, .address = (uint32_t)mf_classic_is_sector_trailer }, /* mf_classic_is_sector_trailer */
    { .hash = 0x48f2aba2, .address = (uint32_t)canvas_set_font_direction }, /* canvas_set_font_direction */
    { .hash = 0x48f2d81c, .address = (uint32_t)furi_timer_free }, /* furi_timer_free */
    { .hash = 0x48fa02e0, .address = (uint32_t)furi_timer_stop }, /* furi_timer_stop */
    { .hash = 0x49506e3f, .address = (uint32_t)__floatsidf }, /* __floatsidf */
    { .hash = 0x498359b0, .address = (uint32_t)aligned_malloc }, /* aligned_malloc */
    { .hash = 0x49bcf04e, .address = (uint32_t)furi_thread_list_free }, /* furi_thread_list_free */
    { .hash = 0x49c3eda7, .address = (uint32_t)furi_thread_list_size }, /* furi_thread_list_size */
    { .hash = 0x49e0d1e3, .address = (uint32_t)furi_hal_nfc_field_is_present }, /* furi_hal_nfc_field_is_present */
    { .hash = 0x49ea8c5e, .address = (uint32_t)number_input_free }, /* number_input_free */
    { .hash = 0x4a04b64a, .address = (uint32_t)furi_thread_set_stdin_callback }, /* furi_thread_set_stdin_callback */
    { .hash = 0x4a3584e3, .address = (uint32_t)gui_view_port_send_to_back }, /* gui_view_port_send_to_back */
    { .hash = 0x4a721d93, .address = (uint32_t)furi_hal_bt_extra_beacon_stop }, /* furi_hal_bt_extra_beacon_stop */
    { .hash = 0x4a758c4b, .address = (uint32_t)cli_is_pipe_broken_or_is_etx_next_char }, /* cli_is_pipe_broken_or_is_etx_next_char */
    { .hash = 0x4a90b15b, .address = (uint32_t)cli_registry_add_command }, /* cli_registry_add_command */
    { .hash = 0x4a9f1ac9, .address = (uint32_t)file_browser_free }, /* file_browser_free */
    { .hash = 0x4aa6458d, .address = (uint32_t)file_browser_stop }, /* file_browser_stop */
    { .hash = 0x4ad1bc18, .address = (uint32_t)file_stream_get_error }, /* file_stream_get_error */
    { .hash = 0x4ae426b1, .address = (uint32_t)furi_hal_light_set }, /* furi_hal_light_set */
    { .hash = 0x4aee044b, .address = (uint32_t)&I_Modern_reader_18x34 }, /* I_Modern_reader_18x34 */
    { .hash = 0x4b40dd0f, .address = (uint32_t)furi_pubsub_unsubscribe }, /* furi_pubsub_unsubscribe */
    { .hash = 0x4b45a060, .address = (uint32_t)__assert_func }, /* __assert_func */
    { .hash = 0x4b64bd4d, .address = (uint32_t)view_dispatcher_remove_view }, /* view_dispatcher_remove_view */
    { .hash = 0x4ce7f80b, .address = (uint32_t)mf_classic_get_sector_by_block }, /* mf_classic_get_sector_by_block */
    { .hash = 0x4d09b22a, .address = (uint32_t)stream_eof }, /* stream_eof */
    { .hash = 0x4d484818, .address = (uint32_t)mjs_get_int }, /* mjs_get_int */
    { .hash = 0x4d4866a3, .address = (uint32_t)mjs_get_ptr }, /* mjs_get_ptr */
    { .hash = 0x4d869aff, .address = (uint32_t)button_menu_alloc }, /* button_menu_alloc */
    { .hash = 0x4d9f1502, .address = (uint32_t)validator_is_file_alloc_init }, /* validator_is_file_alloc_init */
    { .hash = 0x4db13b77, .address = (uint32_t)subghz_protocol_blocks_lfsr_digest8_reflect }, /* subghz_protocol_blocks_lfsr_digest8_reflect */
    { .hash = 0x4dc74dc9, .address = (uint32_t)furi_thread_set_state_callback }, /* furi_thread_set_state_callback */
    { .hash = 0x4df7f165, .address = (uint32_t)furi_hal_crypto_enclave_ensure_key }, /* furi_hal_crypto_enclave_ensure_key */
    { .hash = 0x4eb68177, .address = (uint32_t)button_menu_reset }, /* button_menu_reset */
    { .hash = 0x4ed36a43, .address = (uint32_t)&I_NFC_manual_60x50 }, /* I_NFC_manual_60x50 */
    { .hash = 0x4f31db1b, .address = (uint32_t)saved_struct_load }, /* saved_struct_load */
    { .hash = 0x4f3578ea, .address = (uint32_t)saved_struct_save }, /* saved_struct_save */
    { .hash = 0x4f791416, .address = (uint32_t)furi_mutex_acquire }, /* furi_mutex_acquire */
    { .hash = 0x507a84aa, .address = (uint32_t)infrared_worker_free }, /* infrared_worker_free */
    { .hash = 0x5080142f, .address = (uint32_t)bit_buffer_get_capacity_bytes }, /* bit_buffer_get_capacity_bytes */
    { .hash = 0x50b8922e, .address = (uint32_t)esp_netif_create_default_wifi_sta }, /* esp_netif_create_default_wifi_sta */
    { .hash = 0x5124cf69, .address = (uint32_t)scene_manager_handle_back_event }, /* scene_manager_handle_back_event */
    { .hash = 0x51288d7b, .address = (uint32_t)dialog_ex_enable_extended_events }, /* dialog_ex_enable_extended_events */
    { .hash = 0x51b6f4e3, .address = (uint32_t)canvas_draw_triangle }, /* canvas_draw_triangle */
    { .hash = 0x51dc1d30, .address = (uint32_t)canvas_draw_disc }, /* canvas_draw_disc */
    { .hash = 0x51dec116, .address = (uint32_t)canvas_draw_icon }, /* canvas_draw_icon */
    { .hash = 0x51e07f95, .address = (uint32_t)canvas_draw_line }, /* canvas_draw_line */
    { .hash = 0x51e3ac48, .address = (uint32_t)canvas_draw_rbox }, /* canvas_draw_rbox */
    { .hash = 0x51edd4f6, .address = (uint32_t)furi_event_loop_timer_free }, /* furi_event_loop_timer_free */
    { .hash = 0x51f4ffba, .address = (uint32_t)furi_event_loop_timer_stop }, /* furi_event_loop_timer_stop */
    { .hash = 0x52758c6a, .address = (uint32_t)bit_lib_get_bits }, /* bit_lib_get_bits */
    { .hash = 0x527be9e4, .address = (uint32_t)version_get_custom_name }, /* version_get_custom_name */
    { .hash = 0x52d6ca49, .address = (uint32_t)button_panel_get_view }, /* button_panel_get_view */
    { .hash = 0x52f1560e, .address = (uint32_t)furi_hal_random_get }, /* furi_hal_random_get */
    { .hash = 0x52f3adea, .address = (uint32_t)locale_get_time_format }, /* locale_get_time_format */
    { .hash = 0x535df0f0, .address = (uint32_t)&furi_hal_spi_bus_handle_external }, /* furi_hal_spi_bus_handle_external */
    { .hash = 0x538016fe, .address = (uint32_t)&message_delay_25 }, /* message_delay_25 */
    { .hash = 0x5380175c, .address = (uint32_t)&message_delay_50 }, /* message_delay_50 */
    { .hash = 0x53a28b7f, .address = (uint32_t)furi_event_loop_alloc }, /* furi_event_loop_alloc */
    { .hash = 0x54803eb8, .address = (uint32_t)view_commit_model }, /* view_commit_model */
    { .hash = 0x54c62e2e, .address = (uint32_t)stream_cache_at_end }, /* stream_cache_at_end */
    { .hash = 0x54fd5631, .address = (uint32_t)subghz_protocol_blocks_crc4 }, /* subghz_protocol_blocks_crc4 */
    { .hash = 0x54fd5635, .address = (uint32_t)subghz_protocol_blocks_crc8 }, /* subghz_protocol_blocks_crc8 */
    { .hash = 0x551a1e6d, .address = (uint32_t)float_is_equal }, /* float_is_equal */
    { .hash = 0x552ecfcc, .address = (uint32_t)popup_set_icon }, /* popup_set_icon */
    { .hash = 0x5534e1a8, .address = (uint32_t)popup_set_text }, /* popup_set_text */
    { .hash = 0x5564533f, .address = (uint32_t)manchester_encoder_advance }, /* manchester_encoder_advance */
    { .hash = 0x561f708b, .address = (uint32_t)nfc_device_get_uid }, /* nfc_device_get_uid */
    { .hash = 0x568eaf4c, .address = (uint32_t)flipper_format_write_header_cstr }, /* flipper_format_write_header_cstr */
    { .hash = 0x56f7cd00, .address = (uint32_t)button_panel_add_icon }, /* button_panel_add_icon */
    { .hash = 0x56f81406, .address = (uint32_t)button_panel_add_item }, /* button_panel_add_item */
    { .hash = 0x573571bf, .address = (uint32_t)file_browser_set_callback }, /* file_browser_set_callback */
    { .hash = 0x57749c72, .address = (uint32_t)dialog_ex_free }, /* dialog_ex_free */
    { .hash = 0x580f5621, .address = (uint32_t)view_dispatcher_send_to_back }, /* view_dispatcher_send_to_back */
    { .hash = 0x5838cdf7, .address = (uint32_t)submenu_set_header }, /* submenu_set_header */
    { .hash = 0x5841daf1, .address = (uint32_t)pipe_spaces_available }, /* pipe_spaces_available */
    { .hash = 0x5949547e, .address = (uint32_t)subghz_devices_begin }, /* subghz_devices_begin */
    { .hash = 0x59897e76, .address = (uint32_t)furi_mutex_get_owner }, /* furi_mutex_get_owner */
    { .hash = 0x599735ee, .address = (uint32_t)value_index_float }, /* value_index_float */
    { .hash = 0x59c47517, .address = (uint32_t)__fixunsdfdi }, /* __fixunsdfdi */
    { .hash = 0x59c47706, .address = (uint32_t)__fixunsdfsi }, /* __fixunsdfsi */
    { .hash = 0x59c9920a, .address = (uint32_t)canvas_draw_rframe }, /* canvas_draw_rframe */
    { .hash = 0x59cd36e3, .address = (uint32_t)furi_string_printf }, /* furi_string_printf */
    { .hash = 0x59cea748, .address = (uint32_t)value_index_int32 }, /* value_index_int32 */
    { .hash = 0x59e930cc, .address = (uint32_t)popup_set_header }, /* popup_set_header */
    { .hash = 0x59f662f1, .address = (uint32_t)furi_hal_rfid_field_is_present }, /* furi_hal_rfid_field_is_present */
    { .hash = 0x59f99921, .address = (uint32_t)widget_add_line_element }, /* widget_add_line_element */
    { .hash = 0x5a32bf67, .address = (uint32_t)mf_desfire_get_file_settings }, /* mf_desfire_get_file_settings */
    { .hash = 0x5a4c0299, .address = (uint32_t)view_dispatcher_send_to_front }, /* view_dispatcher_send_to_front */
    { .hash = 0x5a6b0f1c, .address = (uint32_t)subghz_devices_reset }, /* subghz_devices_reset */
    { .hash = 0x5a74b905, .address = (uint32_t)furi_hal_power_suppress_charge_enter }, /* furi_hal_power_suppress_charge_enter */
    { .hash = 0x5a80c2b2, .address = (uint32_t)subghz_devices_sleep }, /* subghz_devices_sleep */
    { .hash = 0x5ac304e4, .address = (uint32_t)furi_thread_flags_clear }, /* furi_thread_flags_clear */
    { .hash = 0x5bab36b9, .address = (uint32_t)variable_item_set_current_value_index }, /* variable_item_set_current_value_index */
    { .hash = 0x5bb44f8f, .address = (uint32_t)furi_hal_hid_kb_release }, /* furi_hal_hid_kb_release */
    { .hash = 0x5c5b842f, .address = (uint32_t)pipe_set_data_arrived_callback }, /* pipe_set_data_arrived_callback */
    { .hash = 0x5c69acee, .address = (uint32_t)icon_animation_free }, /* icon_animation_free */
    { .hash = 0x5c70d7b2, .address = (uint32_t)icon_animation_stop }, /* icon_animation_stop */
    { .hash = 0x5c78aa90, .address = (uint32_t)args_read_string_and_trim }, /* args_read_string_and_trim */
    { .hash = 0x5c8b27e3, .address = (uint32_t)dialog_message_alloc }, /* dialog_message_alloc */
    { .hash = 0x5cce8b34, .address = (uint32_t)furi_string_set_str }, /* furi_string_set_str */
    { .hash = 0x5cd47e70, .address = (uint32_t)version_get_target }, /* version_get_target */
    { .hash = 0x5cdc7ffb, .address = (uint32_t)furi_hal_subghz_set_frequency_and_path }, /* furi_hal_subghz_set_frequency_and_path */
    { .hash = 0x5d415ec6, .address = (uint32_t)&message_blink_start_10 }, /* message_blink_start_10 */
    { .hash = 0x5d4735d9, .address = (uint32_t)furi_string_vprintf }, /* furi_string_vprintf */
    { .hash = 0x5d5c8208, .address = (uint32_t)furi_delay_ms }, /* furi_delay_ms */
    { .hash = 0x5d5c8310, .address = (uint32_t)furi_delay_us }, /* furi_delay_us */
    { .hash = 0x5d7253b3, .address = (uint32_t)furi_thread_suspend }, /* furi_thread_suspend */
    { .hash = 0x5dd98f20, .address = (uint32_t)subghz_environment_load_keystore }, /* subghz_environment_load_keystore */
    { .hash = 0x5e9fe0fe, .address = (uint32_t)&message_blink_stop }, /* message_blink_stop */
    { .hash = 0x5f1f6d6d, .address = (uint32_t)variable_item_list_get_view }, /* variable_item_list_get_view */
    { .hash = 0x5f6d7493, .address = (uint32_t)infrared_worker_alloc }, /* infrared_worker_alloc */
    { .hash = 0x5fbe6b48, .address = (uint32_t)furi_stream_buffer_is_empty }, /* furi_stream_buffer_is_empty */
    { .hash = 0x5fdd1e06, .address = (uint32_t)furi_string_search }, /* furi_string_search */
    { .hash = 0x609d84a3, .address = (uint32_t)storage_file_alloc }, /* storage_file_alloc */
    { .hash = 0x60c1c2ee, .address = (uint32_t)storage_file_close }, /* storage_file_close */
    { .hash = 0x611aacaa, .address = (uint32_t)furi_thread_get_return_code }, /* furi_thread_get_return_code */
    { .hash = 0x611d38e0, .address = (uint32_t)text_input_set_minimum_length }, /* text_input_set_minimum_length */
    { .hash = 0x613f9d29, .address = (uint32_t)subghz_protocol_blocks_reverse_key }, /* subghz_protocol_blocks_reverse_key */
    { .hash = 0x6191415d, .address = (uint32_t)pipe_alloc }, /* pipe_alloc */
    { .hash = 0x61d29917, .address = (uint32_t)cli_registry_add_command_ex }, /* cli_registry_add_command_ex */
    { .hash = 0x622edde3, .address = (uint32_t)storage_file_write }, /* storage_file_write */
    { .hash = 0x628361f7, .address = (uint32_t)bit_lib_bytes_to_num_be }, /* bit_lib_bytes_to_num_be */
    { .hash = 0x62836341, .address = (uint32_t)bit_lib_bytes_to_num_le }, /* bit_lib_bytes_to_num_le */
    { .hash = 0x62a8f770, .address = (uint32_t)furi_string_cat_vprintf }, /* furi_string_cat_vprintf */
    { .hash = 0x62d3490c, .address = (uint32_t)furi_record_create }, /* furi_record_create */
    { .hash = 0x62db2f53, .address = (uint32_t)pipe_state }, /* pipe_state */
    { .hash = 0x633d4e0b, .address = (uint32_t)furi_thread_set_stdout_callback }, /* furi_thread_set_stdout_callback */
    { .hash = 0x635c84fd, .address = (uint32_t)vTaskDelay }, /* vTaskDelay */
    { .hash = 0x63b18cb4, .address = (uint32_t)ble_profile_hid_kb_press }, /* ble_profile_hid_kb_press */
    { .hash = 0x645a1f35, .address = (uint32_t)menu_set_style }, /* menu_set_style */
    { .hash = 0x646b3aea, .address = (uint32_t)subghz_transmitter_yield }, /* subghz_transmitter_yield */
    { .hash = 0x648f76da, .address = (uint32_t)furi_hal_spi_release }, /* furi_hal_spi_release */
    { .hash = 0x64bae9df, .address = (uint32_t)nfc_device_set_loading_callback }, /* nfc_device_set_loading_callback */
    { .hash = 0x64ec0f08, .address = (uint32_t)path_extract_extension }, /* path_extract_extension */
    { .hash = 0x64f8327f, .address = (uint32_t)flipper_format_file_open_always }, /* flipper_format_file_open_always */
    { .hash = 0x653ccd66, .address = (uint32_t)flipper_format_file_open_append }, /* flipper_format_file_open_append */
    { .hash = 0x65a3753d, .address = (uint32_t)bit_buffer_copy_bytes }, /* bit_buffer_copy_bytes */
    { .hash = 0x65cfd524, .address = (uint32_t)esp_wifi_disconnect }, /* esp_wifi_disconnect */
    { .hash = 0x66236b4e, .address = (uint32_t)view_port_get_orientation }, /* view_port_get_orientation */
    { .hash = 0x662589fc, .address = (uint32_t)__lshrdi3 }, /* __lshrdi3 */
    { .hash = 0x665323b8, .address = (uint32_t)mjs_destroy }, /* mjs_destroy */
    { .hash = 0x667f2ddb, .address = (uint32_t)furi_string_alloc }, /* furi_string_alloc */
    { .hash = 0x66bc0054, .address = (uint32_t)bit_buffer_copy_right }, /* bit_buffer_copy_right */
    { .hash = 0x66bc73ce, .address = (uint32_t)infrared_worker_get_raw_signal }, /* infrared_worker_get_raw_signal */
    { .hash = 0x66c82dff, .address = (uint32_t)furi_string_empty }, /* furi_string_empty */
    { .hash = 0x66ca7248, .address = (uint32_t)furi_string_equal }, /* furi_string_equal */
    { .hash = 0x66f03645, .address = (uint32_t)furi_timer_alloc }, /* furi_timer_alloc */
    { .hash = 0x6716ccec, .address = (uint32_t)furi_hal_speaker_init }, /* furi_hal_speaker_init */
    { .hash = 0x67280fac, .address = (uint32_t)furi_thread_set_stack_size }, /* furi_thread_set_stack_size */
    { .hash = 0x6728399c, .address = (uint32_t)subghz_receiver_set_filter }, /* subghz_receiver_set_filter */
    { .hash = 0x67287095, .address = (uint32_t)keys_dict_check_presence }, /* keys_dict_check_presence */
    { .hash = 0x674ad79c, .address = (uint32_t)furi_timer_flush }, /* furi_timer_flush */
    { .hash = 0x677d5c70, .address = (uint32_t)canvas_draw_xbm_ex }, /* canvas_draw_xbm_ex */
    { .hash = 0x678ee68d, .address = (uint32_t)loading_alloc }, /* loading_alloc */
    { .hash = 0x67af1453, .address = (uint32_t)furi_string_reset }, /* furi_string_reset */
    { .hash = 0x67b1132e, .address = (uint32_t)furi_string_right }, /* furi_string_right */
    { .hash = 0x67c13049, .address = (uint32_t)furi_string_set_n }, /* furi_string_set_n */
    { .hash = 0x67ca41d9, .address = (uint32_t)pipe_alloc_ex }, /* pipe_alloc_ex */
    { .hash = 0x67ecab78, .address = (uint32_t)furi_record_exists }, /* furi_record_exists */
    { .hash = 0x67edadac, .address = (uint32_t)cli_shell_line_ensure_not_overwriting_history }, /* cli_shell_line_ensure_not_overwriting_history */
    { .hash = 0x683a2408, .address = (uint32_t)furi_timer_start }, /* furi_timer_start */
    { .hash = 0x685bf492, .address = (uint32_t)mjs_is_number }, /* mjs_is_number */
    { .hash = 0x6893753b, .address = (uint32_t)version_get_gitbranch }, /* version_get_gitbranch */
    { .hash = 0x689b9fa6, .address = (uint32_t)view_set_input_callback }, /* view_set_input_callback */
    { .hash = 0x69336a25, .address = (uint32_t)bt_keys_storage_set_default_path }, /* bt_keys_storage_set_default_path */
    { .hash = 0x6943a53b, .address = (uint32_t)cli_shell_line_set_about_to_exit }, /* cli_shell_line_set_about_to_exit */
    { .hash = 0x6957b300, .address = (uint32_t)mjs_is_object }, /* mjs_is_object */
    { .hash = 0x698e095e, .address = (uint32_t)datetime_is_leap_year }, /* datetime_is_leap_year */
    { .hash = 0x698eebaa, .address = (uint32_t)flipper_format_buffered_file_alloc }, /* flipper_format_buffered_file_alloc */
    { .hash = 0x69996716, .address = (uint32_t)elements_scrollbar_pos }, /* elements_scrollbar_pos */
    { .hash = 0x69b329f5, .address = (uint32_t)flipper_format_buffered_file_close }, /* flipper_format_buffered_file_close */
    { .hash = 0x6aa146b5, .address = (uint32_t)nfc_device_alloc }, /* nfc_device_alloc */
    { .hash = 0x6aa6f432, .address = (uint32_t)subghz_setting_get_hopper_frequency }, /* subghz_setting_get_hopper_frequency */
    { .hash = 0x6aac992f, .address = (uint32_t)mjs_mk_foreign }, /* mjs_mk_foreign */
    { .hash = 0x6ac55831, .address = (uint32_t)nfc_device_clear }, /* nfc_device_clear */
    { .hash = 0x6aebb617, .address = (uint32_t)bit_buffer_copy }, /* bit_buffer_copy */
    { .hash = 0x6aed667e, .address = (uint32_t)bit_buffer_free }, /* bit_buffer_free */
    { .hash = 0x6af67b59, .address = (uint32_t)view_port_enabled_set }, /* view_port_enabled_set */
    { .hash = 0x6ba38690, .address = (uint32_t)flipper_application_preload }, /* flipper_application_preload */
    { .hash = 0x6bc3eb93, .address = (uint32_t)furi_thread_get_current }, /* furi_thread_get_current */
    { .hash = 0x6bfae64e, .address = (uint32_t)xTaskCreatePinnedToCore }, /* xTaskCreatePinnedToCore */
    { .hash = 0x6c14fe13, .address = (uint32_t)menu_get_view }, /* menu_get_view */
    { .hash = 0x6c22c47c, .address = (uint32_t)md5_calc_file }, /* md5_calc_file */
    { .hash = 0x6c418904, .address = (uint32_t)subghz_protocol_blocks_parity_bytes }, /* subghz_protocol_blocks_parity_bytes */
    { .hash = 0x6c831a1c, .address = (uint32_t)furi_string_replace_all_str }, /* furi_string_replace_all_str */
    { .hash = 0x6c8c16b9, .address = (uint32_t)file_browser_worker_folder_enter }, /* file_browser_worker_folder_enter */
    { .hash = 0x6c93a048, .address = (uint32_t)view_dispatcher_set_tick_event_callback }, /* view_dispatcher_set_tick_event_callback */
    { .hash = 0x6cbb124e, .address = (uint32_t)furi_hal_spi_bus_unlock }, /* furi_hal_spi_bus_unlock */
    { .hash = 0x6cbc8ad4, .address = (uint32_t)i2s_new_channel }, /* i2s_new_channel */
    { .hash = 0x6ce9a9db, .address = (uint32_t)esp_wifi_set_config }, /* esp_wifi_set_config */
    { .hash = 0x6e0832ab, .address = (uint32_t)&firmware_api_interface }, /* firmware_api_interface */
    { .hash = 0x6e23aaf7, .address = (uint32_t)mf_classic_is_key_found }, /* mf_classic_is_key_found */
    { .hash = 0x6e5c87f2, .address = (uint32_t)memmgr_heap_enable_thread_trace }, /* memmgr_heap_enable_thread_trace */
    { .hash = 0x6e6ef41e, .address = (uint32_t)furi_thread_get_appid }, /* furi_thread_get_appid */
    { .hash = 0x6e8609dc, .address = (uint32_t)variable_item_get_context }, /* variable_item_get_context */
    { .hash = 0x6eca7ff3, .address = (uint32_t)file_browser_worker_set_folder_callback }, /* file_browser_worker_set_folder_callback */
    { .hash = 0x6f2176a9, .address = (uint32_t)subghz_protocol_registry_count }, /* subghz_protocol_registry_count */
    { .hash = 0x6f2c6a94, .address = (uint32_t)dir_walk_set_recursive }, /* dir_walk_set_recursive */
    { .hash = 0x6f3b3833, .address = (uint32_t)file_browser_worker_set_list_callback }, /* file_browser_worker_set_list_callback */
    { .hash = 0x6f660792, .address = (uint32_t)furi_thread_flags_wait }, /* furi_thread_flags_wait */
    { .hash = 0x6f92e034, .address = (uint32_t)pipe_free }, /* pipe_free */
    { .hash = 0x6f99cafc, .address = (uint32_t)pipe_send }, /* pipe_send */
    { .hash = 0x6fa1dfa5, .address = (uint32_t)cli_shell_line_alloc }, /* cli_shell_line_alloc */
    { .hash = 0x6fb6a051, .address = (uint32_t)furi_thread_get_state }, /* furi_thread_get_state */
    { .hash = 0x703647d0, .address = (uint32_t)menu_add_item }, /* menu_add_item */
    { .hash = 0x705e0bad, .address = (uint32_t)furi_mutex_release }, /* furi_mutex_release */
    { .hash = 0x70abfb15, .address = (uint32_t)cli_registry_remove_external_commands }, /* cli_registry_remove_external_commands */
    { .hash = 0x70b6c701, .address = (uint32_t)mjs_array_del }, /* mjs_array_del */
    { .hash = 0x70b6d3cc, .address = (uint32_t)mjs_array_get }, /* mjs_array_get */
    { .hash = 0x70b706d8, .address = (uint32_t)mjs_array_set }, /* mjs_array_set */
    { .hash = 0x70d14a7a, .address = (uint32_t)infrared_get_protocol_by_name }, /* infrared_get_protocol_by_name */
    { .hash = 0x70f566a4, .address = (uint32_t)args_read_int_and_trim }, /* args_read_int_and_trim */
    { .hash = 0x712a4c7c, .address = (uint32_t)file_browser_worker_set_callback_context }, /* file_browser_worker_set_callback_context */
    { .hash = 0x719b2466, .address = (uint32_t)infrared_get_protocol_address_length }, /* infrared_get_protocol_address_length */
    { .hash = 0x72642115, .address = (uint32_t)furi_event_loop_is_subscribed }, /* furi_event_loop_is_subscribed */
    { .hash = 0x72e153c0, .address = (uint32_t)flipper_format_stream_delete_key_and_write }, /* flipper_format_stream_delete_key_and_write */
    { .hash = 0x730292a0, .address = (uint32_t)submenu_set_header_centered }, /* submenu_set_header_centered */
    { .hash = 0x733df8e9, .address = (uint32_t)furi_hal_rtc_get_timestamp }, /* furi_hal_rtc_get_timestamp */
    { .hash = 0x73b3d3aa, .address = (uint32_t)view_port_input_callback_set }, /* view_port_input_callback_set */
    { .hash = 0x73cc2a0a, .address = (uint32_t)furi_hal_spi_bus_handle_deinit }, /* furi_hal_spi_bus_handle_deinit */
    { .hash = 0x73dcb981, .address = (uint32_t)__getreent }, /* __getreent */
    { .hash = 0x73f68400, .address = (uint32_t)mjs_is_string }, /* mjs_is_string */
    { .hash = 0x7434bbb5, .address = (uint32_t)widget_add_button_element }, /* widget_add_button_element */
    { .hash = 0x7461930a, .address = (uint32_t)furi_message_queue_get_count }, /* furi_message_queue_get_count */
    { .hash = 0x74b4eb1a, .address = (uint32_t)furi_string_end_with_str }, /* furi_string_end_with_str */
    { .hash = 0x74d7f6cb, .address = (uint32_t)furi_hal_subghz_flush_rx }, /* furi_hal_subghz_flush_rx */
    { .hash = 0x74fd7d2c, .address = (uint32_t)simple_array_get }, /* simple_array_get */
    { .hash = 0x758350ed, .address = (uint32_t)furi_message_queue_get_space }, /* furi_message_queue_get_space */
    { .hash = 0x763a8748, .address = (uint32_t)bit_buffer_set_size_bytes }, /* bit_buffer_set_size_bytes */
    { .hash = 0x76939052, .address = (uint32_t)furi_log_get_level }, /* furi_log_get_level */
    { .hash = 0x76b92106, .address = (uint32_t)view_port_update }, /* view_port_update */
    { .hash = 0x771c6580, .address = (uint32_t)mf_classic_set_sector_trailer_read }, /* mf_classic_set_sector_trailer_read */
    { .hash = 0x77202198, .address = (uint32_t)elements_multiline_text }, /* elements_multiline_text */
    { .hash = 0x776c253a, .address = (uint32_t)furi_hal_power_get_battery_remaining_capacity }, /* furi_hal_power_get_battery_remaining_capacity */
    { .hash = 0x77853788, .address = (uint32_t)flipper_format_stream_read_value_line }, /* flipper_format_stream_read_value_line */
    { .hash = 0x78aab280, .address = (uint32_t)furi_stream_buffer_free }, /* furi_stream_buffer_free */
    { .hash = 0x78b19d48, .address = (uint32_t)furi_stream_buffer_send }, /* furi_stream_buffer_send */
    { .hash = 0x792d988c, .address = (uint32_t)canvas_set_bitmap_mode }, /* canvas_set_bitmap_mode */
    { .hash = 0x795bf537, .address = (uint32_t)mjs_mk_undefined }, /* mjs_mk_undefined */
    { .hash = 0x79807734, .address = (uint32_t)flipper_application_alloc }, /* flipper_application_alloc */
    { .hash = 0x799a5e69, .address = (uint32_t)__errno }, /* __errno */
    { .hash = 0x79b731ab, .address = (uint32_t)__gedf2 }, /* __gedf2 */
    { .hash = 0x79c45736, .address = (uint32_t)subghz_environment_alloc }, /* subghz_environment_alloc */
    { .hash = 0x7a19e5df, .address = (uint32_t)__ltdf2 }, /* __ltdf2 */
    { .hash = 0x7a3f8f9d, .address = (uint32_t)&sequence_blink_green_10 }, /* sequence_blink_green_10 */
    { .hash = 0x7a9fec44, .address = (uint32_t)menu_alloc }, /* menu_alloc */
    { .hash = 0x7ac1a9a0, .address = (uint32_t)subghz_environment_set_protocol_registry }, /* subghz_environment_set_protocol_registry */
    { .hash = 0x7ae86aec, .address = (uint32_t)empty_screen_get_view }, /* empty_screen_get_view */
    { .hash = 0x7b166383, .address = (uint32_t)stream_delete }, /* stream_delete */
    { .hash = 0x7b87946d, .address = (uint32_t)flipper_format_stream_get_value_count }, /* flipper_format_stream_get_value_count */
    { .hash = 0x7bb104b5, .address = (uint32_t)furi_hal_speaker_deinit }, /* furi_hal_speaker_deinit */
    { .hash = 0x7bcfd2bc, .address = (uint32_t)menu_reset }, /* menu_reset */
    { .hash = 0x7bf69165, .address = (uint32_t)view_port_free }, /* view_port_free */
    { .hash = 0x7c1de24c, .address = (uint32_t)flipper_format_update_bool }, /* flipper_format_update_bool */
    { .hash = 0x7c394390, .address = (uint32_t)bit_buffer_copy_bytes_with_parity }, /* bit_buffer_copy_bytes_with_parity */
    { .hash = 0x7c943aa9, .address = (uint32_t)atan }, /* atan */
    { .hash = 0x7c943c6f, .address = (uint32_t)atof }, /* atof */
    { .hash = 0x7c943c72, .address = (uint32_t)atoi }, /* atoi */
    { .hash = 0x7c954070, .address = (uint32_t)cosf }, /* cosf */
    { .hash = 0x7c967e3f, .address = (uint32_t)exit }, /* exit */
    { .hash = 0x7c96a7e1, .address = (uint32_t)fabs }, /* fabs */
    { .hash = 0x7c96f087, .address = (uint32_t)free }, /* free */
    { .hash = 0x7c99f227, .address = (uint32_t)labs }, /* labs */
    { .hash = 0x7c9c7b01, .address = (uint32_t)putc }, /* putc */
    { .hash = 0x7c9c7b11, .address = (uint32_t)puts }, /* puts */
    { .hash = 0x7c9d3dea, .address = (uint32_t)rand }, /* rand */
    { .hash = 0x7c9dec55, .address = (uint32_t)sinf }, /* sinf */
    { .hash = 0x7cc35348, .address = (uint32_t)mf_ultralight_get_feature_support_set }, /* mf_ultralight_get_feature_support_set */
    { .hash = 0x7ce3ab85, .address = (uint32_t)furi_stream_buffer_bytes_available }, /* furi_stream_buffer_bytes_available */
    { .hash = 0x7d0b76ee, .address = (uint32_t)button_menu_get_view }, /* button_menu_get_view */
    { .hash = 0x7d429ba6, .address = (uint32_t)widget_add_text_box_element }, /* widget_add_text_box_element */
    { .hash = 0x7d458759, .address = (uint32_t)furi_ms_to_ticks }, /* furi_ms_to_ticks */
    { .hash = 0x7db819a6, .address = (uint32_t)elements_multiline_text_framed }, /* elements_multiline_text_framed */
    { .hash = 0x7e00f017, .address = (uint32_t)memmgr_heap_printf_free_blocks }, /* memmgr_heap_printf_free_blocks */
    { .hash = 0x7e6dd22f, .address = (uint32_t)bit_buffer_get_byte }, /* bit_buffer_get_byte */
    { .hash = 0x7e6e84d5, .address = (uint32_t)bit_buffer_get_data }, /* bit_buffer_get_data */
    { .hash = 0x7e76e156, .address = (uint32_t)bit_buffer_get_size }, /* bit_buffer_get_size */
    { .hash = 0x7e7a5018, .address = (uint32_t)storage_file_exists }, /* storage_file_exists */
    { .hash = 0x7e99681e, .address = (uint32_t)&I_Pin_back_arrow_10x8 }, /* I_Pin_back_arrow_10x8 */
    { .hash = 0x7ea6a62f, .address = (uint32_t)furi_hal_version_uid_size }, /* furi_hal_version_uid_size */
    { .hash = 0x7f30002b, .address = (uint32_t)nfc_listener_alloc }, /* nfc_listener_alloc */
    { .hash = 0x7f3f8c86, .address = (uint32_t)furi_log_remove_handler }, /* furi_log_remove_handler */
    { .hash = 0x7fa3b874, .address = (uint32_t)number_input_set_header_text }, /* number_input_set_header_text */
    { .hash = 0x7fc62622, .address = (uint32_t)view_port_get_width }, /* view_port_get_width */
    { .hash = 0x8079edee, .address = (uint32_t)nfc_listener_start }, /* nfc_listener_start */
    { .hash = 0x80fd54b7, .address = (uint32_t)furi_thread_list_alloc }, /* furi_thread_list_alloc */
    { .hash = 0x812cc0ab, .address = (uint32_t)button_menu_add_item }, /* button_menu_add_item */
    { .hash = 0x815c5496, .address = (uint32_t)subghz_protocol_blocks_parity8 }, /* subghz_protocol_blocks_parity8 */
    { .hash = 0x817434ba, .address = (uint32_t)pipe_set_space_freed_callback }, /* pipe_set_space_freed_callback */
    { .hash = 0x820e6bf0, .address = (uint32_t)__fixdfsi }, /* __fixdfsi */
    { .hash = 0x82b968c6, .address = (uint32_t)nfc_scanner_free }, /* nfc_scanner_free */
    { .hash = 0x82c0938a, .address = (uint32_t)nfc_scanner_stop }, /* nfc_scanner_stop */
    { .hash = 0x82d80748, .address = (uint32_t)simple_array_cget_data }, /* simple_array_cget_data */
    { .hash = 0x834fb0a3, .address = (uint32_t)esp_bt_controller_mem_release }, /* esp_bt_controller_mem_release */
    { .hash = 0x837d8bda, .address = (uint32_t)gui_direct_draw_acquire }, /* gui_direct_draw_acquire */
    { .hash = 0x83838160, .address = (uint32_t)variable_item_list_get_selected_item_index }, /* variable_item_list_get_selected_item_index */
    { .hash = 0x83d1579a, .address = (uint32_t)view_dispatcher_run }, /* view_dispatcher_run */
    { .hash = 0x83d61ca0, .address = (uint32_t)furi_string_cat_str }, /* furi_string_cat_str */
    { .hash = 0x84023d68, .address = (uint32_t)_ctype_ }, /* _ctype_ */
    { .hash = 0x8407dc7e, .address = (uint32_t)view_set_input_mode }, /* view_set_input_mode */
    { .hash = 0x8426fee9, .address = (uint32_t)&message_blink_set_color_blue }, /* message_blink_set_color_blue */
    { .hash = 0x8427c00c, .address = (uint32_t)&message_blink_set_color_cyan }, /* message_blink_set_color_cyan */
    { .hash = 0x8470ad48, .address = (uint32_t)infrared_worker_signal_is_decoded }, /* infrared_worker_signal_is_decoded */
    { .hash = 0x847e9302, .address = (uint32_t)esp_wifi_set_max_tx_power }, /* esp_wifi_set_max_tx_power */
    { .hash = 0x84b51bec, .address = (uint32_t)view_dispatcher_alloc_ex }, /* view_dispatcher_alloc_ex */
    { .hash = 0x84c93294, .address = (uint32_t)empty_screen_free }, /* empty_screen_free */
    { .hash = 0x84de816c, .address = (uint32_t)widget_add_text_scroll_element }, /* widget_add_text_scroll_element */
    { .hash = 0x8511e724, .address = (uint32_t)furi_message_queue_free }, /* furi_message_queue_free */
    { .hash = 0x8525eee3, .address = (uint32_t)notification_apply_led_color }, /* notification_apply_led_color */
    { .hash = 0x855360b9, .address = (uint32_t)pipe_bytes_available }, /* pipe_bytes_available */
    { .hash = 0x8569699c, .address = (uint32_t)furi_log_level_from_string }, /* furi_log_level_from_string */
    { .hash = 0x8593b6f2, .address = (uint32_t)number_input_set_result_callback }, /* number_input_set_result_callback */
    { .hash = 0x85aa1a2b, .address = (uint32_t)locale_celsius_to_fahrenheit }, /* locale_celsius_to_fahrenheit */
    { .hash = 0x85b3004e, .address = (uint32_t)subghz_setting_get_inx_preset_by_name }, /* subghz_setting_get_inx_preset_by_name */
    { .hash = 0x85f4ecef, .address = (uint32_t)esp_rom_delay_us }, /* esp_rom_delay_us */
    { .hash = 0x8658fbb8, .address = (uint32_t)&I_Message_8x7 }, /* I_Message_8x7 */
    { .hash = 0x866e720e, .address = (uint32_t)stream_cache_alloc }, /* stream_cache_alloc */
    { .hash = 0x86914487, .address = (uint32_t)gui_get_framebuffer_size }, /* gui_get_framebuffer_size */
    { .hash = 0x86ac0d27, .address = (uint32_t)furi_hal_gpio_write }, /* furi_hal_gpio_write */
    { .hash = 0x86c91365, .address = (uint32_t)stream_cache_flush }, /* stream_cache_flush */
    { .hash = 0x86de72c7, .address = (uint32_t)number_input_alloc }, /* number_input_alloc */
    { .hash = 0x870064dd, .address = (uint32_t)submenu_get_view }, /* submenu_get_view */
    { .hash = 0x871f6356, .address = (uint32_t)subghz_devices_deinit }, /* subghz_devices_deinit */
    { .hash = 0x873d52ca, .address = (uint32_t)flipper_format_insert_or_update_float }, /* flipper_format_insert_or_update_float */
    { .hash = 0x8742c103, .address = (uint32_t)popup_alloc }, /* popup_alloc */
    { .hash = 0x8749677c, .address = (uint32_t)cli_shell_line_prompt }, /* cli_shell_line_prompt */
    { .hash = 0x8755c6d0, .address = (uint32_t)bit_lib_get_bits_16 }, /* bit_lib_get_bits_16 */
    { .hash = 0x8755c70e, .address = (uint32_t)bit_lib_get_bits_32 }, /* bit_lib_get_bits_32 */
    { .hash = 0x8755c773, .address = (uint32_t)bit_lib_get_bits_64 }, /* bit_lib_get_bits_64 */
    { .hash = 0x8766e1a5, .address = (uint32_t)stream_insert }, /* stream_insert */
    { .hash = 0x8796810c, .address = (uint32_t)mjs_array_push }, /* mjs_array_push */
    { .hash = 0x87d816f1, .address = (uint32_t)mjs_strerror }, /* mjs_strerror */
    { .hash = 0x87e3276a, .address = (uint32_t)flipper_init }, /* flipper_init */
    { .hash = 0x87ffcb4e, .address = (uint32_t)stream_cache_write }, /* stream_cache_write */
    { .hash = 0x88123330, .address = (uint32_t)lwip_recv }, /* lwip_recv */
    { .hash = 0x8812c0ea, .address = (uint32_t)lwip_send }, /* lwip_send */
    { .hash = 0x881717a9, .address = (uint32_t)&I_Fishing_123x52 }, /* I_Fishing_123x52 */
    { .hash = 0x883eb07c, .address = (uint32_t)longjmp }, /* longjmp */
    { .hash = 0x8872a77b, .address = (uint32_t)popup_reset }, /* popup_reset */
    { .hash = 0x889e7a5d, .address = (uint32_t)furi_thread_get_id }, /* furi_thread_get_id */
    { .hash = 0x88d2f21f, .address = (uint32_t)furi_semaphore_free }, /* furi_semaphore_free */
    { .hash = 0x891c181f, .address = (uint32_t)subghz_setting_get_preset_data }, /* subghz_setting_get_preset_data */
    { .hash = 0x89219306, .address = (uint32_t)subghz_setting_get_preset_name }, /* subghz_setting_get_preset_name */
    { .hash = 0x8a00754d, .address = (uint32_t)bit_buffer_write_bytes }, /* bit_buffer_write_bytes */
    { .hash = 0x8a05b0d3, .address = (uint32_t)dir_walk_free }, /* dir_walk_free */
    { .hash = 0x8a0a97c3, .address = (uint32_t)dir_walk_open }, /* dir_walk_open */
    { .hash = 0x8a0c0d8d, .address = (uint32_t)dir_walk_read }, /* dir_walk_read */
    { .hash = 0x8ab50484, .address = (uint32_t)dolphin_deed }, /* dolphin_deed */
    { .hash = 0x8b21ae9a, .address = (uint32_t)submenu_add_item }, /* submenu_add_item */
    { .hash = 0x8b43c111, .address = (uint32_t)validator_is_file_callback }, /* validator_is_file_callback */
    { .hash = 0x8b4e4a71, .address = (uint32_t)furi_string_start_with_str }, /* furi_string_start_with_str */
    { .hash = 0x8ba7e5f0, .address = (uint32_t)text_input_set_header_text }, /* text_input_set_header_text */
    { .hash = 0x8bc6a6ef, .address = (uint32_t)view_set_context }, /* view_set_context */
    { .hash = 0x8bfc82db, .address = (uint32_t)args_read_probably_quoted_string_and_trim }, /* args_read_probably_quoted_string_and_trim */
    { .hash = 0x8c819569, .address = (uint32_t)name_generator_make_random }, /* name_generator_make_random */
    { .hash = 0x8cbbc15e, .address = (uint32_t)iso14443_crc_append }, /* iso14443_crc_append */
    { .hash = 0x8d08b4c9, .address = (uint32_t)text_input_set_validator }, /* text_input_set_validator */
    { .hash = 0x8d3165a1, .address = (uint32_t)file_browser_get_view }, /* file_browser_get_view */
    { .hash = 0x8d8898b8, .address = (uint32_t)canvas_draw_frame }, /* canvas_draw_frame */
    { .hash = 0x8d97cd71, .address = (uint32_t)canvas_draw_glyph }, /* canvas_draw_glyph */
    { .hash = 0x8da35d29, .address = (uint32_t)furi_stream_buffer_alloc }, /* furi_stream_buffer_alloc */
    { .hash = 0x8dee7c3e, .address = (uint32_t)cli_registry_reload_external_commands }, /* cli_registry_reload_external_commands */
    { .hash = 0x8e28d05b, .address = (uint32_t)iso15693_3_get_block_count }, /* iso15693_3_get_block_count */
    { .hash = 0x8e3841e2, .address = (uint32_t)cli_shell_line_get_selected }, /* cli_shell_line_get_selected */
    { .hash = 0x8e40ea3e, .address = (uint32_t)flipper_format_write_empty_line }, /* flipper_format_write_empty_line */
    { .hash = 0x8e57da4c, .address = (uint32_t)subghz_worker_is_running }, /* subghz_worker_is_running */
    { .hash = 0x8e9332ab, .address = (uint32_t)infrared_is_protocol_valid }, /* infrared_is_protocol_valid */
    { .hash = 0x8ea562b6, .address = (uint32_t)&sequence_success }, /* sequence_success */
    { .hash = 0x8ec45c0d, .address = (uint32_t)furi_string_alloc_printf }, /* furi_string_alloc_printf */
    { .hash = 0x8ed343a1, .address = (uint32_t)furi_stream_buffer_reset }, /* furi_stream_buffer_reset */
    { .hash = 0x8f07d1ff, .address = (uint32_t)subghz_setting_alloc }, /* subghz_setting_alloc */
    { .hash = 0x8f1f37a6, .address = (uint32_t)icon_animation_set_update_callback }, /* icon_animation_set_update_callback */
    { .hash = 0x8f4ace5f, .address = (uint32_t)furi_event_loop_timer_alloc }, /* furi_event_loop_timer_alloc */
    { .hash = 0x8f8c1d4f, .address = (uint32_t)mjs_is_function }, /* mjs_is_function */
    { .hash = 0x8f9108a1, .address = (uint32_t)furi_stream_buffer_receive }, /* furi_stream_buffer_receive */
    { .hash = 0x8fadabe5, .address = (uint32_t)subghz_devices_stop_async_rx }, /* subghz_devices_stop_async_rx */
    { .hash = 0x8fadac27, .address = (uint32_t)subghz_devices_stop_async_tx }, /* subghz_devices_stop_async_tx */
    { .hash = 0x9094bc22, .address = (uint32_t)furi_event_loop_timer_start }, /* furi_event_loop_timer_start */
    { .hash = 0x9107569d, .address = (uint32_t)strtok_r }, /* strtok_r */
    { .hash = 0x9138099b, .address = (uint32_t)gui_view_port_send_to_front }, /* gui_view_port_send_to_front */
    { .hash = 0x9159da67, .address = (uint32_t)furi_semaphore_acquire }, /* furi_semaphore_acquire */
    { .hash = 0x9162b0c3, .address = (uint32_t)mf_classic_alloc }, /* mf_classic_alloc */
    { .hash = 0x9183ccbb, .address = (uint32_t)bit_buffer_set_byte }, /* bit_buffer_set_byte */
    { .hash = 0x918cdbe2, .address = (uint32_t)bit_buffer_set_size }, /* bit_buffer_set_size */
    { .hash = 0x9197df80, .address = (uint32_t)__extendsfdf2 }, /* __extendsfdf2 */
    { .hash = 0x91a9e111, .address = (uint32_t)furi_string_cmpi_str }, /* furi_string_cmpi_str */
    { .hash = 0x91ae8039, .address = (uint32_t)flipper_format_insert_or_update_uint32 }, /* flipper_format_insert_or_update_uint32 */
    { .hash = 0x9206d98e, .address = (uint32_t)elements_text_box }, /* elements_text_box */
    { .hash = 0x92291718, .address = (uint32_t)iso14443_3a_poller_halt }, /* iso14443_3a_poller_halt */
    { .hash = 0x925fb3e8, .address = (uint32_t)ble_profile_hid_kb_release }, /* ble_profile_hid_kb_release */
    { .hash = 0x9277341e, .address = (uint32_t)view_holder_free }, /* view_holder_free */
    { .hash = 0x927e632a, .address = (uint32_t)nfc_poller_free }, /* nfc_poller_free */
    { .hash = 0x92858dee, .address = (uint32_t)nfc_poller_stop }, /* nfc_poller_stop */
    { .hash = 0x92a7821c, .address = (uint32_t)furi_hal_speaker_is_mine }, /* furi_hal_speaker_is_mine */
    { .hash = 0x92acf2ea, .address = (uint32_t)file_browser_worker_folder_refresh }, /* file_browser_worker_folder_refresh */
    { .hash = 0x92c03ab2, .address = (uint32_t)text_input_get_view }, /* text_input_get_view */
    { .hash = 0x92f2414c, .address = (uint32_t)infrared_worker_rx_set_received_signal_callback }, /* infrared_worker_rx_set_received_signal_callback */
    { .hash = 0x92fe76cf, .address = (uint32_t)view_get_model }, /* view_get_model */
    { .hash = 0x933ca40c, .address = (uint32_t)keys_dict_rewind }, /* keys_dict_rewind */
    { .hash = 0x93b636a6, .address = (uint32_t)validator_is_file_free }, /* validator_is_file_free */
    { .hash = 0x93f2576c, .address = (uint32_t)i2s_channel_reconfig_std_clock }, /* i2s_channel_reconfig_std_clock */
    { .hash = 0x9424ab13, .address = (uint32_t)subghz_protocol_decoder_base_get_hash_data }, /* subghz_protocol_decoder_base_get_hash_data */
    { .hash = 0x946f668b, .address = (uint32_t)file_browser_worker_alloc }, /* file_browser_worker_alloc */
    { .hash = 0x949adeb3, .address = (uint32_t)subghz_worker_set_pair_callback }, /* subghz_worker_set_pair_callback */
    { .hash = 0x94add11a, .address = (uint32_t)elements_button_right }, /* elements_button_right */
    { .hash = 0x9531b48a, .address = (uint32_t)widget_free }, /* widget_free */
    { .hash = 0x953605e4, .address = (uint32_t)view_holder_set_back_callback }, /* view_holder_set_back_callback */
    { .hash = 0x956c4da5, .address = (uint32_t)elements_bubble_str }, /* elements_bubble_str */
    { .hash = 0x957212cb, .address = (uint32_t)widget_add_circle_element }, /* widget_add_circle_element */
    { .hash = 0x95b9544e, .address = (uint32_t)file_browser_worker_start }, /* file_browser_worker_start */
    { .hash = 0x95cbc77c, .address = (uint32_t)view_stack_remove_view }, /* view_stack_remove_view */
    { .hash = 0x95d7b2e5, .address = (uint32_t)furi_semaphore_get_count }, /* furi_semaphore_get_count */
    { .hash = 0x95fd32bb, .address = (uint32_t)flipper_format_buffered_file_open_existing }, /* flipper_format_buffered_file_open_existing */
    { .hash = 0x962c4710, .address = (uint32_t)view_dispatcher_enable_queue }, /* view_dispatcher_enable_queue */
    { .hash = 0x9634c355, .address = (uint32_t)furi_kernel_is_irq_or_masked }, /* furi_kernel_is_irq_or_masked */
    { .hash = 0x965e3f1c, .address = (uint32_t)mbedtls_sha1_finish }, /* mbedtls_sha1_finish */
    { .hash = 0x96633ebb, .address = (uint32_t)saved_struct_get_metadata }, /* saved_struct_get_metadata */
    { .hash = 0x96af1295, .address = (uint32_t)file_browser_worker_folder_exit }, /* file_browser_worker_folder_exit */
    { .hash = 0x96e7f0a1, .address = (uint32_t)esp_log_timestamp }, /* esp_log_timestamp */
    { .hash = 0x96f970c8, .address = (uint32_t)furi_semaphore_get_space }, /* furi_semaphore_get_space */
    { .hash = 0x97a23c96, .address = (uint32_t)furi_hal_rtc_is_flag_set }, /* furi_hal_rtc_is_flag_set */
    { .hash = 0x97e71f00, .address = (uint32_t)&furi_hal_spi_bus_handle_subghz }, /* furi_hal_spi_bus_handle_subghz */
    { .hash = 0x97f3e820, .address = (uint32_t)&sequence_single_vibro }, /* sequence_single_vibro */
    { .hash = 0x9814df21, .address = (uint32_t)file_browser_worker_set_config }, /* file_browser_worker_set_config */
    { .hash = 0x98159b51, .address = (uint32_t)mf_ultralight_copy }, /* mf_ultralight_copy */
    { .hash = 0x98174bb8, .address = (uint32_t)mf_ultralight_free }, /* mf_ultralight_free */
    { .hash = 0x98383973, .address = (uint32_t)furi_thread_free }, /* furi_thread_free */
    { .hash = 0x983a5ec1, .address = (uint32_t)furi_thread_join }, /* furi_thread_join */
    { .hash = 0x985ace0d, .address = (uint32_t)storage_simply_mkdir }, /* storage_simply_mkdir */
    { .hash = 0x986eec0a, .address = (uint32_t)mbedtls_des_setkey_dec }, /* mbedtls_des_setkey_dec */
    { .hash = 0x986ef174, .address = (uint32_t)mbedtls_des_setkey_enc }, /* mbedtls_des_setkey_enc */
    { .hash = 0x98b5951b, .address = (uint32_t)furi_hal_bt_extra_beacon_start }, /* furi_hal_bt_extra_beacon_start */
    { .hash = 0x9917216e, .address = (uint32_t)furi_init }, /* furi_init */
    { .hash = 0x99771d9a, .address = (uint32_t)furi_hal_light_blink_stop }, /* furi_hal_light_blink_stop */
    { .hash = 0x997780e5, .address = (uint32_t)icon_get_height }, /* icon_get_height */
    { .hash = 0x9990a6bd, .address = (uint32_t)nfc_free }, /* nfc_free */
    { .hash = 0x9997d181, .address = (uint32_t)nfc_stop }, /* nfc_stop */
    { .hash = 0x99f4abab, .address = (uint32_t)mf_desfire_get_application }, /* mf_desfire_get_application */
    { .hash = 0x9a02017a, .address = (uint32_t)bit_buffer_append_bytes }, /* bit_buffer_append_bytes */
    { .hash = 0x9a1ec555, .address = (uint32_t)furi_pubsub_alloc }, /* furi_pubsub_alloc */
    { .hash = 0x9a422229, .address = (uint32_t)canvas_commit }, /* canvas_commit */
    { .hash = 0x9a9a8b99, .address = (uint32_t)infrared_send }, /* infrared_send */
    { .hash = 0x9aa31d61, .address = (uint32_t)mf_ultralight_alloc }, /* mf_ultralight_alloc */
    { .hash = 0x9ab40dcf, .address = (uint32_t)version_get_version }, /* version_get_version */
    { .hash = 0x9ac53cda, .address = (uint32_t)nfc_data_generator_fill_data }, /* nfc_data_generator_fill_data */
    { .hash = 0x9ad9756a, .address = (uint32_t)version_get }, /* version_get */
    { .hash = 0x9b1a8c91, .address = (uint32_t)bit_buffer_append_right }, /* bit_buffer_append_right */
    { .hash = 0x9bc31804, .address = (uint32_t)esp_wifi_connect }, /* esp_wifi_connect */
    { .hash = 0x9bc4b4b9, .address = (uint32_t)stream_rewind }, /* stream_rewind */
    { .hash = 0x9bd259d6, .address = (uint32_t)view_stack_free }, /* view_stack_free */
    { .hash = 0x9bfff649, .address = (uint32_t)memmgr_get_total_heap }, /* memmgr_get_total_heap */
    { .hash = 0x9c120acd, .address = (uint32_t)storage_dir_close }, /* storage_dir_close */
    { .hash = 0x9c12d15a, .address = (uint32_t)furi_event_loop_subscribe_stream_buffer }, /* furi_event_loop_subscribe_stream_buffer */
    { .hash = 0x9c136b60, .address = (uint32_t)flipper_format_free }, /* flipper_format_free */
    { .hash = 0x9c1a5506, .address = (uint32_t)flipper_format_seek }, /* flipper_format_seek */
    { .hash = 0x9c1ae24f, .address = (uint32_t)flipper_format_tell }, /* flipper_format_tell */
    { .hash = 0x9c601223, .address = (uint32_t)stream_write_format }, /* stream_write_format */
    { .hash = 0x9ca9bbd0, .address = (uint32_t)furi_thread_disable_heap_trace }, /* furi_thread_disable_heap_trace */
    { .hash = 0x9cb259ab, .address = (uint32_t)path_extract_dirname }, /* path_extract_dirname */
    { .hash = 0x9cb9b169, .address = (uint32_t)elements_slightly_rounded_box }, /* elements_slightly_rounded_box */
    { .hash = 0x9d56a06f, .address = (uint32_t)elements_scrollable_text_line }, /* elements_scrollable_text_line */
    { .hash = 0x9d580b4b, .address = (uint32_t)furi_log_level_to_string }, /* furi_log_level_to_string */
    { .hash = 0x9d6c343a, .address = (uint32_t)button_panel_alloc }, /* button_panel_alloc */
    { .hash = 0x9db26e16, .address = (uint32_t)furi_event_loop_free }, /* furi_event_loop_free */
    { .hash = 0x9db998da, .address = (uint32_t)furi_event_loop_stop }, /* furi_event_loop_stop */
    { .hash = 0x9dd62ee1, .address = (uint32_t)furi_thread_set_context }, /* furi_thread_set_context */
    { .hash = 0x9e24ce92, .address = (uint32_t)file_browser_alloc }, /* file_browser_alloc */
    { .hash = 0x9e9c1ab2, .address = (uint32_t)button_panel_reset }, /* button_panel_reset */
    { .hash = 0x9ee1c27c, .address = (uint32_t)furi_thread_alloc }, /* furi_thread_alloc */
    { .hash = 0x9f6ebc55, .address = (uint32_t)file_browser_start }, /* file_browser_start */
    { .hash = 0x9f74bed6, .address = (uint32_t)subghz_devices_set_frequency }, /* subghz_devices_set_frequency */
    { .hash = 0x9f8ba228, .address = (uint32_t)furi_string_cmp_str }, /* furi_string_cmp_str */
    { .hash = 0xa02bb03f, .address = (uint32_t)furi_thread_start }, /* furi_thread_start */
    { .hash = 0xa033707f, .address = (uint32_t)flipper_format_string_alloc }, /* flipper_format_string_alloc */
    { .hash = 0xa07e96f3, .address = (uint32_t)datetime_datetime_to_timestamp }, /* datetime_datetime_to_timestamp */
    { .hash = 0xa0924b48, .address = (uint32_t)furi_thread_yield }, /* furi_thread_yield */
    { .hash = 0xa18e3554, .address = (uint32_t)furi_event_flag_get }, /* furi_event_flag_get */
    { .hash = 0xa18e6860, .address = (uint32_t)furi_event_flag_set }, /* furi_event_flag_set */
    { .hash = 0xa18fcf7c, .address = (uint32_t)gpio_reset_pin }, /* gpio_reset_pin */
    { .hash = 0xa1c6f92a, .address = (uint32_t)cli_shell_completions_alloc }, /* cli_shell_completions_alloc */
    { .hash = 0xa1cc46d7, .address = (uint32_t)&sequence_blink_cyan_10 }, /* sequence_blink_cyan_10 */
    { .hash = 0xa1dae36e, .address = (uint32_t)subghz_receiver_reset }, /* subghz_receiver_reset */
    { .hash = 0xa217d439, .address = (uint32_t)flipper_format_insert_or_update_hex }, /* flipper_format_insert_or_update_hex */
    { .hash = 0xa21e3cd1, .address = (uint32_t)ble_profile_serial_set_rpc_active }, /* ble_profile_serial_set_rpc_active */
    { .hash = 0xa238224d, .address = (uint32_t)iso14443_3a_alloc }, /* iso14443_3a_alloc */
    { .hash = 0xa2445322, .address = (uint32_t)flipper_format_get_value_count }, /* flipper_format_get_value_count */
    { .hash = 0xa28c5929, .address = (uint32_t)__adddf3 }, /* __adddf3 */
    { .hash = 0xa2d390a8, .address = (uint32_t)furi_semaphore_alloc }, /* furi_semaphore_alloc */
    { .hash = 0xa2f07b5f, .address = (uint32_t)mjs_set_ffi_resolver }, /* mjs_set_ffi_resolver */
    { .hash = 0xa304212c, .address = (uint32_t)&message_sound_off }, /* message_sound_off */
    { .hash = 0xa315c092, .address = (uint32_t)infrared_send_raw_ext }, /* infrared_send_raw_ext */
    { .hash = 0xa3accf8b, .address = (uint32_t)bit_buffer_starts_with_byte }, /* bit_buffer_starts_with_byte */
    { .hash = 0xa41e22b6, .address = (uint32_t)view_set_orientation }, /* view_set_orientation */
    { .hash = 0xa4287b78, .address = (uint32_t)view_dispatcher_set_custom_event_callback }, /* view_dispatcher_set_custom_event_callback */
    { .hash = 0xa43a2c9f, .address = (uint32_t)submenu_remove_item }, /* submenu_remove_item */
    { .hash = 0xa441b9bd, .address = (uint32_t)furi_hal_display_get_panel_handle }, /* furi_hal_display_get_panel_handle */
    { .hash = 0xa44210ee, .address = (uint32_t)subghz_keystore_raw_get_data }, /* subghz_keystore_raw_get_data */
    { .hash = 0xa4628371, .address = (uint32_t)gui_direct_draw_release }, /* gui_direct_draw_release */
    { .hash = 0xa462bc86, .address = (uint32_t)dialog_ex_set_result_callback }, /* dialog_ex_set_result_callback */
    { .hash = 0xa4b4168a, .address = (uint32_t)canvas_set_color }, /* canvas_set_color */
    { .hash = 0xa4d44c91, .address = (uint32_t)furi_thread_get_stack_space }, /* furi_thread_get_stack_space */
    { .hash = 0xa534ad19, .address = (uint32_t)canvas_height }, /* canvas_height */
    { .hash = 0xa5a69b61, .address = (uint32_t)furi_hal_power_suppress_charge_exit }, /* furi_hal_power_suppress_charge_exit */
    { .hash = 0xa5a73022, .address = (uint32_t)&I_WarningDolphinFlip_45x42 }, /* I_WarningDolphinFlip_45x42 */
    { .hash = 0xa5d37b81, .address = (uint32_t)flipper_format_buffered_file_open_always }, /* flipper_format_buffered_file_open_always */
    { .hash = 0xa5de3da7, .address = (uint32_t)mf_ultralight_get_config_page }, /* mf_ultralight_get_config_page */
    { .hash = 0xa666cb6b, .address = (uint32_t)elements_multiline_text_aligned }, /* elements_multiline_text_aligned */
    { .hash = 0xa668febf, .address = (uint32_t)stream_delete_and_insert_string }, /* stream_delete_and_insert_string */
    { .hash = 0xa73e0d0b, .address = (uint32_t)locale_fahrenheit_to_celsius }, /* locale_fahrenheit_to_celsius */
    { .hash = 0xa74414b4, .address = (uint32_t)mf_classic_get_total_block_num }, /* mf_classic_get_total_block_num */
    { .hash = 0xa74aad7a, .address = (uint32_t)stream_write_vaformat }, /* stream_write_vaformat */
    { .hash = 0xa7661d7d, .address = (uint32_t)view_set_update_callback_context }, /* view_set_update_callback_context */
    { .hash = 0xa7aff4f4, .address = (uint32_t)args_get_first_word_length }, /* args_get_first_word_length */
    { .hash = 0xa7e32bd6, .address = (uint32_t)canvas_invert_color }, /* canvas_invert_color */
    { .hash = 0xa80d129b, .address = (uint32_t)furi_hal_hid_kb_press }, /* furi_hal_hid_kb_press */
    { .hash = 0xa8a3ba4e, .address = (uint32_t)scene_manager_free }, /* scene_manager_free */
    { .hash = 0xa8aae512, .address = (uint32_t)scene_manager_stop }, /* scene_manager_stop */
    { .hash = 0xa8f4232a, .address = (uint32_t)__paritysi2 }, /* __paritysi2 */
    { .hash = 0xa9603a3f, .address = (uint32_t)memmgr_heap_disable_thread_trace }, /* memmgr_heap_disable_thread_trace */
    { .hash = 0xa9c23ff0, .address = (uint32_t)furi_hal_hid_is_connected }, /* furi_hal_hid_is_connected */
    { .hash = 0xa9e82298, .address = (uint32_t)furi_thread_alloc_ex }, /* furi_thread_alloc_ex */
    { .hash = 0xa9e8a877, .address = (uint32_t)simple_array_alloc }, /* simple_array_alloc */
    { .hash = 0xa9e95939, .address = (uint32_t)subghz_devices_get_rssi }, /* subghz_devices_get_rssi */
    { .hash = 0xa9f02c63, .address = (uint32_t)__divdf3 }, /* __divdf3 */
    { .hash = 0xa9f02cc6, .address = (uint32_t)__divdi3 }, /* __divdi3 */
    { .hash = 0xa9f06c32, .address = (uint32_t)__divsf3 }, /* __divsf3 */
    { .hash = 0xaa0bc1fe, .address = (uint32_t)mf_ultralight_support_feature }, /* mf_ultralight_support_feature */
    { .hash = 0xaa228e4e, .address = (uint32_t)subghz_devices_set_rx }, /* subghz_devices_set_rx */
    { .hash = 0xaa228e90, .address = (uint32_t)subghz_devices_set_tx }, /* subghz_devices_set_tx */
    { .hash = 0xaa274271, .address = (uint32_t)subghz_transmitter_alloc_init }, /* subghz_transmitter_alloc_init */
    { .hash = 0xaa442bc6, .address = (uint32_t)iso14443_3a_get_cuid }, /* iso14443_3a_get_cuid */
    { .hash = 0xaa5738d2, .address = (uint32_t)dialog_message_set_buttons }, /* dialog_message_set_buttons */
    { .hash = 0xaa99e8c9, .address = (uint32_t)subghz_setting_get_default_frequency }, /* subghz_setting_get_default_frequency */
    { .hash = 0xaaaa4702, .address = (uint32_t)mf_classic_get_sector_trailer_by_sector }, /* mf_classic_get_sector_trailer_by_sector */
    { .hash = 0xaaced1ce, .address = (uint32_t)isprint }, /* isprint */
    { .hash = 0xab0be54c, .address = (uint32_t)&message_blue_255 }, /* message_blue_255 */
    { .hash = 0xab188eef, .address = (uint32_t)simple_array_reset }, /* simple_array_reset */
    { .hash = 0xab19222f, .address = (uint32_t)plugin_manager_free }, /* plugin_manager_free */
    { .hash = 0xab26b7f2, .address = (uint32_t)subghz_worker_free }, /* subghz_worker_free */
    { .hash = 0xab2de2b6, .address = (uint32_t)subghz_worker_stop }, /* subghz_worker_stop */
    { .hash = 0xac055b11, .address = (uint32_t)i2s_channel_enable }, /* i2s_channel_enable */
    { .hash = 0xac513fd0, .address = (uint32_t)view_holder_send_to_front }, /* view_holder_send_to_front */
    { .hash = 0xac94b72e, .address = (uint32_t)subghz_setting_get_preset_count }, /* subghz_setting_get_preset_count */
    { .hash = 0xacc5bb29, .address = (uint32_t)i2c_master_write_to_device }, /* i2c_master_write_to_device */
    { .hash = 0xad3bef35, .address = (uint32_t)memmgr_heap_get_thread_memory }, /* memmgr_heap_get_thread_memory */
    { .hash = 0xad42c81d, .address = (uint32_t)cli_print_usage }, /* cli_print_usage */
    { .hash = 0xad4b0fd5, .address = (uint32_t)gui_set_lockdown }, /* gui_set_lockdown */
    { .hash = 0xad4b3965, .address = (uint32_t)elements_scrollbar }, /* elements_scrollbar */
    { .hash = 0xada6324e, .address = (uint32_t)furi_record_close }, /* furi_record_close */
    { .hash = 0xae2c5300, .address = (uint32_t)furi_thread_list_get_at }, /* furi_thread_list_get_at */
    { .hash = 0xae500991, .address = (uint32_t)view_free_model }, /* view_free_model */
    { .hash = 0xae5e1eed, .address = (uint32_t)subghz_environment_free }, /* subghz_environment_free */
    { .hash = 0xae604099, .address = (uint32_t)furi_log_print_raw_format }, /* furi_log_print_raw_format */
    { .hash = 0xaee3d796, .address = (uint32_t)flipper_format_write_comment_cstr }, /* flipper_format_write_comment_cstr */
    { .hash = 0xaef6d1a4, .address = (uint32_t)storage_simply_remove }, /* storage_simply_remove */
    { .hash = 0xaf0b2776, .address = (uint32_t)strlcpy }, /* strlcpy */
    { .hash = 0xaf0c3fcc, .address = (uint32_t)strncmp }, /* strncmp */
    { .hash = 0xaf0c4038, .address = (uint32_t)strncpy }, /* strncpy */
    { .hash = 0xaf0cf7e2, .address = (uint32_t)widget_get_view }, /* widget_get_view */
    { .hash = 0xaf0e70ad, .address = (uint32_t)strrchr }, /* strrchr */
    { .hash = 0xaf0fbe22, .address = (uint32_t)strtoul }, /* strtoul */
    { .hash = 0xaf37d3bc, .address = (uint32_t)bit_buffer_get_size_bytes }, /* bit_buffer_get_size_bytes */
    { .hash = 0xaf44c7dd, .address = (uint32_t)value_index_uint32 }, /* value_index_uint32 */
    { .hash = 0xaf8f448f, .address = (uint32_t)esp_event_handler_register }, /* esp_event_handler_register */
    { .hash = 0xafb55087, .address = (uint32_t)elements_scrollable_text_line_str }, /* elements_scrollable_text_line_str */
    { .hash = 0xafb971d9, .address = (uint32_t)&sequence_double_vibro }, /* sequence_double_vibro */
    { .hash = 0xaffa14ed, .address = (uint32_t)subghz_setting_get_frequency_count }, /* subghz_setting_get_frequency_count */
    { .hash = 0xb02115ff, .address = (uint32_t)flipper_format_write_string }, /* flipper_format_write_string */
    { .hash = 0xb06457a4, .address = (uint32_t)__truncdfsf2 }, /* __truncdfsf2 */
    { .hash = 0xb0cf7811, .address = (uint32_t)&I_RFIDDolphinReceive_97x61 }, /* I_RFIDDolphinReceive_97x61 */
    { .hash = 0xb12ba139, .address = (uint32_t)flipper_format_file_open_existing }, /* flipper_format_file_open_existing */
    { .hash = 0xb15957f1, .address = (uint32_t)subghz_protocol_registry_get_by_index }, /* subghz_protocol_registry_get_by_index */
    { .hash = 0xb1669b61, .address = (uint32_t)flipper_application_map_to_memory }, /* flipper_application_map_to_memory */
    { .hash = 0xb235636b, .address = (uint32_t)elements_slightly_rounded_frame }, /* elements_slightly_rounded_frame */
    { .hash = 0xb23ed1fe, .address = (uint32_t)furi_semaphore_release }, /* furi_semaphore_release */
    { .hash = 0xb260e7ad, .address = (uint32_t)flipper_format_write_hex }, /* flipper_format_write_hex */
    { .hash = 0xb2721527, .address = (uint32_t)&sequence_reset_blue }, /* sequence_reset_blue */
    { .hash = 0xb2ded522, .address = (uint32_t)i2c_master_write_read_device }, /* i2c_master_write_read_device */
    { .hash = 0xb2ef6c96, .address = (uint32_t)furi_kernel_is_running }, /* furi_kernel_is_running */
    { .hash = 0xb2efa0f9, .address = (uint32_t)bit_lib_bytes_to_num_bcd }, /* bit_lib_bytes_to_num_bcd */
    { .hash = 0xb3850d3a, .address = (uint32_t)strcasecmp }, /* strcasecmp */
    { .hash = 0xb3b01449, .address = (uint32_t)subghz_receiver_alloc_init }, /* subghz_receiver_alloc_init */
    { .hash = 0xb4024f2d, .address = (uint32_t)flipper_format_write_uint32 }, /* flipper_format_write_uint32 */
    { .hash = 0xb43c3a87, .address = (uint32_t)variable_item_list_set_header }, /* variable_item_list_set_header */
    { .hash = 0xb478f5fb, .address = (uint32_t)scene_manager_get_current_scene }, /* scene_manager_get_current_scene */
    { .hash = 0xb4bfabc9, .address = (uint32_t)keys_dict_get_next_key }, /* keys_dict_get_next_key */
    { .hash = 0xb4d7632e, .address = (uint32_t)view_stack_get_view }, /* view_stack_get_view */
    { .hash = 0xb4e4e3ac, .address = (uint32_t)dialog_message_set_header }, /* dialog_message_set_header */
    { .hash = 0xb5052d26, .address = (uint32_t)file_browser_worker_set_long_load_callback }, /* file_browser_worker_set_long_load_callback */
    { .hash = 0xb52305b4, .address = (uint32_t)&sequence_display_backlight_enforce_auto }, /* sequence_display_backlight_enforce_auto */
    { .hash = 0xb54f6609, .address = (uint32_t)furi_hal_crypto_decrypt }, /* furi_hal_crypto_decrypt */
    { .hash = 0xb55b05af, .address = (uint32_t)furi_log_init }, /* furi_log_init */
    { .hash = 0xb55efb87, .address = (uint32_t)furi_log_puts }, /* furi_log_puts */
    { .hash = 0xb564cc09, .address = (uint32_t)stream_delete_and_insert }, /* stream_delete_and_insert */
    { .hash = 0xb571625c, .address = (uint32_t)mbedtls_sha1_starts }, /* mbedtls_sha1_starts */
    { .hash = 0xb5b2d6ae, .address = (uint32_t)keys_dict_alloc }, /* keys_dict_alloc */
    { .hash = 0xb60d964b, .address = (uint32_t)button_panel_reserve }, /* button_panel_reserve */
    { .hash = 0xb6191733, .address = (uint32_t)stream_clean }, /* stream_clean */
    { .hash = 0xb653a0df, .address = (uint32_t)byte_input_set_header_text }, /* byte_input_set_header_text */
    { .hash = 0xb7865efb, .address = (uint32_t)stream_write }, /* stream_write */
    { .hash = 0xb7a22125, .address = (uint32_t)subghz_setting_get_frequency }, /* subghz_setting_get_frequency */
    { .hash = 0xb7b06613, .address = (uint32_t)submenu_settings_helpers_app_start }, /* submenu_settings_helpers_app_start */
    { .hash = 0xb7cf09de, .address = (uint32_t)variable_item_list_alloc }, /* variable_item_list_alloc */
    { .hash = 0xb81a5bb3, .address = (uint32_t)&message_red_0 }, /* message_red_0 */
    { .hash = 0xb82f5555, .address = (uint32_t)flipper_application_plugin_get_descriptor }, /* flipper_application_plugin_get_descriptor */
    { .hash = 0xb85ea2d5, .address = (uint32_t)furi_hal_version_uid }, /* furi_hal_version_uid */
    { .hash = 0xb8be8b8a, .address = (uint32_t)storage_common_migrate }, /* storage_common_migrate */
    { .hash = 0xb8e347a8, .address = (uint32_t)&gpio_ext_pa4 }, /* gpio_ext_pa4 */
    { .hash = 0xb8e347aa, .address = (uint32_t)&gpio_ext_pa6 }, /* gpio_ext_pa6 */
    { .hash = 0xb8e347c7, .address = (uint32_t)&gpio_ext_pb2 }, /* gpio_ext_pb2 */
    { .hash = 0xb8e347c8, .address = (uint32_t)&gpio_ext_pb3 }, /* gpio_ext_pb3 */
    { .hash = 0xb8e347e6, .address = (uint32_t)&gpio_ext_pc0 }, /* gpio_ext_pc0 */
    { .hash = 0xb8e347e7, .address = (uint32_t)&gpio_ext_pc1 }, /* gpio_ext_pc1 */
    { .hash = 0xb8e347e9, .address = (uint32_t)&gpio_ext_pc3 }, /* gpio_ext_pc3 */
    { .hash = 0xb8ea6245, .address = (uint32_t)furi_hal_nfc_field_detect_stop }, /* furi_hal_nfc_field_detect_stop */
    { .hash = 0xb8fef056, .address = (uint32_t)variable_item_list_reset }, /* variable_item_list_reset */
    { .hash = 0xb8ff9f17, .address = (uint32_t)view_stack_add_view }, /* view_stack_add_view */
    { .hash = 0xb9986c58, .address = (uint32_t)&gpio_ibutton }, /* gpio_ibutton */
    { .hash = 0xb9d4ae5e, .address = (uint32_t)mbedtls_sha1_update }, /* mbedtls_sha1_update */
    { .hash = 0xb9ed2145, .address = (uint32_t)hex_chars_to_uint64 }, /* hex_chars_to_uint64 */
    { .hash = 0xb9f92cce, .address = (uint32_t)nfc_set_guard_time_us }, /* nfc_set_guard_time_us */
    { .hash = 0xba08a5cc, .address = (uint32_t)composite_api_resolver_alloc }, /* composite_api_resolver_alloc */
    { .hash = 0xba3c89f0, .address = (uint32_t)furi_hal_bt_start_advertising }, /* furi_hal_bt_start_advertising */
    { .hash = 0xba9af481, .address = (uint32_t)byte_input_get_view }, /* byte_input_get_view */
    { .hash = 0xbab9e6e0, .address = (uint32_t)furi_hal_spi_bus_trx }, /* furi_hal_spi_bus_trx */
    { .hash = 0xbb0d8d71, .address = (uint32_t)stream_write_string }, /* stream_write_string */
    { .hash = 0xbb9ec365, .address = (uint32_t)&I_Quest_7x8 }, /* I_Quest_7x8 */
    { .hash = 0xbbf0bda8, .address = (uint32_t)strncasecmp }, /* strncasecmp */
    { .hash = 0xbc4d1c50, .address = (uint32_t)view_dispatcher_alloc }, /* view_dispatcher_alloc */
    { .hash = 0xbc867b2f, .address = (uint32_t)subghz_receiver_decode }, /* subghz_receiver_decode */
    { .hash = 0xbc94c80a, .address = (uint32_t)view_alloc }, /* view_alloc */
    { .hash = 0xbcbd5eb7, .address = (uint32_t)scene_manager_alloc }, /* scene_manager_alloc */
    { .hash = 0xbcc216ae, .address = (uint32_t)mjs_array_length }, /* mjs_array_length */
    { .hash = 0xbcfff93e, .address = (uint32_t)fprintf }, /* fprintf */
    { .hash = 0xbd0528f2, .address = (uint32_t)view_get_input_mode }, /* view_get_input_mode */
    { .hash = 0xbd1a5737, .address = (uint32_t)esp_lcd_panel_draw_bitmap }, /* esp_lcd_panel_draw_bitmap */
    { .hash = 0xbd2d8915, .address = (uint32_t)variable_item_set_locked }, /* variable_item_set_locked */
    { .hash = 0xbd42bf88, .address = (uint32_t)bit_buffer_copy_bits }, /* bit_buffer_copy_bits */
    { .hash = 0xbd482881, .address = (uint32_t)bit_buffer_copy_left }, /* bit_buffer_copy_left */
    { .hash = 0xbd5b2cd6, .address = (uint32_t)furi_hal_crypto_load_key }, /* furi_hal_crypto_load_key */
    { .hash = 0xbdd69f1b, .address = (uint32_t)memmove }, /* memmove */
    { .hash = 0xbde04aac, .address = (uint32_t)dialog_message_set_icon }, /* dialog_message_set_icon */
    { .hash = 0xbde65c88, .address = (uint32_t)dialog_message_set_text }, /* dialog_message_set_text */
    { .hash = 0xbe432758, .address = (uint32_t)stream_cache_drop }, /* stream_cache_drop */
    { .hash = 0xbe44196a, .address = (uint32_t)stream_cache_fill }, /* stream_cache_fill */
    { .hash = 0xbe443ec5, .address = (uint32_t)stream_cache_free }, /* stream_cache_free */
    { .hash = 0xbe447663, .address = (uint32_t)nfc_util_even_parity32 }, /* nfc_util_even_parity32 */
    { .hash = 0xbe4a9b7f, .address = (uint32_t)stream_cache_read }, /* stream_cache_read */
    { .hash = 0xbe4b286b, .address = (uint32_t)stream_cache_seek }, /* stream_cache_seek */
    { .hash = 0xbe4b3c1e, .address = (uint32_t)stream_cache_size }, /* stream_cache_size */
    { .hash = 0xbe582cf9, .address = (uint32_t)snprintf }, /* snprintf */
    { .hash = 0xbe6828d8, .address = (uint32_t)furi_string_utf8_length }, /* furi_string_utf8_length */
    { .hash = 0xbeb85543, .address = (uint32_t)text_input_alloc }, /* text_input_alloc */
    { .hash = 0xbec63dba, .address = (uint32_t)subghz_setting_get_hopper_frequency_count }, /* subghz_setting_get_hopper_frequency_count */
    { .hash = 0xbed5161a, .address = (uint32_t)furi_thread_get_signal_callback }, /* furi_thread_get_signal_callback */
    { .hash = 0xbf514ea3, .address = (uint32_t)__moddi3 }, /* __moddi3 */
    { .hash = 0xbf85a879, .address = (uint32_t)elements_string_fit_width }, /* elements_string_fit_width */
    { .hash = 0xbfa79191, .address = (uint32_t)string_stream_alloc }, /* string_stream_alloc */
    { .hash = 0xbfc2444e, .address = (uint32_t)__muldf3 }, /* __muldf3 */
    { .hash = 0xbfc7629b, .address = (uint32_t)nfc_device_get_protocol_name }, /* nfc_device_get_protocol_name */
    { .hash = 0xbfd6b5d7, .address = (uint32_t)nfc_set_fdt_poll_poll_us }, /* nfc_set_fdt_poll_poll_us */
    { .hash = 0xbfe83bbb, .address = (uint32_t)text_input_reset }, /* text_input_reset */
    { .hash = 0xbfec33a1, .address = (uint32_t)furi_log_add_handler }, /* furi_log_add_handler */
    { .hash = 0xc009287c, .address = (uint32_t)storage_common_copy }, /* storage_common_copy */
    { .hash = 0xc01201dd, .address = (uint32_t)storage_common_stat }, /* storage_common_stat */
    { .hash = 0xc0b4831c, .address = (uint32_t)__furi_critical_exit }, /* __furi_critical_exit */
    { .hash = 0xc0c6e8cf, .address = (uint32_t)flipper_format_read_float }, /* flipper_format_read_float */
    { .hash = 0xc0fe5a29, .address = (uint32_t)flipper_format_read_int32 }, /* flipper_format_read_int32 */
    { .hash = 0xc11994c2, .address = (uint32_t)furi_hal_rtc_get_datetime }, /* furi_hal_rtc_get_datetime */
    { .hash = 0xc11fe51b, .address = (uint32_t)nfc_device_get_protocol }, /* nfc_device_get_protocol */
    { .hash = 0xc141ebd2, .address = (uint32_t)args_read_duration }, /* args_read_duration */
    { .hash = 0xc1e0c6d8, .address = (uint32_t)storage_common_mkdir }, /* storage_common_mkdir */
    { .hash = 0xc1fe1b10, .address = (uint32_t)popup_set_callback }, /* popup_set_callback */
    { .hash = 0xc20e9901, .address = (uint32_t)furi_hal_spi_bus_handle_init }, /* furi_hal_spi_bus_handle_init */
    { .hash = 0xc2c80038, .address = (uint32_t)scene_manager_next_scene }, /* scene_manager_next_scene */
    { .hash = 0xc35dcba2, .address = (uint32_t)widget_add_icon_element }, /* widget_add_icon_element */
    { .hash = 0xc382f6ee, .address = (uint32_t)&message_delay_250 }, /* message_delay_250 */
    { .hash = 0xc383030c, .address = (uint32_t)&message_delay_500 }, /* message_delay_500 */
    { .hash = 0xc3c55930, .address = (uint32_t)widget_add_string_element }, /* widget_add_string_element */
    { .hash = 0xc3f52c6d, .address = (uint32_t)dialogs_app_process_module_file_browser }, /* dialogs_app_process_module_file_browser */
    { .hash = 0xc43afa08, .address = (uint32_t)mjs_get_double }, /* mjs_get_double */
    { .hash = 0xc4480b87, .address = (uint32_t)mjs_to_string }, /* mjs_to_string */
    { .hash = 0xc4e0d2ba, .address = (uint32_t)storage_file_free }, /* storage_file_free */
    { .hash = 0xc4e5b9aa, .address = (uint32_t)storage_file_open }, /* storage_file_open */
    { .hash = 0xc4e72f74, .address = (uint32_t)storage_file_read }, /* storage_file_read */
    { .hash = 0xc4e7bc60, .address = (uint32_t)storage_file_seek }, /* storage_file_seek */
    { .hash = 0xc4e7d013, .address = (uint32_t)storage_file_size }, /* storage_file_size */
    { .hash = 0xc4e81295, .address = (uint32_t)storage_file_sync }, /* storage_file_sync */
    { .hash = 0xc4e849a9, .address = (uint32_t)storage_file_tell }, /* storage_file_tell */
    { .hash = 0xc4fd684c, .address = (uint32_t)mf_classic_get_sector_trailer_num_by_block }, /* mf_classic_get_sector_trailer_num_by_block */
    { .hash = 0xc523bff7, .address = (uint32_t)stream_seek_to_char }, /* stream_seek_to_char */
    { .hash = 0xc563cc53, .address = (uint32_t)subghz_protocol_blocks_lfsr_digest8 }, /* subghz_protocol_blocks_lfsr_digest8 */
    { .hash = 0xc586a3ae, .address = (uint32_t)furi_hal_nfc_acquire }, /* furi_hal_nfc_acquire */
    { .hash = 0xc5cdfd42, .address = (uint32_t)nfc_listener_free }, /* nfc_listener_free */
    { .hash = 0xc5d52806, .address = (uint32_t)nfc_listener_stop }, /* nfc_listener_stop */
    { .hash = 0xc605f90e, .address = (uint32_t)pipe_set_callback_context }, /* pipe_set_callback_context */
    { .hash = 0xc614f641, .address = (uint32_t)furi_thread_set_state_context }, /* furi_thread_set_state_context */
    { .hash = 0xc648e496, .address = (uint32_t)subghz_setting_free }, /* subghz_setting_free */
    { .hash = 0xc64c2194, .address = (uint32_t)subghz_setting_load }, /* subghz_setting_load */
    { .hash = 0xc655b74e, .address = (uint32_t)submenu_alloc }, /* submenu_alloc */
    { .hash = 0xc6a36b60, .address = (uint32_t)subghz_environment_get_keystore }, /* subghz_environment_get_keystore */
    { .hash = 0xc6da5261, .address = (uint32_t)cli_shell_completions_free }, /* cli_shell_completions_free */
    { .hash = 0xc77ee764, .address = (uint32_t)bt_disconnect }, /* bt_disconnect */
    { .hash = 0xc7859dc6, .address = (uint32_t)submenu_reset }, /* submenu_reset */
    { .hash = 0xc7951afe, .address = (uint32_t)flipper_format_seek_to_end }, /* flipper_format_seek_to_end */
    { .hash = 0xc83c90e7, .address = (uint32_t)bit_buffer_alloc }, /* bit_buffer_alloc */
    { .hash = 0xc85a9602, .address = (uint32_t)furi_hal_light_blink_start }, /* furi_hal_light_blink_start */
    { .hash = 0xc85e6527, .address = (uint32_t)keys_dict_is_key_present }, /* keys_dict_is_key_present */
    { .hash = 0xc8a8c265, .address = (uint32_t)subghz_keystore_get_data }, /* subghz_keystore_get_data */
    { .hash = 0xc9149901, .address = (uint32_t)flipper_format_stream_write_comment_cstr }, /* flipper_format_stream_write_comment_cstr */
    { .hash = 0xc94c53fc, .address = (uint32_t)lock_screen_get_style }, /* lock_screen_get_style */
    { .hash = 0xc96c775f, .address = (uint32_t)bit_buffer_reset }, /* bit_buffer_reset */
    { .hash = 0xc9a091fe, .address = (uint32_t)esp_event_loop_create_default }, /* esp_event_loop_create_default */
    { .hash = 0xc9b48880, .address = (uint32_t)furi_thread_set_current_priority }, /* furi_thread_set_current_priority */
    { .hash = 0xc9e513e8, .address = (uint32_t)mjs_arg }, /* mjs_arg */
    { .hash = 0xc9e51f03, .address = (uint32_t)mjs_del }, /* mjs_del */
    { .hash = 0xc9e52bce, .address = (uint32_t)mjs_get }, /* mjs_get */
    { .hash = 0xc9e55022, .address = (uint32_t)mjs_own }, /* mjs_own */
    { .hash = 0xc9e55eda, .address = (uint32_t)mjs_set }, /* mjs_set */
    { .hash = 0xca1f4e47, .address = (uint32_t)submenu_settings_helpers_free }, /* submenu_settings_helpers_free */
    { .hash = 0xca45f781, .address = (uint32_t)nfc_poller_get_data }, /* nfc_poller_get_data */
    { .hash = 0xca4a6509, .address = (uint32_t)mjs_is_boolean }, /* mjs_is_boolean */
    { .hash = 0xca5e25dc, .address = (uint32_t)dir_walk_alloc }, /* dir_walk_alloc */
    { .hash = 0xca77a4a5, .address = (uint32_t)&message_vibro_off }, /* message_vibro_off */
    { .hash = 0xca90eebc, .address = (uint32_t)putchar }, /* putchar */
    { .hash = 0xcab546b1, .address = (uint32_t)furi_hal_random_fill_buf }, /* furi_hal_random_fill_buf */
    { .hash = 0xcb00debe, .address = (uint32_t)mjs_get_global }, /* mjs_get_global */
    { .hash = 0xcb205192, .address = (uint32_t)esp_event_handler_unregister }, /* esp_event_handler_unregister */
    { .hash = 0xcb236ca6, .address = (uint32_t)cli_ansi_parser_feed }, /* cli_ansi_parser_feed */
    { .hash = 0xcb23a3f4, .address = (uint32_t)cli_ansi_parser_free }, /* cli_ansi_parser_free */
    { .hash = 0xcb47d906, .address = (uint32_t)nfc_alloc }, /* nfc_alloc */
    { .hash = 0xcb57017f, .address = (uint32_t)&IP_EVENT }, /* IP_EVENT */
    { .hash = 0xcb5a4f62, .address = (uint32_t)mjs_create }, /* mjs_create */
    { .hash = 0xcb8aa2fe, .address = (uint32_t)nfc_device_copy_data }, /* nfc_device_copy_data */
    { .hash = 0xcbb21513, .address = (uint32_t)furi_hal_rfid_field_detect_stop }, /* furi_hal_rfid_field_detect_stop */
    { .hash = 0xcbb8d3d7, .address = (uint32_t)dialog_file_browser_show }, /* dialog_file_browser_show */
    { .hash = 0xcbfeb206, .address = (uint32_t)furi_string_alloc_set }, /* furi_string_alloc_set */
    { .hash = 0xcc1668b7, .address = (uint32_t)flipper_format_stream_write_value_line }, /* flipper_format_stream_write_value_line */
    { .hash = 0xcc5b897a, .address = (uint32_t)notification_message_block }, /* notification_message_block */
    { .hash = 0xcc6ae517, .address = (uint32_t)subghz_devices_idle }, /* subghz_devices_idle */
    { .hash = 0xcc6b0f4d, .address = (uint32_t)subghz_devices_init }, /* subghz_devices_init */
    { .hash = 0xcc91c6c9, .address = (uint32_t)nfc_start }, /* nfc_start */
    { .hash = 0xccd89f64, .address = (uint32_t)loading_free }, /* loading_free */
    { .hash = 0xcd1257dd, .address = (uint32_t)furi_thread_flags_get }, /* furi_thread_flags_get */
    { .hash = 0xcd128ae9, .address = (uint32_t)furi_thread_flags_set }, /* furi_thread_flags_set */
    { .hash = 0xcd1484c2, .address = (uint32_t)mjs_disown }, /* mjs_disown */
    { .hash = 0xcde345df, .address = (uint32_t)name_generator_make_random_prefixed }, /* name_generator_make_random_prefixed */
    { .hash = 0xcdeeace4, .address = (uint32_t)i2c_driver_install }, /* i2c_driver_install */
    { .hash = 0xce19133e, .address = (uint32_t)flipper_format_write_float }, /* flipper_format_write_float */
    { .hash = 0xce3c78d1, .address = (uint32_t)scene_manager_has_previous_scene }, /* scene_manager_has_previous_scene */
    { .hash = 0xce421a34, .address = (uint32_t)simple_array_get_count }, /* simple_array_get_count */
    { .hash = 0xce508498, .address = (uint32_t)flipper_format_write_int32 }, /* flipper_format_write_int32 */
    { .hash = 0xce9e6f25, .address = (uint32_t)mf_classic_set_uid }, /* mf_classic_set_uid */
    { .hash = 0xceed3561, .address = (uint32_t)vTaskDelete }, /* vTaskDelete */
    { .hash = 0xcf54c2f3, .address = (uint32_t)hex_chars_to_uint8 }, /* hex_chars_to_uint8 */
    { .hash = 0xcfb7dc05, .address = (uint32_t)submenu_free }, /* submenu_free */
    { .hash = 0xcfbd5603, .address = (uint32_t)&sequence_semi_success }, /* sequence_semi_success */
    { .hash = 0xcfc37a68, .address = (uint32_t)view_tie_icon_animation }, /* view_tie_icon_animation */
    { .hash = 0xd00066e0, .address = (uint32_t)furi_string_equal_str }, /* furi_string_equal_str */
    { .hash = 0xd0191894, .address = (uint32_t)bit_buffer_append }, /* bit_buffer_append */
    { .hash = 0xd03b919f, .address = (uint32_t)furi_hal_infrared_set_tx_output }, /* furi_hal_infrared_set_tx_output */
    { .hash = 0xd05afca7, .address = (uint32_t)&sequence_error }, /* sequence_error */
    { .hash = 0xd0789005, .address = (uint32_t)gui_is_lockdown }, /* gui_is_lockdown */
    { .hash = 0xd112cc9b, .address = (uint32_t)furi_event_loop_timer_get_remaining_time }, /* furi_event_loop_timer_get_remaining_time */
    { .hash = 0xd126eea3, .address = (uint32_t)canvas_glyph_width }, /* canvas_glyph_width */
    { .hash = 0xd12d9c8c, .address = (uint32_t)cli_ansi_parser_feed_timeout }, /* cli_ansi_parser_feed_timeout */
    { .hash = 0xd1702188, .address = (uint32_t)pipe_detach_from_event_loop }, /* pipe_detach_from_event_loop */
    { .hash = 0xd25b78fc, .address = (uint32_t)text_box_set_focus }, /* text_box_set_focus */
    { .hash = 0xd2a1cc32, .address = (uint32_t)furi_hal_bt_extra_beacon_set_data }, /* furi_hal_bt_extra_beacon_set_data */
    { .hash = 0xd33d4ba7, .address = (uint32_t)flipper_format_read_hex_uint64 }, /* flipper_format_read_hex_uint64 */
    { .hash = 0xd3548936, .address = (uint32_t)furi_event_flag_free }, /* furi_event_flag_free */
    { .hash = 0xd35d93e9, .address = (uint32_t)furi_event_flag_wait }, /* furi_event_flag_wait */
    { .hash = 0xd39ff087, .address = (uint32_t)mf_classic_poller_sync_read_block }, /* mf_classic_poller_sync_read_block */
    { .hash = 0xd3b121a8, .address = (uint32_t)mjs_is_array }, /* mjs_is_array */
    { .hash = 0xd3ee8433, .address = (uint32_t)view_dispatcher_switch_to_view }, /* view_dispatcher_switch_to_view */
    { .hash = 0xd436bfc1, .address = (uint32_t)mf_classic_block_to_value }, /* mf_classic_block_to_value */
    { .hash = 0xd4aa49a0, .address = (uint32_t)manchester_advance }, /* manchester_advance */
    { .hash = 0xd4da4966, .address = (uint32_t)bit_buffer_write_bytes_mid }, /* bit_buffer_write_bytes_mid */
    { .hash = 0xd51ac80e, .address = (uint32_t)bit_buffer_set_byte_with_parity }, /* bit_buffer_set_byte_with_parity */
    { .hash = 0xd51eed4c, .address = (uint32_t)datetime_get_days_per_month }, /* datetime_get_days_per_month */
    { .hash = 0xd551b8be, .address = (uint32_t)i2s_channel_disable }, /* i2s_channel_disable */
    { .hash = 0xd5adbc28, .address = (uint32_t)flipper_format_file_alloc }, /* flipper_format_file_alloc */
    { .hash = 0xd5d1fa73, .address = (uint32_t)flipper_format_file_close }, /* flipper_format_file_close */
    { .hash = 0xd636700d, .address = (uint32_t)furi_hal_nfc_field_detect_start }, /* furi_hal_nfc_field_detect_start */
    { .hash = 0xd63c6b71, .address = (uint32_t)button_panel_free }, /* button_panel_free */
    { .hash = 0xd6a9f1d0, .address = (uint32_t)view_set_exit_callback }, /* view_set_exit_callback */
    { .hash = 0xd700d2f2, .address = (uint32_t)canvas_draw_icon_ex }, /* canvas_draw_icon_ex */
    { .hash = 0xd7091c95, .address = (uint32_t)variable_item_list_free }, /* variable_item_list_free */
    { .hash = 0xd73f9820, .address = (uint32_t)__furi_critical_enter }, /* __furi_critical_enter */
    { .hash = 0xd750477a, .address = (uint32_t)furi_background }, /* furi_background */
    { .hash = 0xd75c81a1, .address = (uint32_t)plugin_manager_get_ep }, /* plugin_manager_get_ep */
    { .hash = 0xd78d168d, .address = (uint32_t)subghz_protocol_decoder_base_get_string }, /* subghz_protocol_decoder_base_get_string */
    { .hash = 0xd7b5f6fa, .address = (uint32_t)submenu_set_orientation }, /* submenu_set_orientation */
    { .hash = 0xd83f118e, .address = (uint32_t)mjs_mk_number }, /* mjs_mk_number */
    { .hash = 0xd85bd689, .address = (uint32_t)mjs_nargs }, /* mjs_nargs */
    { .hash = 0xd875bb89, .address = (uint32_t)lwip_socket }, /* lwip_socket */
    { .hash = 0xd89dd0bb, .address = (uint32_t)flipper_format_write_comment }, /* flipper_format_write_comment */
    { .hash = 0xd8b8ff20, .address = (uint32_t)view_dispatcher_attach_to_gui }, /* view_dispatcher_attach_to_gui */
    { .hash = 0xd8baf456, .address = (uint32_t)scene_manager_previous_scene }, /* scene_manager_previous_scene */
    { .hash = 0xd8d0e145, .address = (uint32_t)scene_manager_set_scene_state }, /* scene_manager_set_scene_state */
    { .hash = 0xd91f014d, .address = (uint32_t)view_dispatcher_set_navigation_event_callback }, /* view_dispatcher_set_navigation_event_callback */
    { .hash = 0xd93acffc, .address = (uint32_t)mjs_mk_object }, /* mjs_mk_object */
    { .hash = 0xd942aa95, .address = (uint32_t)cli_shell_free }, /* cli_shell_free */
    { .hash = 0xd944cfe3, .address = (uint32_t)cli_shell_join }, /* cli_shell_join */
    { .hash = 0xd959a3d5, .address = (uint32_t)canvas_draw_icon_animation }, /* canvas_draw_icon_animation */
    { .hash = 0xd988dc2f, .address = (uint32_t)nfc_scanner_alloc }, /* nfc_scanner_alloc */
    { .hash = 0xd9b8d0fe, .address = (uint32_t)furi_hal_subghz_idle }, /* furi_hal_subghz_idle */
    { .hash = 0xda3ec7b3, .address = (uint32_t)datetime_timestamp_to_datetime }, /* datetime_timestamp_to_datetime */
    { .hash = 0xda809a94, .address = (uint32_t)keys_dict_add_key }, /* keys_dict_add_key */
    { .hash = 0xdac62733, .address = (uint32_t)flipper_application_is_plugin }, /* flipper_application_is_plugin */
    { .hash = 0xdad2c9f2, .address = (uint32_t)nfc_scanner_start }, /* nfc_scanner_start */
    { .hash = 0xdaf4df4b, .address = (uint32_t)icon_animation_get_width }, /* icon_animation_get_width */
    { .hash = 0xdb02a5b6, .address = (uint32_t)canvas_string_width }, /* canvas_string_width */
    { .hash = 0xdb149d9e, .address = (uint32_t)pipe_install_as_stdio }, /* pipe_install_as_stdio */
    { .hash = 0xdb928d96, .address = (uint32_t)button_menu_free }, /* button_menu_free */
    { .hash = 0xdbbbe852, .address = (uint32_t)popup_disable_timeout }, /* popup_disable_timeout */
    { .hash = 0xdbd55fd1, .address = (uint32_t)variable_item_set_item_label }, /* variable_item_set_item_label */
    { .hash = 0xdbde159d, .address = (uint32_t)cli_shell_line_prompt_length }, /* cli_shell_line_prompt_length */
    { .hash = 0xdbe8fe93, .address = (uint32_t)flipper_format_key_exist }, /* flipper_format_key_exist */
    { .hash = 0xdc070dfa, .address = (uint32_t)dialog_message_free }, /* dialog_message_free */
    { .hash = 0xdc0cbdab, .address = (uint32_t)text_box_get_view }, /* text_box_get_view */
    { .hash = 0xdc0e05b9, .address = (uint32_t)dialog_message_show }, /* dialog_message_show */
    { .hash = 0xdc386f64, .address = (uint32_t)subghz_devices_flush_rx }, /* subghz_devices_flush_rx */
    { .hash = 0xdc6dcd22, .address = (uint32_t)mf_classic_get_read_sectors_and_keys }, /* mf_classic_get_read_sectors_and_keys */
    { .hash = 0xdc6ddfaa, .address = (uint32_t)view_dispatcher_send_custom_event }, /* view_dispatcher_send_custom_event */
    { .hash = 0xdc998f7e, .address = (uint32_t)furi_event_loop_subscribe_message_queue }, /* furi_event_loop_subscribe_message_queue */
    { .hash = 0xdcf93e25, .address = (uint32_t)flipper_format_update_hex }, /* flipper_format_update_hex */
    { .hash = 0xdd41e5e1, .address = (uint32_t)&subghz_protocol_registry }, /* subghz_protocol_registry */
    { .hash = 0xdd504af3, .address = (uint32_t)furi_hal_display_get_h_res }, /* furi_hal_display_get_h_res */
    { .hash = 0xdd64b412, .address = (uint32_t)ble_profile_serial_tx }, /* ble_profile_serial_tx */
    { .hash = 0xdd8942e9, .address = (uint32_t)file_browser_configure }, /* file_browser_configure */
    { .hash = 0xdda0fada, .address = (uint32_t)mf_classic_free }, /* mf_classic_free */
    { .hash = 0xddc80662, .address = (uint32_t)flipper_format_read_header }, /* flipper_format_read_header */
    { .hash = 0xde112d5f, .address = (uint32_t)&I_DolphinSuccess_91x55 }, /* I_DolphinSuccess_91x55 */
    { .hash = 0xde4da201, .address = (uint32_t)furi_hal_display_get_v_res }, /* furi_hal_display_get_v_res */
    { .hash = 0xde861c02, .address = (uint32_t)view_holder_set_view }, /* view_holder_set_view */
    { .hash = 0xde8c3a2c, .address = (uint32_t)furi_thread_alloc_service }, /* furi_thread_alloc_service */
    { .hash = 0xdf0ba445, .address = (uint32_t)flipper_format_read_bool }, /* flipper_format_read_bool */
    { .hash = 0xdf2fd110, .address = (uint32_t)icon_get_frame_data }, /* icon_get_frame_data */
    { .hash = 0xdf379799, .address = (uint32_t)view_set_update_callback }, /* view_set_update_callback */
    { .hash = 0xdf6806b0, .address = (uint32_t)subghz_devices_end }, /* subghz_devices_end */
    { .hash = 0xdf79c45d, .address = (uint32_t)property_value_out }, /* property_value_out */
    { .hash = 0xdf81ea15, .address = (uint32_t)bt_keys_storage_set_storage_path }, /* bt_keys_storage_set_storage_path */
    { .hash = 0xdfc741b8, .address = (uint32_t)hex_char_to_hex_nibble }, /* hex_char_to_hex_nibble */
    { .hash = 0xdfd4830b, .address = (uint32_t)furi_timer_pending_callback }, /* furi_timer_pending_callback */
    { .hash = 0xe07429bb, .address = (uint32_t)mjs_is_undefined }, /* mjs_is_undefined */
    { .hash = 0xe0a29adc, .address = (uint32_t)submenu_add_separator }, /* submenu_add_separator */
    { .hash = 0xe1001287, .address = (uint32_t)view_holder_alloc }, /* view_holder_alloc */
    { .hash = 0xe187b854, .address = (uint32_t)view_set_enter_callback }, /* view_set_enter_callback */
    { .hash = 0xe1adfa83, .address = (uint32_t)furi_string_search_char }, /* furi_string_search_char */
    { .hash = 0xe1d208c0, .address = (uint32_t)subghz_setting_get_frequency_default_index }, /* subghz_setting_get_frequency_default_index */
    { .hash = 0xe1ed2313, .address = (uint32_t)nfc_poller_alloc }, /* nfc_poller_alloc */
    { .hash = 0xe2491997, .address = (uint32_t)esp_efuse_mac_get_default }, /* esp_efuse_mac_get_default */
    { .hash = 0xe251c217, .address = (uint32_t)bit_lib_num_to_bytes_be }, /* bit_lib_num_to_bytes_be */
    { .hash = 0xe28d6eec, .address = (uint32_t)mf_classic_get_total_sectors_num }, /* mf_classic_get_total_sectors_num */
    { .hash = 0xe29c0053, .address = (uint32_t)furi_hal_crypto_enclave_load_key }, /* furi_hal_crypto_enclave_load_key */
    { .hash = 0xe2f50b80, .address = (uint32_t)&I_Move_flipper_26x39 }, /* I_Move_flipper_26x39 */
    { .hash = 0xe33710d6, .address = (uint32_t)nfc_poller_start }, /* nfc_poller_start */
    { .hash = 0xe3711faa, .address = (uint32_t)esp_wifi_set_mode }, /* esp_wifi_set_mode */
    { .hash = 0xe380fc4d, .address = (uint32_t)furi_string_get_char }, /* furi_string_get_char */
    { .hash = 0xe3812d8b, .address = (uint32_t)furi_string_get_cstr }, /* furi_string_get_cstr */
    { .hash = 0xe381bfbc, .address = (uint32_t)loading_get_view }, /* loading_get_view */
    { .hash = 0xe3d9a0fc, .address = (uint32_t)mjs_mk_string }, /* mjs_mk_string */
    { .hash = 0xe4147bf9, .address = (uint32_t)furi_string_cmpi }, /* furi_string_cmpi */
    { .hash = 0xe4161c5e, .address = (uint32_t)&I_DolphinWait_59x54 }, /* I_DolphinWait_59x54 */
    { .hash = 0xe41634f2, .address = (uint32_t)furi_string_free }, /* furi_string_free */
    { .hash = 0xe4170734, .address = (uint32_t)furi_string_hash }, /* furi_string_hash */
    { .hash = 0xe419481b, .address = (uint32_t)furi_string_left }, /* furi_string_left */
    { .hash = 0xe41a0107, .address = (uint32_t)furi_string_move }, /* furi_string_move */
    { .hash = 0xe41d324b, .address = (uint32_t)furi_string_size }, /* furi_string_size */
    { .hash = 0xe41d6aab, .address = (uint32_t)furi_string_swap }, /* furi_string_swap */
    { .hash = 0xe41de2cc, .address = (uint32_t)furi_string_trim }, /* furi_string_trim */
    { .hash = 0xe429dfd3, .address = (uint32_t)__floatundidf }, /* __floatundidf */
    { .hash = 0xe429e1c2, .address = (uint32_t)__floatundisf }, /* __floatundisf */
    { .hash = 0xe4321982, .address = (uint32_t)__floatunsidf }, /* __floatunsidf */
    { .hash = 0xe44351b4, .address = (uint32_t)subghz_environment_get_protocol_name_registry }, /* subghz_environment_get_protocol_name_registry */
    { .hash = 0xe444402a, .address = (uint32_t)furi_thread_set_appid }, /* furi_thread_set_appid */
    { .hash = 0xe4bdba7e, .address = (uint32_t)keys_dict_delete_key }, /* keys_dict_delete_key */
    { .hash = 0xe5008d82, .address = (uint32_t)furi_message_queue_get }, /* furi_message_queue_get */
    { .hash = 0xe500b5db, .address = (uint32_t)furi_message_queue_put }, /* furi_message_queue_put */
    { .hash = 0xe50f3ae0, .address = (uint32_t)flipper_format_insert_or_update_bool }, /* flipper_format_insert_or_update_bool */
    { .hash = 0xe58507da, .address = (uint32_t)esp_wifi_set_storage }, /* esp_wifi_set_storage */
    { .hash = 0xe5b27f65, .address = (uint32_t)subghz_block_generic_serialize }, /* subghz_block_generic_serialize */
    { .hash = 0xe5b2f9b9, .address = (uint32_t)subghz_setting_get_preset_data_size }, /* subghz_setting_get_preset_data_size */
    { .hash = 0xe61d840f, .address = (uint32_t)vsnprintf }, /* vsnprintf */
    { .hash = 0xe6563372, .address = (uint32_t)mjs_exec_file }, /* mjs_exec_file */
    { .hash = 0xe66b9b45, .address = (uint32_t)furi_hal_nfc_release }, /* furi_hal_nfc_release */
    { .hash = 0xe6d795e7, .address = (uint32_t)furi_thread_list_get_isr_time }, /* furi_thread_list_get_isr_time */
    { .hash = 0xe776c415, .address = (uint32_t)i2s_channel_write }, /* i2s_channel_write */
    { .hash = 0xe7914ee4, .address = (uint32_t)mjs_get_string }, /* mjs_get_string */
    { .hash = 0xe7dfedba, .address = (uint32_t)strint_to_int32 }, /* strint_to_int32 */
    { .hash = 0xe7dfee1f, .address = (uint32_t)strint_to_int64 }, /* strint_to_int64 */
    { .hash = 0xe7ff8ccf, .address = (uint32_t)mbedtls_des3_set2key_dec }, /* mbedtls_des3_set2key_dec */
    { .hash = 0xe7ff9239, .address = (uint32_t)mbedtls_des3_set2key_enc }, /* mbedtls_des3_set2key_enc */
    { .hash = 0xe860249c, .address = (uint32_t)elements_bold_rounded_frame }, /* elements_bold_rounded_frame */
    { .hash = 0xe88d2352, .address = (uint32_t)bt_profile_restore_default }, /* bt_profile_restore_default */
    { .hash = 0xe8e00f45, .address = (uint32_t)iso14443_4b_poller_send_block }, /* iso14443_4b_poller_send_block */
    { .hash = 0xe92d74d6, .address = (uint32_t)mbedtls_des3_crypt_cbc }, /* mbedtls_des3_crypt_cbc */
    { .hash = 0xe941a557, .address = (uint32_t)icon_animation_alloc }, /* icon_animation_alloc */
    { .hash = 0xe987dcdd, .address = (uint32_t)byte_input_set_result_callback }, /* byte_input_set_result_callback */
    { .hash = 0xe9951039, .address = (uint32_t)canvas_draw_str_aligned }, /* canvas_draw_str_aligned */
    { .hash = 0xea8b931a, .address = (uint32_t)icon_animation_start }, /* icon_animation_start */
    { .hash = 0xeaa35d00, .address = (uint32_t)dialog_ex_set_context }, /* dialog_ex_set_context */
    { .hash = 0xeabf1a2b, .address = (uint32_t)furi_string_end_withi }, /* furi_string_end_withi */
    { .hash = 0xeaed7342, .address = (uint32_t)locale_on_system_start }, /* locale_on_system_start */
    { .hash = 0xeb07fc1b, .address = (uint32_t)mf_classic_poller_sync_detect_type }, /* mf_classic_poller_sync_detect_type */
    { .hash = 0xeb104d4c, .address = (uint32_t)infrared_worker_get_decoded_signal }, /* infrared_worker_get_decoded_signal */
    { .hash = 0xeb24977d, .address = (uint32_t)cli_shell_line_get_editing }, /* cli_shell_line_get_editing */
    { .hash = 0xeb352f76, .address = (uint32_t)canvas_draw_box }, /* canvas_draw_box */
    { .hash = 0xeb3537f4, .address = (uint32_t)canvas_draw_dot }, /* canvas_draw_dot */
    { .hash = 0xeb357866, .address = (uint32_t)canvas_draw_str }, /* canvas_draw_str */
    { .hash = 0xeb358b54, .address = (uint32_t)canvas_draw_xbm }, /* canvas_draw_xbm */
    { .hash = 0xec3e8481, .address = (uint32_t)storage_common_exists }, /* storage_common_exists */
    { .hash = 0xec6856b3, .address = (uint32_t)iso14443_3a_poller_send_standard_frame }, /* iso14443_3a_poller_send_standard_frame */
    { .hash = 0xec68dc5e, .address = (uint32_t)furi_log_set_level }, /* furi_log_set_level */
    { .hash = 0xec744a3b, .address = (uint32_t)menu_free }, /* menu_free */
    { .hash = 0xecf71c74, .address = (uint32_t)subghz_protocol_blocks_add_bytes }, /* subghz_protocol_blocks_add_bytes */
    { .hash = 0xed24309b, .address = (uint32_t)storage_simply_remove_recursive }, /* storage_simply_remove_recursive */
    { .hash = 0xed2ad9c6, .address = (uint32_t)nfc_poller_trx }, /* nfc_poller_trx */
    { .hash = 0xed39c32e, .address = (uint32_t)dialog_file_browser_set_basic_options }, /* dialog_file_browser_set_basic_options */
    { .hash = 0xed7500ce, .address = (uint32_t)mjs_return }, /* mjs_return */
    { .hash = 0xed8fbe8d, .address = (uint32_t)elements_bubble }, /* elements_bubble */
    { .hash = 0xed9a2eb2, .address = (uint32_t)&I_Scanning_123x52 }, /* I_Scanning_123x52 */
    { .hash = 0xedbc55d2, .address = (uint32_t)flipper_format_update_string_cstr }, /* flipper_format_update_string_cstr */
    { .hash = 0xee296313, .address = (uint32_t)stream_read_line }, /* stream_read_line */
    { .hash = 0xee3b804f, .address = (uint32_t)lwip_setsockopt }, /* lwip_setsockopt */
    { .hash = 0xee3e8f65, .address = (uint32_t)keys_dict_free }, /* keys_dict_free */
    { .hash = 0xee3ee06b, .address = (uint32_t)stream_copy }, /* stream_copy */
    { .hash = 0xee4090d2, .address = (uint32_t)stream_free }, /* stream_free */
    { .hash = 0xee46ed8c, .address = (uint32_t)stream_read }, /* stream_read */
    { .hash = 0xee477a78, .address = (uint32_t)stream_seek }, /* stream_seek */
    { .hash = 0xee478e2b, .address = (uint32_t)stream_size }, /* stream_size */
    { .hash = 0xee4807c1, .address = (uint32_t)stream_tell }, /* stream_tell */
    { .hash = 0xee62b92c, .address = (uint32_t)furi_hal_spi_bus_rx }, /* furi_hal_spi_bus_rx */
    { .hash = 0xee62b96e, .address = (uint32_t)furi_hal_spi_bus_tx }, /* furi_hal_spi_bus_tx */
    { .hash = 0xee7243cc, .address = (uint32_t)icon_get_width }, /* icon_get_width */
    { .hash = 0xee758416, .address = (uint32_t)esp_netif_get_handle_from_ifkey }, /* esp_netif_get_handle_from_ifkey */
    { .hash = 0xee84d329, .address = (uint32_t)menu_get_style }, /* menu_get_style */
    { .hash = 0xeed66623, .address = (uint32_t)heap_caps_get_free_size }, /* heap_caps_get_free_size */
    { .hash = 0xef195c40, .address = (uint32_t)hex_char_to_uint8 }, /* hex_char_to_uint8 */
    { .hash = 0xef1a0cd3, .address = (uint32_t)text_box_set_font }, /* text_box_set_font */
    { .hash = 0xef2190e1, .address = (uint32_t)text_box_set_text }, /* text_box_set_text */
    { .hash = 0xef2a9297, .address = (uint32_t)furi_mutex_alloc }, /* furi_mutex_alloc */
    { .hash = 0xef3e3f1c, .address = (uint32_t)variable_item_list_add }, /* variable_item_list_add */
    { .hash = 0xef3e58d3, .address = (uint32_t)variable_item_list_get }, /* variable_item_list_get */
    { .hash = 0xef74328c, .address = (uint32_t)i2s_channel_init_std_mode }, /* i2s_channel_init_std_mode */
    { .hash = 0xf01e9016, .address = (uint32_t)button_menu_set_selected_item }, /* button_menu_set_selected_item */
    { .hash = 0xf0200e54, .address = (uint32_t)bit_buffer_get_parity }, /* bit_buffer_get_parity */
    { .hash = 0xf0345139, .address = (uint32_t)subghz_setting_get_preset_data_by_name }, /* subghz_setting_get_preset_data_by_name */
    { .hash = 0xf07241e3, .address = (uint32_t)getchar }, /* getchar */
    { .hash = 0xf09449f4, .address = (uint32_t)toupper }, /* toupper */
    { .hash = 0xf09f1c98, .address = (uint32_t)stream_write_char }, /* stream_write_char */
    { .hash = 0xf0a237ff, .address = (uint32_t)furi_timer_restart }, /* furi_timer_restart */
    { .hash = 0xf0cd00c0, .address = (uint32_t)furi_thread_list_get_or_insert }, /* furi_thread_list_get_or_insert */
    { .hash = 0xf0d83307, .address = (uint32_t)mjs_strcmp }, /* mjs_strcmp */
    { .hash = 0xf0e4078a, .address = (uint32_t)composite_api_resolver_add }, /* composite_api_resolver_add */
    { .hash = 0xf0e42141, .address = (uint32_t)composite_api_resolver_get }, /* composite_api_resolver_get */
    { .hash = 0xf11bc8a4, .address = (uint32_t)subghz_transmitter_deserialize }, /* subghz_transmitter_deserialize */
    { .hash = 0xf125965c, .address = (uint32_t)mbedtls_sha1 }, /* mbedtls_sha1 */
    { .hash = 0xf20d48ea, .address = (uint32_t)elements_progress_bar }, /* elements_progress_bar */
    { .hash = 0xf24925f1, .address = (uint32_t)furi_event_loop_thread_flag_callback }, /* furi_event_loop_thread_flag_callback */
    { .hash = 0xf28769b3, .address = (uint32_t)ipaddr_addr }, /* ipaddr_addr */
    { .hash = 0xf28d8fc1, .address = (uint32_t)atan2f }, /* atan2f */
    { .hash = 0xf28ff2f4, .address = (uint32_t)atexit }, /* atexit */
    { .hash = 0xf2da67ac, .address = (uint32_t)furi_pubsub_subscribe }, /* furi_pubsub_subscribe */
    { .hash = 0xf3b3f475, .address = (uint32_t)args_char_to_hex }, /* args_char_to_hex */
    { .hash = 0xf3c1459c, .address = (uint32_t)&ble_profile_hid }, /* ble_profile_hid */
    { .hash = 0xf4296c90, .address = (uint32_t)gui_add_view_port }, /* gui_add_view_port */
    { .hash = 0xf4613aa7, .address = (uint32_t)path_extract_basename }, /* path_extract_basename */
    { .hash = 0xf4738880, .address = (uint32_t)subghz_worker_set_context }, /* subghz_worker_set_context */
    { .hash = 0xf4a81c24, .address = (uint32_t)subghz_protocol_blocks_crc16 }, /* subghz_protocol_blocks_crc16 */
    { .hash = 0xf4c9bfe6, .address = (uint32_t)flipper_format_insert_or_update_string_cstr }, /* flipper_format_insert_or_update_string_cstr */
    { .hash = 0xf4d39c2d, .address = (uint32_t)iso15693_3_get_block_size }, /* iso15693_3_get_block_size */
    { .hash = 0xf567b2bd, .address = (uint32_t)iso14443_3a_copy }, /* iso14443_3a_copy */
    { .hash = 0xf5696324, .address = (uint32_t)iso14443_3a_free }, /* iso14443_3a_free */
    { .hash = 0xf580f8d3, .address = (uint32_t)&I_Ok_btn_9x9 }, /* I_Ok_btn_9x9 */
    { .hash = 0xf59b722e, .address = (uint32_t)view_port_set_width }, /* view_port_set_width */
    { .hash = 0xf5cbc8a5, .address = (uint32_t)cli_registry_delete_command }, /* cli_registry_delete_command */
    { .hash = 0xf5e616f3, .address = (uint32_t)calloc }, /* calloc */
    { .hash = 0xf64d7879, .address = (uint32_t)mjs_get_bool }, /* mjs_get_bool */
    { .hash = 0xf659a5a4, .address = (uint32_t)cli_shell_line_format_prompt }, /* cli_shell_line_format_prompt */
    { .hash = 0xf696f6d9, .address = (uint32_t)furi_string_set_char }, /* furi_string_set_char */
    { .hash = 0xf69ff222, .address = (uint32_t)furi_string_set_strn }, /* furi_string_set_strn */
    { .hash = 0xf743cca9, .address = (uint32_t)mjs_set_errorf }, /* mjs_set_errorf */
    { .hash = 0xf7d65020, .address = (uint32_t)furi_string_push_back }, /* furi_string_push_back */
    { .hash = 0xf808955b, .address = (uint32_t)__udivdi3 }, /* __udivdi3 */
    { .hash = 0xf8382627, .address = (uint32_t)memmgr_get_free_heap }, /* memmgr_get_free_heap */
    { .hash = 0xf8527efe, .address = (uint32_t)furi_hal_infrared_async_tx_wait_termination }, /* furi_hal_infrared_async_tx_wait_termination */
    { .hash = 0xf880a84e, .address = (uint32_t)elements_scrollbar_horizontal }, /* elements_scrollbar_horizontal */
    { .hash = 0xf8899db0, .address = (uint32_t)flipper_format_read_string }, /* flipper_format_read_string */
    { .hash = 0xf927c5ad, .address = (uint32_t)variable_item_get_current_value_index }, /* variable_item_get_current_value_index */
    { .hash = 0xf95f7860, .address = (uint32_t)subghz_setting_load_custom_preset }, /* subghz_setting_load_custom_preset */
    { .hash = 0xf974e74c, .address = (uint32_t)furi_hal_subghz_load_custom_preset }, /* furi_hal_subghz_load_custom_preset */
    { .hash = 0xf9d50ef0, .address = (uint32_t)version_set_custom_name }, /* version_set_custom_name */
    { .hash = 0xfa1248bf, .address = (uint32_t)furi_thread_get_current_id }, /* furi_thread_get_current_id */
    { .hash = 0xfa6b18ae, .address = (uint32_t)view_port_alloc }, /* view_port_alloc */
    { .hash = 0xfa87b7d2, .address = (uint32_t)&I_Button_18x18 }, /* I_Button_18x18 */
    { .hash = 0xfacb9ae5, .address = (uint32_t)submenu_set_selected_item }, /* submenu_set_selected_item */
    { .hash = 0xfaf5ceb4, .address = (uint32_t)furi_timer_get_expire_time }, /* furi_timer_get_expire_time */
    { .hash = 0xfb257acc, .address = (uint32_t)furi_stream_buffer_is_full }, /* furi_stream_buffer_is_full */
    { .hash = 0xfb30e01f, .address = (uint32_t)simple_array_is_equal }, /* simple_array_is_equal */
    { .hash = 0xfb46fcd5, .address = (uint32_t)subghz_devices_is_frequency_valid }, /* subghz_devices_is_frequency_valid */
    { .hash = 0xfb769098, .address = (uint32_t)path_extract_filename_no_ext }, /* path_extract_filename_no_ext */
    { .hash = 0xfbada23e, .address = (uint32_t)subghz_devices_is_connect }, /* subghz_devices_is_connect */
    { .hash = 0xfbd7a5eb, .address = (uint32_t)subghz_devices_load_preset }, /* subghz_devices_load_preset */
    { .hash = 0xfbf1563c, .address = (uint32_t)compress_icon_decode }, /* compress_icon_decode */
    { .hash = 0xfbfdcf2a, .address = (uint32_t)furi_hal_power_enable_otg }, /* furi_hal_power_enable_otg */
    { .hash = 0xfc04d968, .address = (uint32_t)i2c_param_config }, /* i2c_param_config */
    { .hash = 0xfc1e7ca2, .address = (uint32_t)name_generator_make_auto_basic }, /* name_generator_make_auto_basic */
    { .hash = 0xfc6ad6de, .address = (uint32_t)flipper_format_read_uint32 }, /* flipper_format_read_uint32 */
    { .hash = 0xfcd24db7, .address = (uint32_t)version_get_builddate }, /* version_get_builddate */
    { .hash = 0xfce657cc, .address = (uint32_t)path_extract_filename }, /* path_extract_filename */
    { .hash = 0xfd09cf21, .address = (uint32_t)fclose }, /* fclose */
    { .hash = 0xfd40322d, .address = (uint32_t)fflush }, /* fflush */
    { .hash = 0xfd50a848, .address = (uint32_t)button_menu_set_header }, /* button_menu_set_header */
    { .hash = 0xfdf5a8c7, .address = (uint32_t)view_dispatcher_free }, /* view_dispatcher_free */
    { .hash = 0xfdfcd38b, .address = (uint32_t)view_dispatcher_stop }, /* view_dispatcher_stop */
    { .hash = 0xfe2b16b2, .address = (uint32_t)mjs_get_context }, /* mjs_get_context */
    { .hash = 0xfe65dcb3, .address = (uint32_t)mjs_is_foreign }, /* mjs_is_foreign */
    { .hash = 0xfe76ea16, .address = (uint32_t)fwrite }, /* fwrite */
    { .hash = 0xfe778b9a, .address = (uint32_t)dir_walk_get_error }, /* dir_walk_get_error */
    { .hash = 0xfe7abcd4, .address = (uint32_t)flipper_format_write_bool }, /* flipper_format_write_bool */
    { .hash = 0xfe93dbd7, .address = (uint32_t)esp_wifi_deinit }, /* esp_wifi_deinit */
    { .hash = 0xfea5c5ed, .address = (uint32_t)subghz_devices_start_async_rx }, /* subghz_devices_start_async_rx */
    { .hash = 0xfea5c62f, .address = (uint32_t)subghz_devices_start_async_tx }, /* subghz_devices_start_async_tx */
    { .hash = 0xff665376, .address = (uint32_t)flipper_format_write_hex_uint64 }, /* flipper_format_write_hex_uint64 */
    { .hash = 0xff7edc8f, .address = (uint32_t)strint_to_uint32 }, /* strint_to_uint32 */
    { .hash = 0xff8760ae, .address = (uint32_t)getenv }, /* getenv */
    { .hash = 0xffc84289, .address = (uint32_t)night_shift_timer_start }, /* night_shift_timer_start */
};
/* clang-format on */

static const HashtableApiInterface firmware_api_impl = {
    .base =
        {
            .api_version_major = 1,
            .api_version_minor = 0,
            .resolver_callback = elf_resolve_from_hashtable,
        },
    .table_begin = firmware_api_table,
    .table_end = firmware_api_table + (sizeof(firmware_api_table) / sizeof(firmware_api_table[0])),
};

const ElfApiInterface* const firmware_api_interface = &firmware_api_impl.base;
