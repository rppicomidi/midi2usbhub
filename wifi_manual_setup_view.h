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
#include "text_entry_box.h"
#include "text_item_chooser_menu.h"
#include "pico_w_connection_manager.h"
#include "view_manager.h"

namespace rppicomidi {
class Wifi_manual_setup_view : public View
{
public:
    Wifi_manual_setup_view() = delete;
    ~Wifi_manual_setup_view() = default;
    Wifi_manual_setup_view(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_);

    void entry() final;
    //void exit() final;
    void draw() final;
    Select_result on_select(View** new_view) final;
    Select_result on_back(View** new_view) final;
    void on_left(uint32_t delta, bool is_shifted) final;
    void on_right(uint32_t delta, bool is_shifted) final;
    void on_increment(uint32_t delta, bool is_shifted) final;
    void on_decrement(uint32_t delta, bool is_shifted) final;
    void on_key(uint8_t key_code, uint8_t modifiers, bool pressed) final;
private:
    static void ssid_select_cb(View* context, int idx);
    static void ssid_cb(View*, bool);
    static void password_cb(View*, bool);
    Mono_mono_font font;
    Pico_w_connection_manager* wifi;
    View_manager* vm;
    Text_item_chooser_menu ssids;
    std::vector<Pico_w_connection_manager::Ssid_info> known_ssids;
    int selected_ssid_idx;
    Text_entry_box ssid;
    Text_entry_box password;
    int auth;
    std::string new_ssid;
    enum Manual_state {Saved_or_new, Entering_ssid, Entering_password} state;
};
}