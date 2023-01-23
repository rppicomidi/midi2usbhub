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

#pragma once
#include "pico/mutex.h"
#include "mono_graphics_lib.h"
#include "view_manager.h"
#include "menu.h"
#include "view_launch_menu_item.h"
#include "pico_w_connection_manager.h"
#include "view_manager.h"
#include "text_item_chooser_menu.h"
#include "text_entry_box.h"
#include "wifi_scan_menu.h"
#include "wifi_manual_setup_view.h"
namespace rppicomidi
{
class Wifi_setup_menu : public View
{
public:

    Wifi_setup_menu(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_);

    virtual ~Wifi_setup_menu()=default;

    void draw() final;
    Select_result on_select(View** new_view) final {return menu.on_select(new_view);}
    void on_increment(uint32_t delta, bool is_shifted) final {menu.on_increment(delta, is_shifted); };
    void on_decrement(uint32_t delta, bool is_shifted) final {menu.on_decrement(delta, is_shifted); };
    void entry() final;
    void exit() final;
private:
    // Get rid of default constructor and copy constructor
    Wifi_setup_menu() = delete;
    Wifi_setup_menu(Wifi_setup_menu&) = delete;
    const char* get_on_off_text();

    static void country_callback(View* view, int selected_idx);
    static void forget_ssid_cb(View* view, int selected_idx);
    static void toggle_wifi_init(View* view, View**);
    static void ssid_done_cb(View* view, bool ssid_ok_);
    const Mono_mono_font& font;
    Menu menu;
    Text_item_chooser_menu country_code_chooser;
    Text_item_chooser_menu forget_ssid_chooser;
    Pico_w_connection_manager* wifi;
    View_manager* vm;
    Text_entry_box ssid_entry_view;
    Wifi_scan_menu scan_view;
    Wifi_manual_setup_view manual_view;
};
}