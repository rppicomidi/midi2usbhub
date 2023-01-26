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
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "tusb.h"
#include "class/midi/midi_host.h"
#include "embedded_cli.h"
#include "parson.h"
#include "preset_manager.h"
#include "pico_w_connection_manager.h"
#include "midi2usbhub_cli.h"

#include "mono_graphics_lib.h"
#include "ssd1306i2c.h"
#include "ssd1306.h"
#include "view_manager.h"
#include "home_screen.h"

namespace rppicomidi
{
class Midi2usbhub
{
public:
    // Singleton Pattern

    /**
     * @brief Get the Instance object
     *
     * @return the singleton instance
     */
    static Midi2usbhub &instance()
    {
        static Midi2usbhub _instance; // Guaranteed to be destroyed.
                                    // Instantiated on first use.
        return _instance;
    }
    Midi2usbhub(Midi2usbhub const &) = delete;
    void operator=(Midi2usbhub const &) = delete;
    void blink_led();
    void poll_usb_rx();
    void flush_usb_tx();
    void poll_midi_uart_rx(uint8_t port_num);
    /**
     * @brief construct a nickname string from the input parameters
     *
     * @param nickname a reference to the std::string that will store the new nickname
     * @param vid the Connected MIDI Device's Vendor ID
     * @param pid the Connected MIDI Device's Product ID
     * @param cable the port's virtual cable number (0-15)
     * @param is_from is true if the hub terminal is from (connected to the Connected MIDI Device's MIDI OUT)
     */
    void make_default_nickname(std::string& nickname, uint16_t vid, uint16_t pid, uint8_t cable, bool is_from);
    struct Midi_device_info
    {
        uint16_t vid;
        uint16_t pid;
        uint8_t tx_cables;
        uint8_t rx_cables;
        std::string product_name;
        bool configured;
    };

    struct Midi_out_port
    {
        uint8_t devaddr;
        uint8_t cable;
        std::string nickname;
    };

    struct Midi_in_port
    {
        uint8_t devaddr;
        uint8_t cable;
        std::string nickname;
        std::vector<Midi_out_port *> sends_data_to_list;
    };
    void tuh_mount_cb(uint8_t dev_addr);
    void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx);
    void tuh_midi_unmount_cb(uint8_t dev_addr, uint8_t instance);
    void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets);
    /**
     * @brief create JSON formatted string that represents the current settings
     *
     * The JSON format is
     * {
     *     "nicknames":{
     *         default_nickname: nickname,
     *         default_nickname: nickname,
     *               ...
     *         default_nickname: nickname
     *     },
     *     "routing": {
     *         from_nickname_string:[nickname_string, nickname_string,..., nickname_string],
     *         from_nickname_string:[nickname_string, nickname_string,..., nickname_string],
     *               ...
     *         from_nickname_string:[nickname_string, nickname_string,..., nickname_string]
     *     }
     * }
     * @param serialized_settings
     */
    void serialize(std::string &serialized_settings);
    JSON_Value* serialize_to_json();
    bool deserialize(const std::string &serialized_settings);

    /**
     * @brief connect the MIDI stream from the device and port from_nickname
     * to the device and port to_nickname
     *
     * @param from_nickname the nickname that represents the device an port
     * of the MIDI stream source
     * @param to_nickname the nickname that represents the device an port
     * of the MIDI stream sink
     * @return int 0 if successful, -1 if the to_nickname is invalid, -2
     * if the from_nickname is invalid
     */
    int connect(const std::string& from_nickname, const std::string& to_nickname);

    /**
     * @brief disconnect the MIDI stream from the device and port from_nickname
     * to the device and port to_nickname
     *
     * @param from_nickname the nickname that represents the device an port
     * of the MIDI stream source
     * @param to_nickname the nickname that represents the device an port
     * of the MIDI stream sink
     * @return int 0 if successful, -1 if the to_nickname is invalid, -2
     * if the from_nickname is invalid
     */
    int disconnect(const std::string& from_nickname, const std::string& to_nickname);

    /**
     * @brief clear all MIDI stream connections
     *
     */
    void reset();

    /**
     * @brief rename a device and port nickname
     *
     * @param old_nickname the previous nickname of the device and port
     * @param new_nickname the new nickname of the device and port
     * @return int -1 if the new_nickname is already in use, -2 if the old_nickname is not found,
     * 1 if FROM nickname renamed successfully, 2 if TO nickname renamed successfully
     */
    int rename(const std::string& old_nickname, const std::string& new_nickname);

    /**
     * @brief route the MIDI traffic
     *
     */
    void task();

#ifdef RPPICOMIDI_PICO_W
    void get_connected();
#endif
    Midi_device_info* get_attached_device(size_t addr) { if (addr < 1 || addr > uart_devaddr) return nullptr; return &attached_devices[addr]; }
    const std::vector<Midi_out_port *>& get_midi_out_port_list() {return midi_out_port_list; }
    const std::vector<Midi_in_port *>& get_midi_in_port_list() {return midi_in_port_list; }

    /**
     * @brief Get the connected state in JSON serialized string format
     *
     * @return const char* the serialized string
     */
    const char* get_json_connected_state() const {return json_connected_state.c_str(); }
    /**
     * @brief Update the JSON serialization string of the device connected state
     *
     * @note the serialized string always contains "force":"false". The web page will
     * set its local copy to true to force UI resynchronization.
     */
    void update_json_connected_state();
#ifdef RPPICOMIDI_PICO_W
    err_t post_begin(void *connection, const char *uri, const char *http_request, u16_t http_request_len,
                        int content_len, char *response_uri, u16_t response_uri_len, u8_t *post_auto_wnd);
    err_t post_receive_data(void *connection, struct pbuf *p);
    void post_finished(void *connection, char *response_uri, u16_t response_uri_len);
    static const size_t cmd_len = 4;
    static const size_t max_arg_len = 13;
    static const size_t max_args = 2;
    static const size_t max_pending_cmds = 10;

    struct Post_cmd {
        size_t nargs;
        char cmd[cmd_len];
        char arg0[max_arg_len];
        char arg1[max_arg_len];
    };

    /**
     * @brief return true if cmd is a valid command
     *
     * @param cmd is the Post_cmd structure under test
     * @return true if the command can be dispatched, false otherwise
     */
    bool is_cmd_valid(const Post_cmd& cmd);

    /**
     * @brief Add the command to the pending command list
     *
     * @param cmd the command to add
     * @return true if the command can be dispatched and there is
     * space in the pending command list; false otherwise
     * @note only call this from interrupt context
     */
    bool push_cmd(Post_cmd& cmd);
#endif
    void screenshot();
    bool export_screenshots() {return preset_manager.export_all_screenshots() == 0; }
private:
    Midi2usbhub();
    static bool static_connect_cmd(Post_cmd& cmd);
    static bool static_disconnect_cmd(Post_cmd& cmd);
    static bool static_rename_cmd(Post_cmd& cmd);
    static bool static_mv_cmd(Post_cmd& cmd);
    static bool static_reset_cmd(Post_cmd& cmd);
    static bool static_load_cmd(Post_cmd& cmd);
    static bool static_save_cmd(Post_cmd& cmd);
    static bool static_delete_cmd(Post_cmd& cmd);
    void protect_from_lwip() {irq_set_enabled(IO_IRQ_BANK0, false);}
    void unprotect_from_lwip() {irq_set_enabled(IO_IRQ_BANK0, true);}
    Preset_manager preset_manager;

    // UART selection Pin mapping. You can move these for your design if you want to
    static const uint MIDI_UART_A_TX_GPIO = 4;
    static const uint MIDI_UART_A_RX_GPIO = 5;
    static const uint MIDI_UART_B_TX_GPIO = 12;
    static const uint MIDI_UART_B_RX_GPIO = 13;
    static const size_t num_midi_uarts = 2;
    // On-board LED mapping. If no LED, set to NO_LED_GPIO
    static const uint NO_LED_GPIO = 255;
    static const uint LED_GPIO = 25;

    static const uint8_t uart_devaddr = CFG_TUH_DEVICE_MAX + 1;

    // Indexed by dev_addr
    // device addresses start at 1. location 0 is unused
    // extra entry is for the UART MIDI Port
    Midi_device_info attached_devices[CFG_TUH_DEVICE_MAX + 2];
    void* midi_uarts[num_midi_uarts];
    std::vector<Midi_out_port *> midi_out_port_list;
    std::vector<Midi_in_port *> midi_in_port_list;

    Midi_in_port uart_midi_in_port[num_midi_uarts];
    Midi_out_port uart_midi_out_port[num_midi_uarts];

#ifdef RPPICOMIDI_PICO_W
    Pico_w_connection_manager wifi;

    /**
     * @brief Mark a command in the command list as free
     *
     * @param idx the index of the command to mark not pending
     * @return true if idx is in range and command was originally pending,
     * false otherwise
     * @note only call this command after having called protect_from_lwip
     */
    bool free_cmd(size_t idx);

    void process_pending_cmds();
    void* current_connection;
    struct Post_cmd_dispatch {
        Post_cmd cmd;
        bool (*fn)(Post_cmd& cmd);
    };

    struct Pending_cmd {
        Pending_cmd() : pending{false} { }
        Post_cmd_dispatch cmd;
        bool pending;
    };

    std::vector<Post_cmd_dispatch> post_cmd_list;
    Pending_cmd pending_cmds[max_pending_cmds];
    void* last_post_error_connection;
#endif
    Midi2usbhub_cli cli;
    std::string json_connected_state;
    std::string json_current_settings;
    const uint8_t OLED_ADDR=0x3c;   // the OLED I2C address as a constant
    uint8_t addr[1];                // the OLED I2C address is stored here
    const uint8_t MUX_ADDR=0;       // no I2C mux
    uint8_t* mux_map=nullptr;       // no I2C mux
    const uint8_t OLED_SCL_GPIO = 3;   // The OLED SCL pin
    const uint8_t OLED_SDA_GPIO = 2;   // The OLED SDA pin

    // the i2c driver object
    Ssd1306i2c i2c_driver_oled{i2c1, addr, OLED_SDA_GPIO, OLED_SCL_GPIO, sizeof(addr), MUX_ADDR, mux_map};

    Ssd1306 ssd1306;    // the SSD1306 driver object
    Mono_graphics oled_screen; // the screen object
    View_manager oled_view_manager; // the view manager object
    static uint16_t render_done_mask;
    static void callback(uint8_t display_num)
    {
        render_done_mask |= (1u << display_num);
    }
    Home_screen home_screen;
};
}