/**
 * MIT License
 *
 * Copyright (c) 2022 rppicomidi
 *
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
#include "embedded_cli.h"
#include "pico_w_connection_manager.h"
namespace rppicomidi
{
    class Pico_w_connection_manager_cli
    {
    public:
        Pico_w_connection_manager_cli() = delete;
        Pico_w_connection_manager_cli(Pico_w_connection_manager_cli const &) = delete;
        void operator=(Pico_w_connection_manager_cli const &) = delete;

        Pico_w_connection_manager_cli(EmbeddedCli *cli_) : cli{cli_} {}
        ~Pico_w_connection_manager_cli() = default;
        /**
         * @brief Set the up cli
         *
         * @param wifi_ a pointer to an instance of the Pico_w_connection_manager class
         * that the CLI commands will act upon.
         */
        void setup_cli(Pico_w_connection_manager *wifi_);
        /**
         * @brief Get the number of CLI commands this class provides
         *
         * @note You must update this number if you add commands or else
         * the code will assert
         * @return the number of CLI commands
         */
        static uint16_t get_num_commands() { return 15; }

    private:
        // CLI commands
        static void on_initialize(EmbeddedCli *cli, char *args, void *context);
        static void on_deinitialize(EmbeddedCli *cli, char *args, void *context);
        static void on_country_code(EmbeddedCli *cli, char *args, void *context);
        static void on_list_countries(EmbeddedCli *cli, char *args, void *context);
        static void on_save_settings(EmbeddedCli *cli, char *args, void *context);
        static void on_load_settings(EmbeddedCli *cli, char *args, void *context);
        static void on_start_scan(EmbeddedCli *cli, char *args, void *context);
        static void on_link_status(EmbeddedCli *cli, char *args, void *context);
        static void on_scan_connect(EmbeddedCli *cli, char *args, void *context);
        static void on_autoconnect(EmbeddedCli *cli, char *args, void *context);
        static void on_connect(EmbeddedCli *cli, char *args, void *context);
        static void on_disconnect(EmbeddedCli *cli, char *args, void *context);
        static void on_rssi(EmbeddedCli *cli, char *args, void *context);
        static void on_list_known_aps(EmbeddedCli *cli, char *args, void *context);
        static void on_forget_ap(EmbeddedCli *cli, char *args, void *context);

        // callbacks
        static void scan_complete(void *);
        static void scan_connect_complete(void *);
        static void link_up(void *);
        static void link_down(void *);

        // data
        EmbeddedCli *cli;
    };
}
