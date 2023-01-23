/**
 * 
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
#include <cstring>
#include <cstdio>
#include "wifi_setup_menu.h"
#include "view_launch_menu_item.h"
#include "callback_menu_item.h"

rppicomidi::Wifi_setup_menu::Wifi_setup_menu(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_) :
    View{screen_, screen_.get_clip_rect()},
    font{screen.get_font_12()},
    menu{screen, static_cast<uint8_t>(font.height), font},
    country_code_chooser{screen, 0, this, country_callback},
    forget_ssid_chooser{screen, 0, this, forget_ssid_cb},
    wifi{wifi_}, vm{vm_}, ssid_entry_view{screen, "Enter SSID", 32, "", this, ssid_done_cb, false},
    scan_view{screen, wifi, vm},
    manual_view{screen, wifi, vm}
{
}

void rppicomidi::Wifi_setup_menu::entry()
{
    menu.clear();
    country_code_chooser.clear();
    auto onoff = new Callback_menu_item(get_on_off_text(), screen, font, this, toggle_wifi_init);
    menu.add_menu_item(onoff);
    std::string code;
    wifi->get_country_code(code);
    const char* country = wifi->get_country_from_code(code);
    std::vector<std::string> codes;
    int idx = 0;
    int jdx = 0;
    wifi->get_all_country_codes(codes);
    for (auto cc: codes) {
        const char* cntry = wifi->get_country_from_code(cc.substr(cc.find(':')+1, 2));
        auto item = new Menu_item(cntry, screen, font);
        country_code_chooser.add_menu_item(item);
        if (strncmp(country, cntry, strlen(country)) == 0) {
            idx = jdx;
        }
        ++jdx;
    }
    country_code_chooser.set_current_item_idx(idx);
    auto ccc = new View_launch_menu_item(country_code_chooser, country, screen, font);
    menu.add_menu_item(ccc);

    auto vlmi = new View_launch_menu_item(scan_view, "SSID Scan...", screen, font);
    menu.add_menu_item(vlmi);
    forget_ssid_chooser.clear();
    auto known_ssids = wifi->get_known_ssids();
    for (auto known: known_ssids) {
        forget_ssid_chooser.add_menu_item(new Menu_item(known.ssid.c_str(), screen, font));
    }
    vlmi = new View_launch_menu_item(forget_ssid_chooser, "Forget SSID...", screen, font);
    menu.add_menu_item(vlmi);

    vlmi = new View_launch_menu_item(manual_view, "Manual SSID...", screen, font);
    menu.add_menu_item(vlmi);

    menu.entry();
}

void rppicomidi::Wifi_setup_menu::exit()
{
    menu.clear();
    country_code_chooser.clear();
}

void rppicomidi::Wifi_setup_menu::country_callback(View* context, int selected)
{
    auto me = reinterpret_cast<Wifi_setup_menu*>(context);
    std::vector<std::string> codes;
    me->wifi->get_all_country_codes(codes);
    if (selected >=0 && (size_t)selected < codes.size()) {
        if (me->wifi->set_country_code(codes.at(selected).substr(codes.at(selected).find(':')+1, 2))) {
            if (me->wifi->get_state() != me->wifi->DEINITIALIZED) {
                // re-init radio with new country code
                me->wifi->deinitialize();
                me->wifi->initialize();
            }
        }
    }
}

const char* rppicomidi::Wifi_setup_menu::get_on_off_text()
{
    return wifi->get_state() != wifi->DEINITIALIZED ? "Wi-Fi On" : "Wi-Fi Off";
}

void rppicomidi::Wifi_setup_menu::toggle_wifi_init(View* context, View**)
{
    auto me = reinterpret_cast<Wifi_setup_menu*>(context);
    auto state = me->wifi->get_state();
    if (state != me->wifi->DEINITIALIZED) {
        me->wifi->deinitialize();
    }
    else {
        me->wifi->initialize();
    }
    state = me->wifi->get_state();
    me->menu.set_menu_item_text(me->menu.get_current_item_idx(), me->get_on_off_text());
    me->draw();
}

void rppicomidi::Wifi_setup_menu::draw()
{
    screen.clear_canvas();
    screen.center_string(font, "Wi-Fi Setup", 0);
    menu.draw();
}

void rppicomidi::Wifi_setup_menu::ssid_done_cb(View* view, bool ssid_ok_)
{
    auto me = reinterpret_cast<Wifi_setup_menu*>(view);
    if (ssid_ok_) {
        const char* text = me->ssid_entry_view.get_text();

        printf("SSID Text OK: %s Length=%u\r\n", text, strlen(text));
    }
    else {
        printf("SSID Text Canceled\r\n");
    }
}

void rppicomidi::Wifi_setup_menu::forget_ssid_cb(View* view, int selected_idx)
{
    auto me = reinterpret_cast<Wifi_setup_menu*>(view);
    if (selected_idx >= 0) {
        me->wifi->erase_known_ssid_by_idx(selected_idx);
    }

}