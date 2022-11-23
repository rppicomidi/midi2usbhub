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
#include "midi_uart_lib.h"
#include "bsp/board.h"
#include "preset_manager.h"

void rppicomidi::Midi2usbhub::serialize(std::string &serialized_string)
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

    auto ser = json_serialize_to_string(root_value);
    serialized_string = std::string(ser);
    json_free_serialized_string(ser);
    json_value_free(root_value);
}

bool rppicomidi::Midi2usbhub::deserialize(std::string &serialized_string)
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
                        // poorly formatted JSON
                        json_value_free(root_value);
                        return false;
                    }
                }
            }
            else {
                // poorly formatted JSON
                json_value_free(root_value);
                return false;
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

void rppicomidi::Midi2usbhub::blink_led()
{
    static absolute_time_t previous_timestamp = {0};

    static bool led_state = false;

    // This design has no on-board LED
    if (NO_LED_GPIO == LED_GPIO)
        return;
    absolute_time_t now = get_absolute_time();

    int64_t diff = absolute_time_diff_us(previous_timestamp, now);
    if (diff > 1000000)
    {
        gpio_put(LED_GPIO, led_state);
        led_state = !led_state;
        previous_timestamp = now;
    }
}

void rppicomidi::Midi2usbhub::poll_usb_rx()
{
    for (auto &in_port : midi_in_port_list)
    {
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

void rppicomidi::Midi2usbhub::poll_midi_uart_rx()
{
    uint8_t rx[48];
    // Pull any bytes received on the MIDI UART out of the receive buffer and
    // send them out via USB MIDI on virtual cable 0
    uint8_t nread = midi_uart_poll_rx_buffer(midi_uart_instance, rx, sizeof(rx));
    if (nread > 0)
    {
        // figure out where to send data from UART MIDI IN
        for (auto &out_port : uart_midi_in_port.sends_data_to_list)
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

void rppicomidi::Midi2usbhub::cli_task()
{
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT)
    {
        embeddedCliReceiveChar(cli, c);
        embeddedCliProcess(cli);
    }
}

rppicomidi::Midi2usbhub::Midi2usbhub()
{
    bi_decl(bi_program_description("Provide a USB host interface for Serial Port MIDI."));
    bi_decl(bi_1pin_with_name(LED_GPIO, "On-board LED"));
    bi_decl(bi_2pins_with_names(MIDI_UART_TX_GPIO, "MIDI UART TX", MIDI_UART_RX_GPIO, "MIDI UART RX"));

    board_init();
    printf("Pico MIDI Host to MIDI UART Adapter\r\n");
    tusb_init();

    // Map the pins to functions
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
    midi_uart_instance = midi_uart_configure(MIDI_UART_NUM, MIDI_UART_TX_GPIO, MIDI_UART_RX_GPIO);
    printf("Configured MIDI UART %u for 31250 baud\r\n", MIDI_UART_NUM);
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT)
    {
        // flush out the console input buffer
    }
    uart_midi_in_port.cable = 0;
    uart_midi_in_port.devaddr = uart_devaddr;
    uart_midi_in_port.sends_data_to_list.clear();
    uart_midi_in_port.nickname = "MIDI-IN-A";
    uart_midi_out_port.cable = 0;
    uart_midi_out_port.devaddr = uart_devaddr;
    uart_midi_out_port.nickname = "MIDI-OUT-A";
    attached_devices[uart_devaddr].vid = 0;
    attached_devices[uart_devaddr].pid = 0;
    attached_devices[uart_devaddr].product_name = "MIDI A";
    attached_devices[uart_devaddr].rx_cables = 1;
    attached_devices[uart_devaddr].tx_cables = 1;
    attached_devices[uart_devaddr].configured = true;
    midi_in_port_list.push_back(&uart_midi_in_port);
    midi_out_port_list.push_back(&uart_midi_out_port);
    // Initialize the CLI
    EmbeddedCliConfig cli_config = {
        .rxBufferSize = 64,
        .cmdBufferSize = 64,
        .historyBufferSize = 128,
        .maxBindingCount = 15,
        .cliBuffer = NULL,
        .cliBufferSize = 0,
        .enableAutoComplete = true,
    };
    cli = embeddedCliNew(&cli_config);
    cli->onCommand = onCommandFn;
    cli->writeChar = writeCharFn;
    assert(embeddedCliAddBinding(cli, {"connect",
                                       "Route MIDI streams. usage: connect <FROM nickname> <TO nickname>",
                                       true,
                                       this,
                                       static_connect}));
    assert(embeddedCliAddBinding(cli, {"disconnect",
                                       "Break MIDI stream route. usage: disconnect <FROM nickname> <TO nickname>",
                                       true,
                                       this,
                                       static_disconnect}));
    assert(embeddedCliAddBinding(cli, {"list",
                                       "List all connected MIDI Devices: usage: list",
                                       false,
                                       this,
                                       static_list}));
    assert(embeddedCliAddBinding(cli, {"rename",
                                       "Change a nickname. usage: rename <Old Nickname> <New Nickname>",
                                       true,
                                       this,
                                       static_rename}));
    assert(embeddedCliAddBinding(cli, {"reset",
                                       "Disconnect all routes. usage reset",
                                       false,
                                       this,
                                       static_reset}));
    assert(embeddedCliAddBinding(cli, {"show",
                                       "Show the connection matrix. usage show",
                                       false,
                                       this,
                                       static_show}));

    Preset_manager::instance().init_cli(cli);
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

// Main loop
int main()
{
    rppicomidi::Midi2usbhub &instance = rppicomidi::Midi2usbhub::instance();

    while (1)
    {
        tuh_task();

        instance.blink_led();

        instance.poll_midi_uart_rx();
        instance.flush_usb_tx();
        instance.poll_usb_rx();
        midi_uart_drain_tx_buffer(instance.midi_uart_instance);

        instance.cli_task();
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

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+
static uint16_t dev_string_buffer[128];
static uint16_t langid;
void rppicomidi::Midi2usbhub::prod_str_cb(tuh_xfer_t *xfer)
{
    if (xfer->actual_len >= 4 /* long enough for at least one character*/)
    {
        size_t nchars = (xfer->actual_len - 2) / 2;
        char str[nchars + 1];
        uint16_t *utf16le = (uint16_t *)(xfer->buffer + 2);
        for (size_t idx = 0; idx < nchars; idx++)
        {
            str[idx] = (uint8_t)utf16le[idx];
        }
        str[nchars] = '\0';
        auto devinfo = reinterpret_cast<rppicomidi::Midi2usbhub::Midi_device_info *>(xfer->user_data);
        devinfo->product_name = std::string(str);

        for (auto &midi_in : instance().midi_in_port_list)
        {
            if (midi_in->devaddr == xfer->daddr)
            {
                instance().make_default_nickname(midi_in->nickname, instance().attached_devices[xfer->daddr].vid,
                                                 instance().attached_devices[xfer->daddr].pid,
                                                 midi_in->cable, true);
            }
        }
        for (auto &midi_out : instance().midi_out_port_list)
        {
            if (midi_out->devaddr == xfer->daddr)
            {
                instance().make_default_nickname(midi_out->nickname, instance().attached_devices[xfer->daddr].vid,
                                                 instance().attached_devices[xfer->daddr].pid,
                                                 midi_out->cable, false);
            }
        }
        std::string current;
        Preset_manager::instance().get_current_preset_name(current);
        if (current.length() < 1 || !Preset_manager::instance().load_preset(current)) {
            printf("current preset load failed.\r\n");
        }
        devinfo->configured = true;
    }
}

void rppicomidi::Midi2usbhub::langid_cb(tuh_xfer_t *xfer)
{
    if (xfer->actual_len >= 4 /*length, type, and one lang ID*/)
    {
        langid = *((uint16_t *)(xfer->buffer + 2));
        tuh_descriptor_get_product_string(xfer->daddr, langid, dev_string_buffer, sizeof(dev_string_buffer), rppicomidi::Midi2usbhub::prod_str_cb, xfer->user_data);
    }
}

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
    tuh_vid_pid_get(dev_addr, &attached_devices[dev_addr].vid, &attached_devices[dev_addr].pid);

    tuh_descriptor_get_string(dev_addr, 0, 0, dev_string_buffer, sizeof(dev_string_buffer), langid_cb, (uintptr_t)(attached_devices + dev_addr));
}

void tuh_mount_cb(uint8_t dev_addr)
{
    rppicomidi::Midi2usbhub::instance().tuh_mount_cb(dev_addr);
}

// Invoked when device with hid interface is un-mounted
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

    for (std::vector<Midi_out_port *>::iterator it = uart_midi_in_port.sends_data_to_list.begin(); it != uart_midi_in_port.sends_data_to_list.end();)
    {
        if ((*it)->devaddr == dev_addr)
        {
            uart_midi_in_port.sends_data_to_list.erase(it);
        }
        else
        {
            ++it;
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
                                uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer, bytes_read);
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

void rppicomidi::Midi2usbhub::static_list(EmbeddedCli *, char *, void *)
{
    printf("USB ID      Port  Direction Nickname     Product Name\n");

    for (size_t addr = 1; addr <= CFG_TUH_DEVICE_MAX + 1; addr++)
    {
        auto dev = instance().attached_devices[addr];
        if (dev.configured)
        {
            // printf("%04x-%04x    %d      %s    %s %s\r\n", dev.vid, dev.pid, 1, "FROM", "0944-0117-1 ", dev.product_name.c_str());
            for (auto &in_port : instance().midi_in_port_list)
            {
                if (in_port->devaddr == addr)
                {
                    printf("%04x-%04x    %-2d     %s    %-12s %s\r\n", dev.vid, dev.pid, in_port->cable + 1,
                           "FROM", in_port->nickname.c_str(), dev.product_name.c_str());
                    for (auto &out_port : instance().midi_out_port_list)
                    {
                        if (out_port->devaddr == addr && out_port->cable == in_port->cable)
                        {
                            printf("%04x-%04x    %-2d     %s    %-12s %s\r\n", dev.vid, dev.pid,
                                   out_port->cable + 1,
                                   " TO ", out_port->nickname.c_str(), dev.product_name.c_str());
                            break;
                        }
                    }
                }
            }
        }
    }
}

void rppicomidi::Midi2usbhub::static_connect(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2)
    {
        printf("connect <FROM nickname> <TO nickname>\r\n");
        return;
    }
    auto from_nickname = std::string(embeddedCliGetToken(args, 1));
    auto to_nickname = std::string(embeddedCliGetToken(args, 2));
    for (auto &in_port : instance().midi_in_port_list)
    {
        if (in_port->nickname == from_nickname)
        {
            for (auto &out_port : instance().midi_out_port_list)
            {
                if (out_port->nickname == to_nickname)
                {
                    in_port->sends_data_to_list.push_back(out_port);
                    printf("%s connect to %s: successful\r\n",
                           in_port->nickname.c_str(), out_port->nickname.c_str());
                    return;
                }
            }
            printf("TO nickname %s not found\r\n", to_nickname.c_str());
        }
    }
    printf("FROM nickname %s not found\r\n", from_nickname.c_str());
}

void rppicomidi::Midi2usbhub::static_disconnect(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2)
    {
        printf("disconnect <FROM nickname> <TO nickname>\r\n");
        return;
    }
    auto from_nickname = std::string(embeddedCliGetToken(args, 1));
    auto to_nickname = std::string(embeddedCliGetToken(args, 2));
    for (auto &in_port : instance().midi_in_port_list)
    {
        if (in_port->nickname == from_nickname)
        {
            for (auto it = in_port->sends_data_to_list.begin();
                 it != in_port->sends_data_to_list.end();)
            {
                if ((*it)->nickname == to_nickname)
                {
                    in_port->sends_data_to_list.erase(it);
                    printf("%s disconnect %s: successful\r\n",
                           in_port->nickname.c_str(), to_nickname.c_str());
                    return;
                }
                else
                {
                    ++it;
                }
            }
            printf("TO nickname %s not found\r\n", to_nickname.c_str());
            return;
        }
    }
    printf("FROM nickname %s not found\r\n", from_nickname.c_str());
}

void rppicomidi::Midi2usbhub::static_reset(EmbeddedCli *, char *, void *)
{
    for (auto &in_port : instance().midi_in_port_list)
    {
        in_port->sends_data_to_list.clear();
    }
}

void rppicomidi::Midi2usbhub::static_show(EmbeddedCli *, char *, void *)
{
    // Print the top header
    for (size_t line = 0; line < 12; line++)
    {
        if (line == 0)
            printf("        TO->|");
        else if (line == 7)
            printf("  FROM |    |");
        else if (line == 8)
            printf("       v    |");
        else
            printf("            |");

        for (auto &midi_out : instance().midi_out_port_list)
        {
            size_t first_idx = line;
            if (midi_out->nickname.length() < 12)
            {
                first_idx = 12 - midi_out->nickname.length();
            }
            if (line < first_idx)
                printf("   |");
            else
                printf(" %c |", midi_out->nickname.c_str()[line - first_idx]);
        }
        printf("\r\n");
    }
    printf("------------+");
    for (size_t col = 0; col < instance().midi_out_port_list.size(); col++)
    {
        printf("---+");
    }
    printf("\r\n");
    for (auto &midi_in : instance().midi_in_port_list)
    {
        printf("%-12s|", midi_in->nickname.c_str());
        for (auto &midi_out : instance().midi_out_port_list)
        {
            char connection_mark = ' ';
            for (auto &sends_to : midi_in->sends_data_to_list)
            {
                if (sends_to == midi_out)
                {
                    connection_mark = 'x';
                }
            }
            printf(" %c |", connection_mark);
        }
        printf("\r\n------------+");
        for (size_t col = 0; col < instance().midi_out_port_list.size(); col++)
        {
            printf("---+");
        }
        printf("\r\n");
    }
}

void rppicomidi::Midi2usbhub::static_rename(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2)
    {
        printf("usage: rename <Old Nickname> <New Nickname>\r\n");
        return;
    }
    auto old_nickname = std::string(embeddedCliGetToken(args, 1));
    auto new_nickname = std::string(embeddedCliGetToken(args, 2));
    // make sure the new nickname is not already in use
    for (auto &midi_in : instance().midi_in_port_list)
    {
        if (midi_in->nickname == new_nickname)
        {
            printf("New Nickname %s already in use\r\n", new_nickname.c_str());
            return;
        }
    }
    for (auto &midi_out : instance().midi_out_port_list)
    {
        if (midi_out->nickname == new_nickname)
        {
            printf("New Nickname %s already in use\r\n", new_nickname.c_str());
            return;
        }
    }
    for (auto &midi_in : instance().midi_in_port_list)
    {
        if (midi_in->nickname == old_nickname)
        {
            midi_in->nickname = new_nickname;
            printf("FROM Nickname %s set to %s\r\n", old_nickname.c_str(), new_nickname.c_str());
            return;
        }
    }
    for (auto &midi_out : instance().midi_out_port_list)
    {
        if (midi_out->nickname == old_nickname)
        {
            midi_out->nickname = new_nickname;
            printf("TO Nickname %s set to %s\r\n", old_nickname.c_str(), new_nickname.c_str());
            return;
        }
    }
    printf("Old Nickname %s not found\r\n", old_nickname.c_str());
}
