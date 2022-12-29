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
#include <stdio.h>
#include <assert.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico_w_connection_manager_cli.h"
#include "getsn.h"
void rppicomidi::Pico_w_connection_manager_cli::on_initialize(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli; // unused parameter
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->initialize()) {
        printf("WiFi driver initialized\r\n");
    }
    else {
        printf("WiFi driver initialization failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_deinitialize(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli; // unused parameter
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->deinitialize()) {
        printf("WiFi driver deinitialized\r\n");
    }
    else {
        printf("WiFi driver deinitialization failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_country_code(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli; // unused parameter
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (embeddedCliGetTokenCount(args) == 0) {
        std::string code, country;
        me->get_country_code(code);
        me->get_country_from_code(code, country);
        printf("Country code is %s: %s\r\n", code.c_str(), country.c_str());
    }
    else if (embeddedCliGetTokenCount(args) == 1) {
        std::string code = std::string(embeddedCliGetToken(args,1));
        for (auto& c:code) {
            c = std::toupper(c);
        }
        std::string country;
        if (me->set_country_code(code)) {
            me->get_country_from_code(code, country);
            printf("Successfully set country code to %s: %s\r\n", code.c_str(), country.c_str());
        }
    }
    else {
        printf("usage: wifi-country [country code]\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_list_countries(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    std::vector<std::string> code_list;
    me->get_all_country_codes(code_list);
    printf("Country:Code\r\n");
    for (auto code:code_list) {
        printf("%s\r\n", code.c_str());
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_save_settings(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->save_settings()) {
        printf("wifi settings saved successfully\r\n");
    }
    else {
        printf("wifi settings save failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_load_settings(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->load_settings()) {
        printf("wifi settings loaded successfully\r\n");
    }
    else {
        printf("wifi settings load failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::scan_complete(void* context)
{
    printf("*************** Scan completed *************** \r\n");
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    auto discovered_ssids = me->get_discovered_ssids();
    for (auto it: *discovered_ssids) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
            it.ssid, it.rssi, it.channel, it.bssid[0], it.bssid[1], it.bssid[2], it.bssid[3], it.bssid[4], it.bssid[5], it.auth_mode);
    }
    me->register_scan_complete_callback(nullptr, 0);
}

void rppicomidi::Pico_w_connection_manager_cli::on_start_scan(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->get_state() == Pico_w_connection_manager::DEINITIALIZED) {
        printf("You must run wifi-initialize before running wifi-scan\r\n");
        return;
    }
    me->register_scan_complete_callback(scan_complete, me);
    printf("*********** WiFi Scan Requested ************* \r\n");

    if (me->start_scan()) {
        printf("*********** Starting WiFi Scan ************* \r\n");
    }
    else {
        printf("Scan already in progress\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_disconnect(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->disconnect()) {
        printf("disconnect successful\r\n");
    }
    else {
        printf("disconnect failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::scan_connect_complete(void *context)
{
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    me->register_scan_complete_callback(nullptr, nullptr); // unregister the scan complete callback
    const std::vector<cyw43_ev_scan_result_t>* discovered_ssids = me->get_discovered_ssids();
    if (discovered_ssids->size() == 0) {
        printf("No discovered SSIDs. Cannot connect.\r\n");
        return;
    }
    size_t idx = 0;
    printf("Index RSSI SSID\r\n");
    for (auto it: *discovered_ssids) {
        printf("%5d %4d %s\r\n", idx++, it.rssi, it.ssid);
    }
    printf("Type index of SSID to connect and press [Enter]\r\n");
    char line[128];
    if (getsn(line, sizeof(line)) > 0) {
        idx = atoi(line);
        if (idx >= discovered_ssids->size()) {
            printf("invalid index %u\r\n", idx);
            return;
        }
        me->set_current_ssid(std::string((const char*)discovered_ssids->at(idx).ssid));
        me->set_current_security(discovered_ssids->at(idx).auth_mode);
        if (discovered_ssids->at(idx).auth_mode != 0) {
            const std::vector<Pico_w_connection_manager::Ssid_info> known = me->get_known_ssids();
            line[0] = '\0';
            for (auto it : known) {
                if (it.ssid == std::string((const char*)discovered_ssids->at(idx).ssid) &&
                        it.security == discovered_ssids->at(idx).auth_mode) {
                    strncpy(line,it.passphrase.c_str(), sizeof(line)-1);
                    line[sizeof(line)-1] = 0;
                    break;
                }
            }
            if (strlen(line) == 0) {
                // password is not stored. Need to get it
                printf("Enter password for %s\r\n", discovered_ssids->at(idx).ssid);
                if (getsn(line, sizeof(line)) == 0) {
                    printf("Error entering password\r\n");
                    return;
                }
            }
            me->set_current_passphrase(std::string(line));
        }
        if (!me->connect()) {
            printf("error trying to connect to SSID %s\r\n", discovered_ssids->at(idx).ssid);
        }
        else {
            printf("Requesting connection to %s\r\n", discovered_ssids->at(idx).ssid);
        }
    }

}

void rppicomidi::Pico_w_connection_manager_cli::on_scan_connect(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (!me->load_settings()) {
        printf("settings failed to load; using defaults\r\n");
    }
    if (me->get_state() == Pico_w_connection_manager::DEINITIALIZED && !me->initialize()) {
        printf("cannot initialize Wi-Fi\r\n");
        return;
    }
    me->register_scan_complete_callback(scan_connect_complete, me);
    printf("*********** WiFi Scan Requested ************* \r\n");

    if (me->start_scan()) {
        printf("*********** Starting WiFi Scan ************* \r\n");
    }
    else {
        printf("Scan already in progress\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_connect(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    uint16_t argc = embeddedCliGetTokenCount(args);
    if (argc == 0) {
        printf("usage: connect SSID <OPEN|WPA|WPA2|MIXED> [passphrase]\r\n");
        return;
    }
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    bool settings_loaded = me->load_settings();
    if (!settings_loaded) {
        printf("settings failed to load; using defaults\r\n");
    }
    if (me->get_state() == Pico_w_connection_manager::DEINITIALIZED && !me->initialize()) {
        printf("cannot initialize Wi-Fi\r\n");
        return;
    }
    int security = 0;
    if (argc == 1) {
        std::string ssid = std::string(embeddedCliGetToken(args, 1));
        if (settings_loaded) {
            const std::vector<Pico_w_connection_manager::Ssid_info> known = me->get_known_ssids();
            for (auto it: known) {
                if (it.ssid == ssid) {
                    me->set_current_ssid(ssid);
                    me->set_current_passphrase(it.passphrase);
                    me->set_current_security(it.security);
                    if (me->connect()) {
                        printf("connection to %s requested\r\n", ssid.c_str());
                    }
                    else {
                        printf("failed to connect to %s\r\n", ssid.c_str());
                    }
                    return;
                }
            }
        }
        printf("Security info for SSID=%s not found\r\n", ssid.c_str());
    }
    else if (argc <= 3) { // i.e., == 2 or == 3
        std::string ssid = std::string(embeddedCliGetToken(args, 1));
        std::string auth = std::string(embeddedCliGetToken(args, 2));
        if (auth != "OPEN") {
            if (argc == 3) {
                if (auth == "WPA") {
                    security = Pico_w_connection_manager::WPA;
                }
                else if (auth == "WPA2") {
                    security = Pico_w_connection_manager::WPA2;
                }
                else if (auth == "MIXED") {
                    security = Pico_w_connection_manager::MIXED;
                }
                else {
                    printf("Illegal auth type %s\r\n", auth.c_str());
                    printf("usage: connect SSID [OPEN|WPA|WPA2|MIXED] [passphrase]\r\n");
                    return;
                }
                me->set_current_passphrase(std::string(embeddedCliGetToken(args, 3)));
            }
            else {
                printf("usage: connect SSID <OPEN|WPA|WPA2|MIXED> [passphrase]\r\n");
                return;
            }
        }
        else {
            printf("usage: connect SSID <OPEN|WPA|WPA2|MIXED> [passphrase]\r\n");
            return;
        }
        me->set_current_ssid(ssid);
        me->set_current_security(security);
        if (me->connect()) {
            printf("connection to %s requested\r\n", ssid.c_str());
        }
        else {
            printf("failed to connect to %s\r\n", ssid.c_str());
        }
    }
    else { // > 3
        printf("usage: connect SSID <OPEN|WPA|WPA2|MIXED> [passphrase]\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_autoconnect(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (!me->autoconnect()) {
        printf("autoconnect failed\r\n");
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_link_status(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    if (me->get_state() == Pico_w_connection_manager::DEINITIALIZED)
        printf("WiFi not initialized\r\n");
    else if (me->is_link_up()) {
        link_up(context);
    }
    else {
        std::string ssid;
        me->get_current_ssid(ssid);
        printf("%s WiFi link is not up, status is: ", ssid.c_str());
        int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        switch(status) {
            case CYW43_LINK_DOWN:
                printf("link down\r\n");
                break;
            case CYW43_LINK_JOIN:
                printf("connecting\r\n");
                break;
            case CYW43_LINK_NOIP:
                printf("connected, no IP address\r\n");
                break;
            case CYW43_LINK_BADAUTH:
                printf("not authorized\r\n");
                break;
            case CYW43_LINK_NONET:
                printf("cannot find SSID\r\n");
                break;
            case CYW43_LINK_FAIL:
                printf("link failure\r\n");
                break;
            default:
                printf("unknown error\r\n");
                break;
        }
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_rssi(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me = reinterpret_cast<Pico_w_connection_manager*>(context);
    auto rssi = me->get_rssi();
    std::string ssid;
    me->get_current_ssid(ssid);
    if (rssi == INT_MIN) {
        printf("Error reading RSSI\r\n");
    }
    else {
        printf("%s RSSI=%d\r\n", ssid.c_str(), rssi);
    }
}

void rppicomidi::Pico_w_connection_manager_cli::setup_cli(Pico_w_connection_manager* wifi)
{
    // Make sure WIFI settings are consistent because some methods force setting load
    if (wifi->get_settings_saved_state() == Pico_w_connection_manager::UNKNOWN)
        wifi->load_settings();
    assert(embeddedCliAddBinding(cli, {
            "wifi-country",
            "Set or get the WIFI country code; usage wifi-country [2-letter code to set]",
            true,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_country_code
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-connect",
            "Connect to AP with known SSID; usage wifi-connect SSID [OPEN|WPA|WPA2|MIXED] [Passphrase]",
            true,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_connect
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-deinitialize",
            "Shut down WiFi driver; usage wifi-deinitialize",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_deinitialize
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-initialize",
            "Turn on WiFi driver; usage wifi-initialize",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_initialize
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-list-countries",
            "List all supported country codes; usage wifi-list-countries",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_list_countries
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-save",
            "Save current WiFi settings; usage wifi-save",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_save_settings
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-load",
            "Load WiFi settings; usage wifi-load",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_load_settings
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-scan",
            "Start WiFi SSID scan; usage wifi-scan",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_start_scan
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-scan-connect",
            "Scan, then connect to a discovered SSID; usage wifi-scan-connect",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_scan_connect
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-disconnect",
            "Disconnect or stop reconnect; usage wifi-disconnect",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_disconnect
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-autoconnect",
            "Connect to last saved SSID; usage wifi-autoconnect",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_autoconnect
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-link-status",
            "Report current WiFi link status; usage wifi-link-status",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_link_status
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-rssi",
            "Report current WiFi link RSSI; usage wifi-rssi",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_rssi
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-list-known",
            "List previously known SSIDs; usage wifi-list-known",
            false,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_list_known_aps
    }));
    assert(embeddedCliAddBinding(cli, {
            "wifi-forget",
            "Remove SSID from known SSIDs; usage wifi-forget [SSID]",
            true,
            wifi,
            rppicomidi::Pico_w_connection_manager_cli::on_forget_ap
    }));
    wifi->register_link_up_callback(link_up, wifi);
    wifi->register_link_down_callback(link_down, wifi);

}

void rppicomidi::Pico_w_connection_manager_cli::link_up(void* context)
{
    auto me=reinterpret_cast<Pico_w_connection_manager*>(context);
    std::string ip;
    std::string ssid;
    me->get_ip_address_string(ip);
    me->get_current_ssid(ssid);
    printf("%s Link Up IP address=%s\r\n", ssid.c_str(), ip.c_str());
}

void rppicomidi::Pico_w_connection_manager_cli::link_down(void* context)
{
    auto me=reinterpret_cast<Pico_w_connection_manager*>(context);
    std::string ssid;
    me->get_current_ssid(ssid);
    printf("%s Link Down\r\n", ssid.c_str());
}

void rppicomidi::Pico_w_connection_manager_cli::on_list_known_aps(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    auto me=reinterpret_cast<Pico_w_connection_manager*>(context);
    if (!me->load_settings()) {
        printf("Failed to load stored known SSID list\r\n");
        return;
    }
    auto known = me->get_known_ssids();
    if (known.size() == 0) {
        printf("Known AP list is empty\r\n");
        return;
    }
    std::string auth;
    printf("%-6s %s\r\n", "Auth", "SSID");
    for (auto it: known) {
        if (it.security == Pico_w_connection_manager::OPEN) {
            auth = "OPEN";
        }
        else if ((it.security & Pico_w_connection_manager::MIXED) == Pico_w_connection_manager::MIXED) {
            auth = "MIXED";
        }
        else if ((it.security & Pico_w_connection_manager::WPA2) == Pico_w_connection_manager::WPA2) {
            auth = "WPA2";
        }
        else if ((it.security & Pico_w_connection_manager::WPA) == Pico_w_connection_manager::WPA) {
            auth = "WPA";
        }
        else {
            auth = "ERROR";
        }
        printf("%-6s %s\r\n", auth.c_str(), it.ssid.c_str());
    }
}

void rppicomidi::Pico_w_connection_manager_cli::on_forget_ap(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    std::string ssid;
    auto me=reinterpret_cast<Pico_w_connection_manager*>(context);
    if (!me->load_settings()) {
        printf("Failed to load stored known SSID list\r\n");
        return;
    }
    auto known = me->get_known_ssids();
    if (known.size() == 0) {
        printf("Known AP list is empty\r\n");
        return;
    }
    size_t sel = 0;
    if (embeddedCliGetTokenCount(args) == 1) {
        ssid = std::string(embeddedCliGetToken(args, 1));
        for (auto it: known) {
            if (it.ssid == ssid) {
                break;
            }
            ++sel;
        }
        if (sel >= known.size()) {
            printf("SSID %s not found\r\n", ssid.c_str());
            return;
        }
    }
    else {
        printf("Index  SSID\r\n");
        for (auto it: known) {
            printf("%-5u %s\r\n", sel++, it.ssid.c_str());
        }
        printf("enter Index for SSID to forget\r\n");
        char line[128];
        getsn(line, sizeof(line));
        sel = std::atoi(line);
        if (sel < known.size()) {
            ssid=known[sel].ssid;
        }
        else {
            printf("Index %u is too big\r\n", sel);
            return;
        }
    }
    if (!me->erase_known_ssid_by_idx(sel)) {
        printf("Index %u is too big\r\n", sel);
    }
    else {
        if (!me->save_settings()) {
            printf("Error saving updated Known SSID list\r\n");
        }
        else {
            printf("SSID %s removed from known SSID list\r\n", ssid.c_str());
        }
    }
}