/**
 * Copyright (c) 2023 rppicomidi
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 */

#include "wifi_scan_menu.h"

rppicomidi::Wifi_scan_menu::Wifi_scan_menu(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_) :
    View{screen_, screen_.get_clip_rect()},
    font{screen.get_font_12()},
    wifi{wifi_}, vm{vm_},
    ssids{screen, (uint8_t)(font.height*2+4), this, ssid_select_cb},
    selected_ssid_idx{-1},
    password{screen, "Password", 64, "\t", this, password_cb, false}
{

}

void rppicomidi::Wifi_scan_menu::entry()
{
    ssids.clear();
    wifi->register_scan_complete_callback(scan_complete_cb, this);
    wifi->register_link_up_callback(link_up_cb, this);
    wifi->register_link_down_callback(link_down_cb, this);
    wifi->register_link_error_callback(link_err_cb, this);
    last_scan_results.clear();
    selected_ssid_idx = -1;
    wifi->start_scan();
}

void rppicomidi::Wifi_scan_menu::exit()
{
    wifi->register_scan_complete_callback(nullptr, nullptr);
    wifi->register_link_up_callback(nullptr, nullptr);
    wifi->register_link_down_callback(nullptr, nullptr);
    wifi->register_link_error_callback(nullptr, nullptr);
}

void rppicomidi::Wifi_scan_menu::scan_complete_cb(void* context_)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    auto discovered = me->wifi->get_discovered_ssids();
    for (auto found: *discovered) {
        // only add non-hidden access point SSIDs that have names 32 chars or less and are either open or WPA or WPA2
        if (found.ssid_len != 0 && found.ssid_len <= 32 && found.auth_mode != me->wifi->WEP) {
            auto item = new Menu_item((std::string("RSSI:") + std::to_string(found.rssi)+
                (found.auth_mode == 0 ? " Open" : " Secure")).c_str(), me->screen, me->font);
            me->ssids.add_menu_item(item);
            me->last_scan_results.push_back(found);
        }
    }
    me->ssids.entry();
    me->draw();
}

void rppicomidi::Wifi_scan_menu::link_up_cb(void* context_)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    me->draw();
}

void rppicomidi::Wifi_scan_menu::link_down_cb(void* context_)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    me->draw();
}

void rppicomidi::Wifi_scan_menu::link_err_cb(void* context_, const char*)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    me->draw();
}

void rppicomidi::Wifi_scan_menu::draw()
{
    screen.clear_canvas();
    auto state = wifi->get_state();
    if (state == wifi->SCAN_REQUESTED || state == wifi->SCANNING) {
        screen.center_string(font, "Scanning...",font.height);
    }
    #if 0
    else if (state == wifi->INITIALIZED) {
        auto err = wifi->get_last_link_error();
        if (strlen(err) > 0) {
            std::string ssid;
            wifi->get_current_ssid(ssid);
            screen.center_string_on_two_lines(font, ssid.c_str(), 0);
            screen.center_string(font, err,font.height*2+4);
        }
        else {
            screen.center_string(font, "Wi-Fi On",font.height);
        }
    }
    #endif
    else if (state == wifi->SCAN_COMPLETE) {
        int idx = ssids.get_current_item_idx();
        std::string ssid{(const char*)(last_scan_results[idx].ssid), last_scan_results[idx].ssid_len};
        screen.center_string_on_two_lines(font, ssid.c_str(), 0);
        ssids.draw();
    }
    #if 0
    else if (state == wifi->CONNECTED || state == wifi->CONNECTION_REQUESTED) {
        std::string ssid;
        wifi->get_current_ssid(ssid);
        screen.center_string_on_two_lines(font, ssid.c_str(), 0);
        if (state == wifi->CONNECTED) {
            std::string addr;
            wifi->get_ip_address_string(addr);
            addr = std::string("IP:")+addr;
            screen.draw_string(font, 0, font.height*2+4, addr.c_str(), addr.size(), Pixel_state::PIXEL_ONE, Pixel_state::PIXEL_ZERO);
        }
        else {
            const char* req = "Connection Requested";
            screen.draw_string(font, 0, font.height*2+4, req, strlen(req), Pixel_state::PIXEL_ONE, Pixel_state::PIXEL_ZERO);
        }
    }
    else {
        const char* err = "Turn On Wi-Fi first";
        screen.center_string(font, err, 26);
    }
    #endif
}

void rppicomidi::Wifi_scan_menu::ssid_select_cb(View* context_, int idx_)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    me->selected_ssid_idx = idx_;
    if (me->selected_ssid_idx >= 0) {
        std::string ssid{(const char*)(me->last_scan_results[idx_].ssid), me->last_scan_results[idx_].ssid_len};

        if (me->last_scan_results[idx_].auth_mode == Pico_w_connection_manager::OPEN) {
            // we have all the info we need to request a connection
            me->wifi->set_current_ssid(ssid);
            me->wifi->set_current_security(me->last_scan_results[idx_].auth_mode);

            me->wifi->set_current_passphrase("");
            me->wifi->connect();
            // Connection requested;
            me->vm->go_home();
        }
        else {
            // Go get a password; pre-populate with the last known password for this SSID
            auto all_known = me->wifi->get_known_ssids();
            std::string pw = "";
            for (auto known: all_known)  {
                if (known.ssid == ssid) {
                    pw = known.passphrase;
                    break;
                }
            }
            me->password.set_text(pw.c_str());
            me->vm->push_view(&me->password);
        }
    }
}

void rppicomidi::Wifi_scan_menu::password_cb(View* context_, bool selected_)
{
    auto me = reinterpret_cast<Wifi_scan_menu*>(context_);
    const char* pw = me->password.get_text();
    if (selected_ && pw && strlen(pw) > 0) {
        size_t idx = me->selected_ssid_idx;
        std::string ssid{(const char*)(me->last_scan_results[idx].ssid), me->last_scan_results[idx].ssid_len};

        // we have all the info we need to request a connection
        me->wifi->set_current_ssid(ssid);
        me->wifi->set_current_security(me->last_scan_results[idx].auth_mode);
        me->wifi->set_current_passphrase(std::string(pw));
        me->wifi->connect();
        // Connection requested;
        me->vm->go_home();
    }
}

rppicomidi::View::Select_result rppicomidi::Wifi_scan_menu::on_select(View** new_view)
{
    if (last_scan_results.size() > 0)
        ssids.on_select(new_view);
    return Select_result::no_op;
}

void rppicomidi::Wifi_scan_menu::on_increment(uint32_t delta, bool is_shifted)
{
    if (last_scan_results.size()) {
        ssids.on_increment(delta, is_shifted);
        draw();
    }
}
void rppicomidi::Wifi_scan_menu::on_decrement(uint32_t delta, bool is_shifted)
{
    if (last_scan_results.size()) {
        ssids.on_decrement(delta, is_shifted);
        draw();
    }
}