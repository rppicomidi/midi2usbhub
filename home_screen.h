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
#include "preset_manager.h"
#include "wifi_setup_menu.h"
#include "callback_menu_item.h"
#include "text_item_chooser_menu.h"
namespace rppicomidi
{
class Home_screen : public View
{
public:

    Home_screen(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, Preset_manager* pm_, View_manager* vm_);

    virtual ~Home_screen()=default;

    void draw() final;
    Select_result on_select(View** new_view) final;
    void on_increment(uint32_t delta, bool is_shifted) final {menu.on_increment(delta, is_shifted); };
    void on_decrement(uint32_t delta, bool is_shifted) final {menu.on_decrement(delta, is_shifted); };
    void entry() final;
    void exit() final;

    /**
     * @brief Force the IP address menu item to update based on the Wi-Fi connected state
     */
    void update_ipaddr_menu_item();

private:
    // Get rid of default constructor and copy constructor
    Home_screen() = delete;
    Home_screen(Home_screen&) = delete;
    void update_current_preset();
    static void ipaddr_callback(View*, View**);
    static void link_up_cb(void* context_);
    static void link_down_cb(void* context_);
    static void link_err_cb(void* context_, const char*);
    static void preset_chooser_cb(View* context_, int idx);
    static void preset_change_cb(void* context_);
    const Mono_mono_font& font;
    Menu menu;
    Pico_w_connection_manager* wifi;
    Preset_manager* pm;
    View_manager* vm;
    Wifi_setup_menu wifi_setup;
    Callback_menu_item* ipaddr_item;
    View_launch_menu_item* preset_item;
    Text_item_chooser_menu preset_chooser;
    std::vector<std::string> presets;
};
}