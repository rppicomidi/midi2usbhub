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
#include <vector>
#include <cstdint>
#include <string>
#include "tusb.h"
#include "class/midi/midi_host.h"
#include "embedded_cli.h"
#include "parson.h"

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
        void poll_midi_uart_rx();
        void cli_task();
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
        void get_info_from_default_nickname(std::string nickname, uint16_t &vid, uint16_t &pid, uint8_t &cable, bool &is_from);
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
        void *midi_uart_instance;
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
        bool deserialize(std::string &serialized_settings);

        void task();
    private:
        Midi2usbhub();
        static void langid_cb(tuh_xfer_t *xfer);
        static void prod_str_cb(tuh_xfer_t *xfer);

        /*
         * The following 3 functions are required by the EmbeddedCli library
         */
        static void onCommand(const char *name, char *tokens)
        {
            printf("Received command: %s\r\n", name);

            for (int i = 0; i < embeddedCliGetTokenCount(tokens); ++i)
            {
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

        // The following are CLI functions
        static void static_list(EmbeddedCli *, char *, void *);
        static void static_connect(EmbeddedCli *, char *, void *);
        static void static_disconnect(EmbeddedCli *, char *, void *);
        static void static_show(EmbeddedCli *, char *, void *);
        static void static_reset(EmbeddedCli *, char *, void *);
        static void static_rename(EmbeddedCli *, char *, void *);

        // UART selection Pin mapping. You can move these for your design if you want to
        // Make sure all these values are consistent with your choice of midi_uart
        static const uint MIDI_UART_NUM = 1;
        static const uint MIDI_UART_TX_GPIO = 4;
        static const uint MIDI_UART_RX_GPIO = 5;
        // On-board LED mapping. If no LED, set to NO_LED_GPIO
        static const uint NO_LED_GPIO = 255;
        static const uint LED_GPIO = 25;

        static const uint8_t uart_devaddr = CFG_TUH_DEVICE_MAX + 1;

        // Indexed by dev_addr
        // device addresses start at 1. location 0 is unused
        // extra entry is for the UART MIDI Port
        Midi_device_info attached_devices[CFG_TUH_DEVICE_MAX + 2];

        std::vector<Midi_out_port *> midi_out_port_list;
        std::vector<Midi_in_port *> midi_in_port_list;

        Midi_in_port uart_midi_in_port;
        Midi_out_port uart_midi_out_port;
        EmbeddedCli *cli;
    };
}