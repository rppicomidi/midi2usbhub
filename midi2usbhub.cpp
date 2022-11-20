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
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "midi_uart_lib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/midi/midi_host.h"
#include "embedded_cli.h"
// On-board LED mapping. If no LED, set to NO_LED_GPIO
const uint NO_LED_GPIO = 255;
const uint LED_GPIO = 25;
// UART selection Pin mapping. You can move these for your design if you want to
// Make sure all these values are consistent with your choice of midi_uart
#define MIDI_UART_NUM 1
const uint MIDI_UART_TX_GPIO = 4;
const uint MIDI_UART_RX_GPIO = 5;

static void *midi_uart_instance;

static const uint8_t uart_devaddr = 128;
struct Midi_out_port {
    uint8_t devaddr;
    uint8_t cable;
};

struct Midi_in_port {
    uint8_t devaddr;
    uint8_t cable;
    std::vector<Midi_out_port*> sends_data_to_list;
};

static std::vector<Midi_out_port*> midi_out_port_list;
static std::vector<Midi_in_port*> midi_in_port_list;

struct Midi_in_port uart_midi_in_port;

static void onCommand(const char* name, char *tokens)
{
    printf("Received command: %s\r\n",name);

    for (int i = 0; i < embeddedCliGetTokenCount(tokens); ++i) {
        printf("Arg %d : %s\r\n", i, embeddedCliGetToken(tokens, i + 1));
    }
}

static void onCommandFn(EmbeddedCli *embeddedCli, CliCommand *command)
{
    (void)embeddedCli;
    embeddedCliTokenizeArgs(command->args);
    onCommand(command->name == NULL ? "" : command->name, command->args);
}

static void writeCharFn(EmbeddedCli *embeddedCli, char c)
{
    (void)embeddedCli;
    putchar(c);
}


static void blink_led(void)
{
    static absolute_time_t previous_timestamp = {0};

    static bool led_state = false;

    // This design has no on-board LED
    if (NO_LED_GPIO == LED_GPIO)
        return;
    absolute_time_t now = get_absolute_time();
    
    int64_t diff = absolute_time_diff_us(previous_timestamp, now);
    if (diff > 1000000) {
        gpio_put(LED_GPIO, led_state);
        led_state = !led_state;
        previous_timestamp = now;
    }
}

static void poll_usb_rx()
{
    for (auto& in_port: midi_in_port_list) {
        // poll each connected MIDI IN port only once per device address
        if (in_port->cable == 0 && tuh_midi_configured(in_port->devaddr))
            tuh_midi_read_poll(in_port->devaddr);
    }
}


static void poll_midi_uart_rx()
{
    uint8_t rx[48];
    // Pull any bytes received on the MIDI UART out of the receive buffer and
    // send them out via USB MIDI on virtual cable 0
    uint8_t nread = midi_uart_poll_rx_buffer(midi_uart_instance, rx, sizeof(rx));
    if (nread > 0) {
        // figure out where to send data from UART MIDI IN
        for (auto& out_port: uart_midi_in_port.sends_data_to_list) {
            if (out_port->devaddr != 0 && tuh_midi_configured(out_port->devaddr))
            {
                uint32_t nwritten = tuh_midi_stream_write(out_port->devaddr, out_port->cable,rx, nread);
                if (nwritten != nread) {
                    TU_LOG1("Warning: Dropped %lu bytes receiving from UART MIDI In\r\n", nread - nwritten);
                }
            }
        }
    }
}

static void cli_task(EmbeddedCli *cli)
{
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        embeddedCliReceiveChar(cli, c);
        embeddedCliProcess(cli);
    }
}

int main() {

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
    while(getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {
        // flush out the console input buffer
    }
    uart_midi_in_port.cable = 0;
    uart_midi_in_port.devaddr = uart_devaddr;
    uart_midi_in_port.sends_data_to_list.clear();
    // Initialize the CLI
    EmbeddedCli *cli = embeddedCliNewDefault();
    cli->onCommand = onCommandFn;
    cli->writeChar = writeCharFn;
    printf("Cli is running. Type \"exit\" to exit\r\n");
    printf("Type \"help\" for a list of commands\r\n");
    printf("Use backspace and tab to remove chars and autocomplete\r\n");
    printf("Use up and down arrows to recall previous commands\r\n");

    while (1) {
        tuh_task();

        blink_led();
        poll_midi_uart_rx();
        for(auto& out_port : midi_out_port_list) {
            // Call tuh_midi_stream_flush() once per output port device address
            if (out_port->cable == 0 && tuh_midi_configured(out_port->devaddr)) {
                tuh_midi_stream_flush(out_port->devaddr);
            }
        }
        poll_usb_rx();
        midi_uart_drain_tx_buffer(midi_uart_instance);
        cli_task(cli);
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
    (void)in_ep;
    (void)out_ep;
    TU_LOG2("MIDI device address = %u, IN endpoint %u has %u cables, OUT endpoint %u has %u cables\r\n",
        dev_addr, in_ep & 0xf, num_cables_rx, out_ep & 0xf, num_cables_tx);
    // As many MIDI IN ports and MIDI OUT ports as required
    for (uint8_t cable = 0; cable < num_cables_rx; cable++) {
        auto port = new Midi_in_port;
        port->cable = cable;
        port->devaddr = dev_addr;

        midi_in_port_list.push_back(port);
    }
    for (uint8_t cable = 0; cable < num_cables_tx; cable++) {
        auto port = new Midi_out_port;
        port->cable = cable;
        port->devaddr = dev_addr;

        midi_out_port_list.push_back(port);
    }
    // TODO: update MIDI IN to MIDI OUT wiring based on stored settings for current preset
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)instance;
    for (std::vector<Midi_in_port*>::iterator it = midi_in_port_list.begin(); it != midi_in_port_list.end();) {
        if ((*it)->devaddr == dev_addr) {
            delete (*it);
            midi_in_port_list.erase(it);
        }
        else {
            // remove all reference to the device address in existing sends_data_to_list elements
            for (std::vector<Midi_out_port*>::iterator jt = (*it)->sends_data_to_list.begin(); jt != (*it)->sends_data_to_list.end();) {
                if ((*jt)->devaddr == dev_addr) {
                    (*it)->sends_data_to_list.erase(jt);
                }
                else {
                    ++jt;
                }
            }

            ++it;
        }
    }
    for (std::vector<Midi_out_port*>::iterator it = uart_midi_in_port.sends_data_to_list.begin(); it != uart_midi_in_port.sends_data_to_list.end();) {
        if ((*it)->devaddr == dev_addr) {
            uart_midi_in_port.sends_data_to_list.erase(it);
        }
        else {
            ++it;
        }
    }
    for (std::vector<Midi_out_port*>::iterator it = midi_out_port_list.begin(); it != midi_out_port_list.end();) {
        if ((*it)->devaddr == dev_addr) {
            delete (*it);
            midi_out_port_list.erase(it);
        }
        else {
            ++it;
        }
    }
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
    if (num_packets != 0)
    {
        uint8_t cable_num;
        uint8_t buffer[48];
        while (1) {
            uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, sizeof(buffer));
            if (bytes_read == 0)
                return;
            // Route the MIDI stream to the correct MIDI OUT port
            for (auto& in_port : midi_in_port_list) {
                if (in_port->devaddr == dev_addr && in_port->cable == cable_num) {
                    for (auto& out_port : in_port->sends_data_to_list) {
                        if (out_port->devaddr != 0 && tuh_midi_configured(out_port->devaddr)) {
                            if (out_port->devaddr != uart_devaddr)
                                tuh_midi_stream_write(out_port->devaddr, out_port->cable, buffer, bytes_read);
                            else {
                                uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance,buffer,bytes_read);
                                if (npushed != bytes_read) {
                                    TU_LOG1("Warning: Dropped %lu bytes sending to UART MIDI Out\r\n", bytes_read - npushed);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

void tuh_midi_tx_cb(uint8_t dev_addr)
{
    (void)dev_addr;
}