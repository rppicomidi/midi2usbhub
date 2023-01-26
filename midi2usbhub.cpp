/**
 * @file Pico-USB-Host-MIDI-Adapter.c
 * @brief A USB Host to Serial Port MIDI adapter that runs on a Raspberry Pi
 * Pico board
 *
 * MIT License

 * Copyright (c) 2022 rppicomidi

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <cstdio>
#include <vector>
#include <cstdint>
#include <string>
#include "midi2usbhub.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pio_midi_uart_lib.h"
#include "bsp/board.h"
#include "preset_manager.h"
#include "diskio.h"
#include "hid_keyboard.h"
#ifdef RPPICOMIDI_PICO_W
#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/fs.h"
#include "main_lwipopts.h"
#endif
uint16_t rppicomidi::Midi2usbhub::render_done_mask = 0;

JSON_Value* rppicomidi::Midi2usbhub::serialize_to_json()
{
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    JSON_Value *from_value = json_value_init_object();
    JSON_Object *from_object = json_value_get_object(from_value);
    JSON_Value *to_value = json_value_init_object();
    JSON_Object *to_object = json_value_get_object(to_value);
    for (auto &midi_in : midi_in_port_list)
    {
        std::string default_nickname;
        make_default_nickname(default_nickname, attached_devices[midi_in->devaddr].vid, attached_devices[midi_in->devaddr].pid, midi_in->cable, true);
        json_object_set_string(from_object, default_nickname.c_str(), midi_in->nickname.c_str());
    }
    for (auto &midi_out : midi_out_port_list)
    {
        std::string default_nickname;
        make_default_nickname(default_nickname, attached_devices[midi_out->devaddr].vid, attached_devices[midi_out->devaddr].pid, midi_out->cable, false);
        json_object_set_string(to_object, default_nickname.c_str(), midi_out->nickname.c_str());
    }
    json_object_set_value(root_object, "from", from_value);
    json_object_set_value(root_object, "to", to_value);

    JSON_Value *routing_value = json_value_init_object();
    JSON_Object *routing_object = json_value_get_object(routing_value);
    for (auto &midi_in : midi_in_port_list)
    {
        JSON_Value *routing = json_value_init_array();
        JSON_Array *routing_array = json_value_get_array(routing);
        for (auto &midi_out : midi_in->sends_data_to_list)
        {
            json_array_append_string(routing_array, midi_out->nickname.c_str());
        }
        json_object_set_value(routing_object, midi_in->nickname.c_str(), json_array_get_wrapping_value(routing_array));
    }
    json_object_set_value(root_object, "routing", routing_value);
    return root_value;
}
void rppicomidi::Midi2usbhub::serialize(std::string &serialized_string)
{
    JSON_Value *root_value = serialize_to_json();
    auto ser = json_serialize_to_string(root_value);
    serialized_string = std::string(ser);
    json_free_serialized_string(ser);
    json_value_free(root_value);
}

bool rppicomidi::Midi2usbhub::deserialize(const std::string &serialized_string)
{
    JSON_Value* root_value = json_parse_string(serialized_string.c_str());
    if (root_value == nullptr) {
        return false;
    }
    JSON_Object* root_object = json_value_get_object(root_value);
    JSON_Value* midi_in_nicknames_value = json_object_get_value(root_object, "from");
    if (midi_in_nicknames_value == nullptr) {
        json_value_free(root_value);
        return false;
    }
    JSON_Object* midi_in_nicknames_object = json_value_get_object(midi_in_nicknames_value);
    if (midi_in_nicknames_object) {
        // update the nicknames
        for (auto& midi_in: midi_in_port_list) {
            std::string def_nickname;
            auto info = &attached_devices[midi_in->devaddr];
            make_default_nickname(def_nickname, info->vid, info->pid, midi_in->cable, true);
            const char* nickname = json_object_get_string(midi_in_nicknames_object, def_nickname.c_str());
            if (nickname) {
                midi_in->nickname = std::string(nickname);
            }
        }
    }
    else {
        json_value_free(root_value);
        return false;
    }
    JSON_Value* midi_out_nicknames_value = json_object_get_value(root_object, "to");
    if (midi_out_nicknames_value == nullptr) {
        json_value_free(root_value);
        return false;
    }
    JSON_Object* midi_out_nicknames_object = json_value_get_object(midi_out_nicknames_value);
    if (midi_out_nicknames_object) {
        // update the nicknames
        for (auto& midi_out: midi_out_port_list) {
            std::string def_nickname;
            auto info = &attached_devices[midi_out->devaddr];
            make_default_nickname(def_nickname, info->vid, info->pid, midi_out->cable, false);
            const char* nickname = json_object_get_string(midi_out_nicknames_object, def_nickname.c_str());
            if (nickname) {
                midi_out->nickname = std::string(nickname);
            }
            else {
                printf("could not find nickname %s\r\n", def_nickname.c_str());
            }
        }
    }
    else {
        json_value_free(root_value);
        return false;
    }
    JSON_Value* routing_value = json_object_get_value(root_object, "routing");
    if (routing_value == nullptr) {
        json_value_free(root_value);
        return false;
    }
    JSON_Object* routing_object = json_value_get_object(routing_value);
    if (routing_object) {
        for (auto& midi_in: midi_in_port_list) {
            JSON_Array* routes = json_object_get_array(routing_object, midi_in->nickname.c_str());
            if (routes) {
                midi_in->sends_data_to_list.clear();
                size_t count = json_array_get_count(routes);
                for (size_t idx = 0; idx < count; idx++) {
                    const char* to_nickname = json_array_get_string(routes, idx);
                    if (to_nickname) {
                        std::string nickname = std::string(to_nickname);
                        // Find to_nickname in the midi_out_port_list
                        for (auto& midi_out: midi_out_port_list ) {
                            if (nickname == midi_out->nickname) {
                                // it's connected, so route it
                                midi_in->sends_data_to_list.push_back(midi_out);
                                break;
                            }
                        }
                    }
                    else {
                        // either
                        // poorly formatted JSON
                        // or just plugged in a new device not in the preset; ignore it
                        //json_value_free(root_value);
                        //return false;
                    }
                }
            }
            else {
                // either
                // poorly formatted JSON
                // or just plugged in a new device not in the preset; ignore it
                //json_value_free(root_value);
                //return false;
            }
        }
    }
    else {
        // poorly formatted JSON
        json_value_free(root_value);
        return false;
    }
    json_value_free(root_value);
    return true;
}


int rppicomidi::Midi2usbhub::connect(const std::string& from_nickname, const std::string& to_nickname)
{
    for (auto &in_port : midi_in_port_list) {
        if (in_port->nickname == from_nickname) {
            for (auto out_port : midi_out_port_list) {
                if (out_port->nickname == to_nickname) {
                    in_port->sends_data_to_list.push_back(out_port);
                    update_json_connected_state();
                    return 0;
                }
            }
           return -1;
        }
    }
    return -2;
}

int rppicomidi::Midi2usbhub::disconnect(const std::string& from_nickname, const std::string& to_nickname)
{
    for (auto &in_port : midi_in_port_list) {
        if (in_port->nickname == from_nickname) {
            for (auto it = in_port->sends_data_to_list.begin(); it != in_port->sends_data_to_list.end();) {
                if ((*it)->nickname == to_nickname) {
                    in_port->sends_data_to_list.erase(it);
                    update_json_connected_state();
                    return 0;
                }
                else {
                    ++it;
                }
            }
            return -1;
        }
    }
    return -2;
}

void rppicomidi::Midi2usbhub::reset()
{
    for (auto &in_port :midi_in_port_list) {
        in_port->sends_data_to_list.clear();
    }
    update_json_connected_state();
}

int rppicomidi::Midi2usbhub::rename(const std::string& old_nickname, const std::string& new_nickname)
{
    // make sure the new nickname is not already in use
    for (auto midi_in : midi_in_port_list) {
        if (midi_in->nickname == new_nickname) {
            return -1;
        }
    }
    for (auto midi_out : midi_out_port_list) {
        if (midi_out->nickname == new_nickname) {
            return -1;
        }
    }
    for (auto &midi_in : midi_in_port_list) {
        if (midi_in->nickname == old_nickname) {
            midi_in->nickname = new_nickname;
            update_json_connected_state();
            return 1;
        }
    }
    for (auto &midi_out : midi_out_port_list) {
        if (midi_out->nickname == old_nickname) {
            midi_out->nickname = new_nickname;
            update_json_connected_state();
            return 2;
        }
    }
    return -2;
}

void rppicomidi::Midi2usbhub::blink_led()
{
    static absolute_time_t previous_timestamp = {0};

    static bool led_state = false;

    // This design has no on-board LED
    if (NO_LED_GPIO == LED_GPIO)
        return;
    absolute_time_t now = get_absolute_time();

    int64_t diff = absolute_time_diff_us(previous_timestamp, now);
    if (diff > 1000000) {
        #ifndef RPPICOMIDI_PICO_W
        gpio_put(LED_GPIO, led_state);
        #else
        if (wifi.get_state() != Pico_w_connection_manager::DEINITIALIZED)
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
        #endif
        led_state = !led_state;
        previous_timestamp = now;
    }
}

void rppicomidi::Midi2usbhub::poll_usb_rx()
{
    for (auto &in_port : midi_in_port_list) {
        // poll each connected MIDI IN port only once per device address
        if (in_port->devaddr != uart_devaddr && in_port->cable == 0 && tuh_midi_configured(in_port->devaddr))
            tuh_midi_read_poll(in_port->devaddr);
    }
}

void rppicomidi::Midi2usbhub::flush_usb_tx()
{
    for (auto &out_port : midi_out_port_list)
    {
        // Call tuh_midi_stream_flush() once per output port device address
        if (out_port->devaddr != uart_devaddr &&
            out_port->cable == 0 &&
            tuh_midi_configured(out_port->devaddr))
        {
            tuh_midi_stream_flush(out_port->devaddr);
        }
    }
}

void rppicomidi::Midi2usbhub::poll_midi_uart_rx(uint8_t port)
{
    uint8_t rx[48];
    // Pull any bytes received on the MIDI UART out of the receive buffer and
    // send them out via USB MIDI on virtual cable 0
    uint8_t nread = pio_midi_uart_poll_rx_buffer(midi_uarts[port], rx, sizeof(rx));
    if (nread > 0)
    {
        // figure out where to send data from UART MIDI IN
        for (auto &out_port : uart_midi_in_port[port].sends_data_to_list)
        {
            if (out_port->devaddr != 0 && attached_devices[out_port->devaddr].configured)
            {
                uint32_t nwritten = tuh_midi_stream_write(out_port->devaddr, out_port->cable, rx, nread);
                if (nwritten != nread)
                {
                    TU_LOG1("Warning: Dropped %lu bytes receiving from UART MIDI In\r\n", nread - nwritten);
                }
            }
        }
    }
}

rppicomidi::Midi2usbhub::Midi2usbhub() : current_connection{nullptr}, cli{&preset_manager, &wifi}, 
    addr{OLED_ADDR},
    ssd1306{&i2c_driver_oled, 0, Ssd1306::Com_pin_cfg::ALT_DIS, 128, 64, 0, 0}, // set up the SSD1306 to drive at 128 x 64 oled
    oled_screen{&ssd1306, Display_rotation::Landscape180}, home_screen{oled_screen, &wifi, &preset_manager, &oled_view_manager}
{
    bi_decl(bi_program_description("Provide a USB host interface for Serial Port MIDI."));
    bi_decl(bi_1pin_with_name(LED_GPIO, "On-board LED"));
    bi_decl(bi_2pins_with_names(MIDI_UART_A_TX_GPIO, "MIDI UART TX", MIDI_UART_A_RX_GPIO, "MIDI UART RX"));
    board_init();
    printf("Pico MIDI Host to MIDI UART Adapter\r\n");
    tusb_init();
    // Initialize the POST command list
    post_cmd_list.push_back({{2,{'c','o','n','\0'},{'\0'}, {'\0'}}, static_connect_cmd}); // connect from_nickname to_nickname
    post_cmd_list.push_back({{2,{'d','i','s','\0'},{'\0'}, {'\0'}}, static_disconnect_cmd}); // disconnect from_nickname to_nickname
    post_cmd_list.push_back({{2,{'r','e','n','\0'},{'\0'}, {'\0'}}, static_rename_cmd}); // rename old_nickname new_nickname
    post_cmd_list.push_back({{2,{'m','v',' ','\0'},{'\0'}, {'\0'}}, static_mv_cmd}); // rename old_preset_filename new_preset_filename
    post_cmd_list.push_back({{0,{'r','s','t','\0'},{'\0'}, {'\0'}}, static_reset_cmd}); // reset routing
    post_cmd_list.push_back({{1,{'l','o','d','\0'},{'\0'}, {'\0'}}, static_load_cmd}); // load preset
    post_cmd_list.push_back({{1,{'s','a','v','\0'},{'\0'}, {'\0'}}, static_save_cmd}); // save preset
    post_cmd_list.push_back({{1,{'d','e','l','\0'},{'\0'}, {'\0'}}, static_delete_cmd}); // delete preset
    // Map the pins to functions
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
    midi_uarts[0] = pio_midi_uart_create(MIDI_UART_A_TX_GPIO, MIDI_UART_A_RX_GPIO);
    midi_uarts[1] = pio_midi_uart_create(MIDI_UART_B_TX_GPIO, MIDI_UART_B_RX_GPIO);
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT)
    {
        // flush out the console input buffer
    }
    char in_name[] = "MIDI IN A";
    char out_name[] = "MIDI OUT A";
    for (uint8_t port=0; port < num_midi_uarts; port++) {
        in_name[strlen(in_name)-1] = 'A' + port;
        out_name[strlen(out_name)-1] = 'A' + port;
        uart_midi_in_port[port].cable = port;
        uart_midi_in_port[port].devaddr = uart_devaddr;
        uart_midi_in_port[port].sends_data_to_list.clear();
        uart_midi_in_port[port].nickname = in_name;
        uart_midi_out_port[port].cable = port;
        uart_midi_out_port[port].devaddr = uart_devaddr;
        uart_midi_out_port[port].nickname = out_name;
        midi_in_port_list.push_back(uart_midi_in_port + port);
        midi_out_port_list.push_back(uart_midi_out_port + port);
    }
    attached_devices[uart_devaddr].vid = 0;
    attached_devices[uart_devaddr].pid = 0;
    attached_devices[uart_devaddr].product_name = "UART MIDI";
    attached_devices[uart_devaddr].rx_cables = num_midi_uarts;
    attached_devices[uart_devaddr].tx_cables = num_midi_uarts;
    attached_devices[uart_devaddr].configured = true;

    render_done_mask = 0;
    int num_displays = 1;
    uint16_t target_done_mask = (1<<(num_displays)) -1;
    bool success = true;
    oled_view_manager.push_view(&home_screen);
    oled_screen.render_non_blocking(callback, 0);
    while (success && render_done_mask != target_done_mask) {
        if (success) {
            success = oled_screen.task();
        }
    }
    assert(success);
    Hid_keyboard::instance().set_view_manager(&oled_view_manager);
    printf("Cli is running.\r\n");
    printf("Type \"help\" for a list of commands\r\n");
    printf("Use backspace and tab to remove chars and autocomplete\r\n");
    printf("Use up and down arrows to recall previous commands\r\n");
}

#if 0
// the following utf conversion and print code comes from tinyusb example code, copyright Ha Thach 2019 (tinyusb.org)
//--------------------------------------------------------------------+
// String Descriptor Helper
//--------------------------------------------------------------------+

static void _convert_utf16le_to_utf8(const uint16_t *utf16, size_t utf16_len, uint8_t *utf8, size_t utf8_len) {
    // TODO: Check for runover.
    (void)utf8_len;
    // Get the UTF-16 length out of the data itself.

    for (size_t i = 0; i < utf16_len; i++) {
        uint16_t chr = utf16[i];
        if (chr < 0x80) {
            *utf8++ = chr & 0xffu;
        } else if (chr < 0x800) {
            *utf8++ = (uint8_t)(0xC0 | (chr >> 6 & 0x1F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        } else {
            // TODO: Verify surrogate.
            *utf8++ = (uint8_t)(0xE0 | (chr >> 12 & 0x0F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 6 & 0x3F));
            *utf8++ = (uint8_t)(0x80 | (chr >> 0 & 0x3F));
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
}

// Count how many bytes a utf-16-le encoded string will take in utf-8.
static int _count_utf8_bytes(const uint16_t *buf, size_t len) {
    size_t total_bytes = 0;
    for (size_t i = 0; i < len; i++) {
        uint16_t chr = buf[i];
        if (chr < 0x80) {
            total_bytes += 1;
        } else if (chr < 0x800) {
            total_bytes += 2;
        } else {
            total_bytes += 3;
        }
        // TODO: Handle UTF-16 code points that take two entries.
    }
    return (int) total_bytes;
}

static void print_utf16(uint16_t *temp_buf, size_t buf_len) {
    size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
    size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
    _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t *) temp_buf, sizeof(uint16_t) * buf_len);
    ((uint8_t*) temp_buf)[utf8_len] = '\0';

    printf((char*)temp_buf);
}
#endif

void rppicomidi::Midi2usbhub::task()
{
    tuh_task();

    blink_led();

    for (uint8_t port=0; port < num_midi_uarts; port++) {
        poll_midi_uart_rx(port);
    }
    flush_usb_tx();
    poll_usb_rx();
    for (uint8_t port=0; port < num_midi_uarts; port++) {
        pio_midi_uart_drain_tx_buffer(midi_uarts[port]);
    }
#ifdef RPPICOMIDI_PICO_W
    wifi.task();
    process_pending_cmds();
#endif
    cli.task();
    if (oled_screen.can_render()) {
        oled_screen.render_non_blocking(nullptr, 0);
    }
    oled_screen.task();
}

void rppicomidi::Midi2usbhub::get_connected()
{
    bool connected = false;
    // If successfully loaded settings, attempt to autoconnect now.
    if (wifi.load_settings()) {
        std::string ssid;
        wifi.get_current_ssid(ssid);
        if (ssid.size() > 0) {
            wifi.autoconnect();
            if (oled_view_manager.get_current_view() == &home_screen) {
                home_screen.update_ipaddr_menu_item();
                home_screen.draw();
            }
        }
    }
    while (!connected) {
        connected = wifi.get_state() == wifi.CONNECTED;
        task();
    }
}

static void make_ajax_response_file_data(struct fs_file *file, const char* result, const char* content)
{
#if 1
    static char data[2048];
    size_t content_len = strlen(content);
    file->len = snprintf(data, sizeof(data), "HTTP/1.0 %s\r\nContent-Type: application/json;charset=UTF-8\r\nContent-Length: %d+\r\n\r\n%s", result, content_len, content);
    if ((size_t)file->len >= sizeof(data)) {
        printf("make_ajax_response_file_data: response truncated\r\n");
    }
#else
    // don't allocate memory in an irq
    std::string content_len = std::to_string(strlen(content) + 1);
    std::string file_str = std::string("HTTP/1.0 ")+std::string(result) +
            std::string("\nContent-Type: application/json\nContent-Length: ") +
                content_len+std::string("\n\n") + std::string(content);
    file->len = file_str.length();
    char* data = new char[file->len];
    strncpy(data, file_str.c_str(), file->len);
    data[file->len] = '\0';
#endif
    file->data = data;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_INCLUDED | FS_FILE_FLAGS_CUSTOM;
}

// Required by LWIP_HTTPD_CUSTOM_FILES
int fs_open_custom(struct fs_file *file, const char *name)
{
    cyw43_arch_lwip_begin();
    int result = 0;
    const char* OK_200 = "200 OK";
    const char* Created_201 = "201 Created";
    const char* url = "/connected_state.json";
    if (strncmp(url, name, strlen(url)) == 0) {
        make_ajax_response_file_data(file, OK_200, rppicomidi::Midi2usbhub::instance().get_json_connected_state());
        result = 1;
    }
    else {
        url = "/post_cmd.json";
        static const char* json_result_ok = "{\"result\":\"OK\"}";
        if (strncmp(url, name, strlen(url)) == 0) {
            printf("response = %s\r\n", json_result_ok);
            make_ajax_response_file_data(file, Created_201, json_result_ok);
            result = 1;
        }
    }
    cyw43_arch_lwip_end();
    return result;
}

void fs_close_custom(struct fs_file *file)
{
    // Nothing to do here
    LWIP_UNUSED_ARG(file);
}

err_t rppicomidi::Midi2usbhub::post_begin(void *connection, const char *uri, const char *http_request, u16_t http_request_len,
                            int content_len, char *response_uri, u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    //printf("Got POST message %u bytes content %d bytes\r\n", http_request_len, content_len);
    //hexdump(http_request, http_request_len);
    if (!memcmp(uri, "/post_cmd.txt", strlen("/post_cmd.txt"))) {
        if (current_connection != NULL) {
            last_post_error_connection = connection;
            snprintf(response_uri, response_uri_len, "/429.html");
        }
        else {
            current_connection = connection;
            last_post_error_connection = NULL;
            /* default page is 400 Bad Request */
            snprintf(response_uri, response_uri_len, "/400.html");
            /* e.g. for large uploads to slow flash over a fast connection, you should
                manually update the rx window. That way, a sender can only send a full
                tcp window at a time. If this is required, set 'post_aut_wnd' to 0.
                We do not need to throttle upload speed here, so: */
            *post_auto_wnd = 1;
            return ERR_OK;
        }
    }
    else {
        printf("bad POST URI=%s\r\n", uri);
        last_post_error_connection = connection;
    }
    return ERR_VAL;
}

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request, u16_t http_request_len,
                            int content_len, char *response_uri, u16_t response_uri_len, u8_t *post_auto_wnd)
{
    return rppicomidi::Midi2usbhub::instance().post_begin(connection, uri, http_request, http_request_len, content_len, response_uri, response_uri_len, post_auto_wnd);
}

err_t rppicomidi::Midi2usbhub::post_receive_data(void *connection, struct pbuf *p)
{
    cyw43_arch_lwip_begin();
    err_t result = ERR_VAL;
    if (connection == current_connection) {
        char data[p->tot_len];
        char* buffer = static_cast<char*>(pbuf_get_contiguous(p, data, p->tot_len, p->tot_len, 0));
        // the buffer must contain data that fits in a Post_command structure
        if (buffer != NULL && p->tot_len < (max_arg_len * max_args)*2+cmd_len) {
            // parse out the command and the arguments
            char temp[(max_arg_len * max_args)*2+cmd_len];
            Post_cmd cmd;
            memcpy(temp, buffer, p->tot_len);
            temp[p->tot_len] = '\0';
            char* ptr = strtok(temp, "?");
            if (ptr && strlen(ptr) == cmd_len-1u) {
                strcpy(cmd.cmd, ptr);
                cmd.nargs = 0;
                ptr = strtok(NULL, "?");
                if (ptr && strlen(ptr) < max_arg_len) {
                    cmd.nargs = 1;
                    strcpy(cmd.arg0, ptr);
                    ptr = strtok(NULL, "?");
                    if (ptr && strlen(ptr) < max_arg_len) {
                        cmd.nargs = 2;
                        strcpy(cmd.arg1, ptr);
                        ptr = strtok(NULL, "?");
                        if (ptr != NULL) {
                            // oops, no command should have 3 arguments make sure push_cmd fails
                            cmd.cmd[0] = '\0';
                        }
                    }
                }
                if (push_cmd(cmd)) {
                    // command can be dispatched and there is space in the pending command list
                    result = ERR_OK;
                }
            }
        }
    }
    if (result != ERR_OK) {
        printf("failed to process the POST data\r\n");
        last_post_error_connection = connection;
    }
    pbuf_free(p);
    cyw43_arch_lwip_end();
    return result;

}
err_t httpd_post_receive_data (void *connection, struct pbuf *p)
{
    return rppicomidi::Midi2usbhub::instance().post_receive_data(connection, p);
}

void rppicomidi::Midi2usbhub::post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    if (connection == current_connection) {
        if (last_post_error_connection == NULL) {
            snprintf(response_uri, response_uri_len, "/post_cmd.json");
        }
        else {
            // The code should have a respose file name in the response_uri, except that LwIP trashed it
            // Put it back
            response_uri[0]='/';
        }
        current_connection = NULL;
        last_post_error_connection = NULL;
    }
}

void httpd_post_finished (void *connection, char *response_uri, u16_t response_uri_len)
{
    rppicomidi::Midi2usbhub::instance().post_finished(connection, response_uri, response_uri_len);
}

// Main loop
int main()
{
    rppicomidi::Midi2usbhub &instance = rppicomidi::Midi2usbhub::instance();
#ifdef RPPICOMIDI_PICO_W
    instance.get_connected();
    httpd_init();
#endif
    while (1) {
        instance.task();
    }
}

void rppicomidi::Midi2usbhub::make_default_nickname(std::string &nickname, uint16_t vid, uint16_t pid, uint8_t cable, bool is_from)
{
    char default_nickname[17];
    snprintf(default_nickname, sizeof(default_nickname) - 1, "%04x-%04x%c%d",
             vid,
             pid,
             is_from ? 'F' : 'T',
             cable + 1);
    default_nickname[12] = '\0'; // limit to 12 characters.
    nickname = std::string(default_nickname);
}

void get_info_from_default_nickname(std::string nickname, uint16_t &vid, uint16_t &pid, uint8_t &cable, bool &is_from)
{
    vid = std::stoi(nickname.substr(0, 4), 0, 16);
    pid = std::stoi(nickname.substr(5, 4), 0, 16);
    cable = std::stoi(nickname.substr(10, std::string::npos));
    is_from = nickname.substr(9, 1) == "F";
}

void rppicomidi::Midi2usbhub::update_json_connected_state()
{
    JSON_Value *root_value = serialize_to_json();
    JSON_Object *root_object = json_value_get_object(root_value);    
    JSON_Value *attached_value = json_value_init_object();
    JSON_Object *attached_object = json_value_get_object(attached_value);
    for (size_t addr = 1; addr <= uart_devaddr; addr++) {
        auto dev = Midi2usbhub::instance().get_attached_device(addr);
        char usbid[10];
        if (dev && dev->configured) {
            snprintf(usbid, sizeof(usbid), "%04x-%04x", dev->vid, dev->pid);
            json_object_set_string(attached_object, usbid, dev->product_name.c_str());
        }
    }

    json_object_set_value(root_object, "attached", attached_value);
    json_object_set_string(root_object, "curpre", preset_manager.get_current_preset_name());
    json_object_set_boolean(root_object, "force", false); // always false from the webserver
    preset_manager.serialize_preset_list_to_json(root_object);
    auto ser = json_serialize_to_string(root_value);
    // temporarily disable wifi interrupts to prevent accessing
    // json_connected_state whilst it might be changing
    protect_from_lwip();
    json_connected_state = std::string(ser);
    unprotect_from_lwip();
    json_free_serialized_string(ser);
    json_value_free(root_value);
    //printf("\r\n%s\r\n",json_connected_state.c_str());
}

bool rppicomidi::Midi2usbhub::static_connect_cmd(Post_cmd& cmd) {
    bool success = false;
    if (strncmp("con", cmd.cmd, 3)==0 && cmd.nargs == 2) {
        success = instance().connect(std::string(cmd.arg0), std::string(cmd.arg1)) == 0;
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_disconnect_cmd(Post_cmd& cmd) {
    bool success = false;
    if (strncmp("dis", cmd.cmd, 3)==0 && cmd.nargs == 2) {
        success = instance().disconnect(std::string(cmd.arg0), std::string(cmd.arg1)) == 0;
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_mv_cmd(Post_cmd& cmd) {
    bool success = false;
    if (strncmp("ren", cmd.cmd, 3)==0 && cmd.nargs == 2) {
        std::string old_fn = std::string(cmd.arg0) + ".json";
        std::string new_fn = std::string(cmd.arg1) + ".json";
        success = instance().preset_manager.remame_preset(old_fn, new_fn);
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_rename_cmd(Post_cmd& cmd) {
    bool success = false;
    if (strncmp("ren", cmd.cmd, 3)==0 && cmd.nargs == 2) {
        std::string old_nn = std::string(cmd.arg0);
        std::string new_nn = std::string(cmd.arg1);
        success = instance().rename(old_nn, new_nn);
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_reset_cmd(Post_cmd& cmd) {
    bool success = false;
    if (strncmp("rst", cmd.cmd, 3)==0 && cmd.nargs == 0) {
        instance().reset();
        success = true;
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_load_cmd(Post_cmd& cmd)
{
    bool success = false;
    if (strncmp("lod", cmd.cmd, 3)==0 && cmd.nargs == 1 && strlen(cmd.arg0) <= 12 && strlen(cmd.arg0) > 0) {
        std::string fn = std::string(cmd.arg0) + ".json";
        success = instance().preset_manager.load_preset(fn);
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_save_cmd(Post_cmd& cmd)
{
    bool success = false;
    if (strncmp("sav", cmd.cmd, 3)==0 && cmd.nargs == 1 && strlen(cmd.arg0) <= 12 && strlen(cmd.arg0) > 0) {
        std::string fn = std::string(cmd.arg0) + ".json";
        success = instance().preset_manager.save_current_preset(fn);
    }
    return success;
}

bool rppicomidi::Midi2usbhub::static_delete_cmd(Post_cmd& cmd)
{
    bool success = false;
    if (strncmp("del", cmd.cmd, 3)==0 && cmd.nargs == 1 && strlen(cmd.arg0) <= 12 && strlen(cmd.arg0) > 0) {
        std::string fn = std::string(cmd.arg0) + ".json";
        success = instance().preset_manager.delete_preset(fn);
    }
    return success;
}

bool rppicomidi::Midi2usbhub::is_cmd_valid(const Post_cmd& cmd)
{
    for (auto test: post_cmd_list) {
        if (strncmp(test.cmd.cmd, cmd.cmd, 3) == 0 && test.cmd.nargs == cmd.nargs) {
            return true;
        }
    }
    return false;
}

bool rppicomidi::Midi2usbhub::push_cmd(Post_cmd& cmd)
{
    for (size_t idx = 0; idx < max_pending_cmds; idx++) {
        if (!pending_cmds[idx].pending) {
            for (auto test: post_cmd_list) {
                if (strncmp(test.cmd.cmd, cmd.cmd, 3) == 0 && test.cmd.nargs == cmd.nargs) {
                    pending_cmds[idx].cmd = test;
                    pending_cmds[idx].cmd.cmd = cmd;
                    pending_cmds[idx].pending = true;
                    return true;
                }
            }
            return false; //invalid command
        }
    }
    return false; // no free commands
}

bool rppicomidi::Midi2usbhub::free_cmd(size_t idx)
{
    if (idx < max_pending_cmds && pending_cmds[idx].pending) {
        pending_cmds[idx].pending = false;
        return true;
    }
    return false;
}

void rppicomidi::Midi2usbhub::process_pending_cmds()
{
    protect_from_lwip();
    for (size_t idx = 0; idx < max_pending_cmds; idx++) {
        if (pending_cmds[idx].pending) {
            pending_cmds[idx].cmd.fn(pending_cmds[idx].cmd.cmd);
            free_cmd(idx);
        }
    }
    unprotect_from_lwip();
}

void rppicomidi::Midi2usbhub::screenshot()
{
    int nbytes = oled_screen.get_bmp_file_data_size();
    auto bmp = oled_screen.make_bmp_file_data();
    if (bmp) {
        preset_manager.save_screenshot(bmp, nbytes);
        delete[] bmp;
    }

}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+
void rppicomidi::Midi2usbhub::tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
    (void)in_ep;
    (void)out_ep;
    TU_LOG2("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
            dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);
    // As many MIDI IN ports and MIDI OUT ports as required
    for (uint8_t cable = 0; cable < num_cables_rx; cable++)
    {
        auto port = new Midi_in_port;
        port->cable = cable;
        port->devaddr = dev_addr;

        midi_in_port_list.push_back(port);
    }
    for (uint8_t cable = 0; cable < num_cables_tx; cable++)
    {
        auto port = new Midi_out_port;
        port->cable = cable;
        port->devaddr = dev_addr;

        midi_out_port_list.push_back(port);
    }
}

void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
    rppicomidi::Midi2usbhub::instance().tuh_midi_mount_cb(dev_addr, in_ep, out_ep, num_cables_rx, num_cables_tx);
}

void rppicomidi::Midi2usbhub::tuh_mount_cb(uint8_t dev_addr)
{
    // Don't need to fetch the product string if this is notification for MSC drive or HID device
    if (msc_fat_is_plugged_in(dev_addr-1) || tuh_hid_instance_count(dev_addr) != 0)
        return;

    // Get the VID, PID and Product String info for all MIDI devices
    tuh_vid_pid_get(dev_addr, &attached_devices[dev_addr].vid, &attached_devices[dev_addr].pid);

    //tuh_descriptor_get_string(dev_addr, 0, 0, dev_string_buffer, sizeof(dev_string_buffer), langid_cb, (uintptr_t)(attached_devices + dev_addr));

    // use blocking API
    uint16_t dev_string_buffer[128];
    uint16_t langid = 0;
    if (0 == tuh_descriptor_get_string_sync(dev_addr, 0, 0, dev_string_buffer, sizeof(dev_string_buffer))) {
        langid = dev_string_buffer[1];
        memset(dev_string_buffer,0, sizeof(dev_string_buffer));
        if (0 == tuh_descriptor_get_product_string_sync(dev_addr, langid, dev_string_buffer, sizeof(dev_string_buffer))) {
            char str[128];
            memset(str, 0, 128);
            for (size_t idx = 1; idx < sizeof(dev_string_buffer)/ sizeof(dev_string_buffer[0]) && dev_string_buffer[idx]; idx++) {
                str[idx-1] = dev_string_buffer[idx];
            }
            str[127] = '\0'; // just in case;
            auto devinfo = attached_devices+dev_addr;
            devinfo->product_name = std::string(str);

            for (auto &midi_in : instance().midi_in_port_list) {
                if (midi_in->devaddr == dev_addr) {
                    instance().make_default_nickname(midi_in->nickname, instance().attached_devices[dev_addr].vid,
                                                    instance().attached_devices[dev_addr].pid,
                                                    midi_in->cable, true);
                }
            }
            for (auto &midi_out : instance().midi_out_port_list) {
                if (midi_out->devaddr == dev_addr) {
                    instance().make_default_nickname(midi_out->nickname, instance().attached_devices[dev_addr].vid,
                                                    instance().attached_devices[dev_addr].pid,
                                                    midi_out->cable, false);
                }
            }
            std::string current;
            instance().preset_manager.get_current_preset_name(current);
            if (current.length() < 1 || !instance().preset_manager.load_preset(current)) {
                printf("current preset load failed.\r\n");
            }
            devinfo->configured = true;
            instance().update_json_connected_state();
        }
    }
}

void tuh_mount_cb(uint8_t dev_addr)
{
    rppicomidi::Midi2usbhub::instance().tuh_mount_cb(dev_addr);
}

// Invoked when device with MIDI interface is un-mounted
void rppicomidi::Midi2usbhub::tuh_midi_unmount_cb(uint8_t dev_addr, uint8_t)
{
    for (std::vector<Midi_in_port *>::iterator it = midi_in_port_list.begin(); it != midi_in_port_list.end();)
    {
        if ((*it)->devaddr == dev_addr)
        {
            delete (*it);
            midi_in_port_list.erase(it);
        }
        else
        {
            // remove all reference to the device address in existing sends_data_to_list elements
            for (std::vector<Midi_out_port *>::iterator jt = (*it)->sends_data_to_list.begin(); jt != (*it)->sends_data_to_list.end();)
            {
                if ((*jt)->devaddr == dev_addr)
                {
                    (*it)->sends_data_to_list.erase(jt);
                }
                else
                {
                    ++jt;
                }
            }
            ++it;
        }
    }
    for (uint8_t port=0; port < num_midi_uarts; port++) {
        for (std::vector<Midi_out_port *>::iterator it = uart_midi_in_port[port].sends_data_to_list.begin(); it != uart_midi_in_port[port].sends_data_to_list.end();)
        {
            if ((*it)->devaddr == dev_addr)
            {
                uart_midi_in_port[port].sends_data_to_list.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    for (std::vector<Midi_out_port *>::iterator it = midi_out_port_list.begin(); it != midi_out_port_list.end();)
    {
        if ((*it)->devaddr == dev_addr)
        {
            delete (*it);
            midi_out_port_list.erase(it);
        }
        else
        {
            ++it;
        }
    }

    attached_devices[dev_addr].configured = false;
    attached_devices[dev_addr].product_name.clear();
    attached_devices[dev_addr].vid = 0;
    attached_devices[dev_addr].pid = 0;
    attached_devices[dev_addr].rx_cables = 0;
    attached_devices[dev_addr].tx_cables = 0;
    update_json_connected_state();
}

void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    rppicomidi::Midi2usbhub::instance().tuh_midi_unmount_cb(dev_addr, instance);
}

void rppicomidi::Midi2usbhub::tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
    if (num_packets != 0)
    {
        uint8_t cable_num;
        uint8_t buffer[48];
        while (1)
        {
            uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
            if (bytes_read == 0)
                return;
            // Route the MIDI stream to the correct MIDI OUT port
            for (auto &in_port : midi_in_port_list)
            {
                if (in_port->devaddr == dev_addr && in_port->cable == cable_num)
                {
                    for (auto &out_port : in_port->sends_data_to_list)
                    {
                        if (out_port->devaddr != 0 && attached_devices[out_port->devaddr].configured)
                        {
                            if (out_port->devaddr != uart_devaddr)
                                tuh_midi_stream_write(out_port->devaddr, out_port->cable, buffer, bytes_read);
                            else
                            {
                                uint8_t npushed = pio_midi_uart_write_tx_buffer(midi_uarts[out_port->cable], buffer, bytes_read);
                                if (npushed != bytes_read)
                                {
                                    TU_LOG1("Warning: Dropped %lu bytes sending to UART MIDI Out\r\n", bytes_read - npushed);
                                }
                            }
                        }
                        else
                        {
                            TU_LOG1("skipping %s dev_addr=%u\r\n", out_port->nickname.c_str(), out_port->devaddr);
                        }
                    }
                    break; // found the right in_port; don't need to stay in the loop
                }
            }
        }
    }
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
    rppicomidi::Midi2usbhub::instance().tuh_midi_rx_cb(dev_addr, num_packets);
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}
