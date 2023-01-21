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
#include "view.h"
#include "pico_w_connection_manager.h"
#include "text_item_chooser_menu.h"
#include "text_entry_box.h"
#include "view_manager.h"
namespace rppicomidi
{
class Wifi_scan_menu : public View
{
public:
    Wifi_scan_menu() = delete;
    ~Wifi_scan_menu() = default;
    Wifi_scan_menu(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_);
    void entry() final;
    void exit() final;
    void draw() final;
    Select_result on_select(View** new_view) final;
    void on_increment(uint32_t delta, bool is_shifted) final;
    void on_decrement(uint32_t delta, bool is_shifted) final;
private: 
    static void ssid_select_cb(View* context, int idx);
    static void scan_complete_cb(void* context_);
    static void link_up_cb(void* context_);
    static void link_down_cb(void* context_);
    static void link_err_cb(void* context_, const char* err_);
    static void password_cb(View* context_, bool selected_);
    Mono_mono_font font;
    Pico_w_connection_manager* wifi;
    View_manager* vm;
    Text_item_chooser_menu ssids;
    std::vector<cyw43_ev_scan_result_t> last_scan_results;
    int selected_ssid_idx;
    Text_entry_box password;
    bool password_mode;
};
}