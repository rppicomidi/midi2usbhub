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
#include "midi2usbhub_cli.h"
#include "pico/stdlib.h"
#include "pico_lfs_cli.h"
#include "pico_fatfs_cli.h"
#include "preset_manager_cli.h"
#include "pico_w_connection_manager_cli.h"
#include "midi2usbhub.h"
rppicomidi::Midi2usbhub_cli::Midi2usbhub_cli(Preset_manager* pm, Pico_w_connection_manager* cm)
{
    // Initialize the CLI
    EmbeddedCliConfig cli_config = {
        .rxBufferSize = 64,
        .cmdBufferSize = 64,
        .historyBufferSize = 128,
        .maxBindingCount = static_cast<uint16_t>(6 +
                            Preset_manager_cli::get_num_commands() +
                            Pico_lfs_cli::get_num_commands() +
                            Pico_fatfs_cli::get_num_commands() +
                            Pico_w_connection_manager_cli::get_num_commands()),
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
    Preset_manager_cli pm_cli(cli, pm);
    Pico_lfs_cli lfs_cli(cli);
    Pico_fatfs_cli fatfs_cli(cli);
    Pico_w_connection_manager_cli connection_manager_cli(cli);
    connection_manager_cli.setup_cli(cm);
}

void rppicomidi::Midi2usbhub_cli::task()
{
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT)
    {
        embeddedCliReceiveChar(cli, c);
        embeddedCliProcess(cli);
    }
}

void rppicomidi::Midi2usbhub_cli::static_list(EmbeddedCli *, char *, void *)
{
    printf("USB ID      Port  Direction Nickname     Product Name\n");

    for (size_t addr = 1; addr <= CFG_TUH_DEVICE_MAX + 1; addr++)
    {
        auto dev = Midi2usbhub::instance().get_attached_device(addr);
        if (dev && dev->configured)
        {
            // printf("%04x-%04x    %d      %s    %s %s\r\n", dev.vid, dev.pid, 1, "FROM", "0944-0117-1 ", dev.product_name.c_str());
            for (auto in_port : Midi2usbhub::instance().get_midi_in_port_list())
            {
                if (in_port->devaddr == addr)
                {
                    printf("%04x-%04x    %-2d     %s    %-12s %s\r\n", dev->vid, dev->pid, in_port->cable + 1,
                           "FROM", in_port->nickname.c_str(), dev->product_name.c_str());
                    for (auto out_port : Midi2usbhub::instance().get_midi_out_port_list())
                    {
                        if (out_port->devaddr == addr && out_port->cable == in_port->cable)
                        {
                            printf("%04x-%04x    %-2d     %s    %-12s %s\r\n", dev->vid, dev->pid,
                                   out_port->cable + 1,
                                   " TO ", out_port->nickname.c_str(), dev->product_name.c_str());
                            break;
                        }
                    }
                }
            }
        }
    }
}

void rppicomidi::Midi2usbhub_cli::static_connect(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2) {
        printf("connect <FROM nickname> <TO nickname>\r\n");
        return;
    }
    auto from_nickname = std::string(embeddedCliGetToken(args, 1));
    auto to_nickname = std::string(embeddedCliGetToken(args, 2));
    switch (Midi2usbhub::instance().connect(from_nickname, to_nickname)) {
        case 0:
            printf("%s connect to %s: successful\r\n",
                           from_nickname.c_str(), to_nickname.c_str());
            break;
        case -1:
            printf("TO nickname %s not found\r\n", to_nickname.c_str());
            break;
        case -2:
            printf("FROM nickname %s not found\r\n", from_nickname.c_str());
            break;
        default:
            printf("unknown return from connect()\r\n");
            break;
    }
}

void rppicomidi::Midi2usbhub_cli::static_disconnect(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2) {
        printf("disconnect <FROM nickname> <TO nickname>\r\n");
        return;
    }
    auto from_nickname = std::string(embeddedCliGetToken(args, 1));
    auto to_nickname = std::string(embeddedCliGetToken(args, 2));
    switch (Midi2usbhub::instance().disconnect(from_nickname, to_nickname)) {
        case 0:
            printf("%s disconnect to %s: successful\r\n",
                           from_nickname.c_str(), to_nickname.c_str());
            break;
        case -1:
            printf("TO nickname %s not found\r\n", to_nickname.c_str());
            break;
        case -2:
            printf("FROM nickname %s not found\r\n", from_nickname.c_str());
            break;
        default:
            printf("unknown return from disconnect()\r\n");
            break;
    }
}

void rppicomidi::Midi2usbhub_cli::static_reset(EmbeddedCli *, char *, void *)
{
    rppicomidi::Midi2usbhub::instance().reset();
}

void rppicomidi::Midi2usbhub_cli::static_show(EmbeddedCli *, char *, void *)
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

        for (auto midi_out : Midi2usbhub::instance().get_midi_out_port_list())
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
    for (size_t col = 0; col < Midi2usbhub::instance().get_midi_out_port_list().size(); col++)
    {
        printf("---+");
    }
    printf("\r\n");
    for (auto midi_in : Midi2usbhub::instance().get_midi_in_port_list())
    {
        printf("%-12s|", midi_in->nickname.c_str());
        for (auto midi_out : Midi2usbhub::instance().get_midi_out_port_list())
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
        for (size_t col = 0; col < Midi2usbhub::instance().get_midi_out_port_list().size(); col++)
        {
            printf("---+");
        }
        printf("\r\n");
    }
}

void rppicomidi::Midi2usbhub_cli::static_rename(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 2) {
        printf("usage: rename <Old Nickname> <New Nickname>\r\n");
        return;
    }
    auto old_nickname = std::string(embeddedCliGetToken(args, 1));
    auto new_nickname = std::string(embeddedCliGetToken(args, 2));
    switch (Midi2usbhub::instance().rename(old_nickname, new_nickname)) {
        case -1:
            printf("New Nickname %s already in use\r\n", new_nickname.c_str());
            break;
        case -2:
            printf("Old Nickname %s not found\r\n", old_nickname.c_str());
            break;
        case 1:
            printf("FROM Nickname %s set to %s\r\n", old_nickname.c_str(), new_nickname.c_str());
            break;
        case 2:
            printf("TO Nickname %s set to %s\r\n", old_nickname.c_str(), new_nickname.c_str());
            break;
        default:
            printf("unknown return from rename()\r\n");
            break;
    }
}
