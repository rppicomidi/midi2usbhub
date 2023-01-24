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
#include "wifi_manual_setup_view.h"

rppicomidi::Wifi_manual_setup_view::Wifi_manual_setup_view(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, View_manager* vm_) :
    View{screen_, screen_.get_clip_rect()},
    font{screen.get_font_12()},
    wifi{wifi_}, vm{vm_},
    ssids{screen, (uint8_t)(font.height*2+4), this, ssid_select_cb},
    selected_ssid_idx{-1},
    ssid{screen, "SSID", 32, "\t", this, ssid_cb, false},
    password{screen, "Password", 64, "\t", this, password_cb, false}
{

}

void rppicomidi::Wifi_manual_setup_view::entry()
{
    state = Saved_or_new;
    ssids.clear();
    known_ssids.clear();
    known_ssids = wifi->get_known_ssids();
    for (auto known: known_ssids) {
        std::string text = "Saved ";
        if (known.security & Pico_w_connection_manager::WPA2) {
            text += "WPA2";
        }
        else if (known.security & Pico_w_connection_manager::WPA) {
            text += "WPA";
        }
        else {
            text += "OPEN";
        }
        auto item = new Menu_item(text.c_str(), screen, font);
        ssids.add_menu_item(item);
    }
    auto item = new Menu_item("New WPA2...", screen, font);
    ssids.add_menu_item(item);
    item = new Menu_item("New WPA...", screen, font);
    ssids.add_menu_item(item);
    item = new Menu_item("New OPEN...", screen, font);
    ssids.add_menu_item(item);
    ssids.entry();
}

void rppicomidi::Wifi_manual_setup_view::ssid_select_cb(View* context_, int idx_)
{
    auto me = reinterpret_cast<Wifi_manual_setup_view*>(context_);
    if (idx_ >= 0) {
        if (idx_ < (int)me->known_ssids.size()) {
            // known SSID was selected; we have all the info we need to request a connection
            me->wifi->set_current_ssid(me->known_ssids[idx_].ssid);
            me->wifi->set_current_security(me->known_ssids[idx_].security);
            me->wifi->set_current_passphrase(me->known_ssids[idx_].passphrase);
            me->wifi->connect();
            // Connection requested;
            me->vm->go_home();
        }
        else {
            // creating a new SSID; set up security value first based on menu selection
            int sec_idx = idx_ - (int)me->known_ssids.size();
            if (sec_idx == 0) {
                me->auth = Pico_w_connection_manager::WPA2;
            }
            else if (sec_idx == 1) {
                me->auth = Pico_w_connection_manager::WPA;
            }
            else {
                me->auth = Pico_w_connection_manager::OPEN;
            }

            me->ssid.set_text("");
            me->password.set_text("");
            me->state = Entering_ssid;
            me->ssid.entry();
            me->draw();
        }
    }
}

void rppicomidi::Wifi_manual_setup_view::ssid_cb(View* context_, bool select_)
{
    auto me = reinterpret_cast<Wifi_manual_setup_view*>(context_);
    me->state = Saved_or_new;
    if (select_ && strlen(me->ssid.get_text())) {
        me->new_ssid = std::string(me->ssid.get_text());

        if (me->auth == Pico_w_connection_manager::OPEN) {
            // we have all we need to connect now
            me->wifi->set_current_security(me->auth);
            me->wifi->set_current_ssid(me->new_ssid);
            me->wifi->set_current_passphrase("");
            me->wifi->connect();
            me->vm->go_home();
        }
        else {
            // need to get a password
            me->ssid.exit();
            me->state = Entering_password;
            me->password.set_text("");
            me->vm->push_view(&(me->password));
        }
    }
    else {
        // cancelled
        me->new_ssid = "";
    }
}

void rppicomidi::Wifi_manual_setup_view::password_cb(View* context_, bool select_)
{
    auto me = reinterpret_cast<Wifi_manual_setup_view*>(context_);
    me->state = Saved_or_new; // either way, done now
    if (select_ && strlen(me->password.get_text())) {
        // got a password; now can connect
        me->wifi->set_current_security(me->auth);
        me->wifi->set_current_ssid(me->new_ssid);
        me->wifi->set_current_passphrase(std::string(me->password.get_text()));
        me->wifi->connect();
        me->vm->go_home();
    }
    else {
        // cancelled
        me->new_ssid = "";
    }
}

rppicomidi::View::Select_result rppicomidi::Wifi_manual_setup_view::on_select(View** new_view)
{
    switch(state) {
    default:
    case Saved_or_new:
        ssids.on_select(new_view);
        break;
    case Entering_ssid:
        ssid.on_select(new_view);
        break;
    case Entering_password:
        password.on_select(new_view);
        break;
    }
    return no_op;
}

rppicomidi::View::Select_result rppicomidi::Wifi_manual_setup_view::on_back(View** new_view)
{
    switch(state) {
    default:
    case Saved_or_new:
        return Select_result::exit_view;
        break;
    case Entering_ssid:
        state = Saved_or_new;
        ssid.set_text("");
        draw();
        break;
    case Entering_password:
        password.on_select(new_view);
        break;
    }
    return no_op;
}

void rppicomidi::Wifi_manual_setup_view::on_increment(uint32_t delta, bool is_shifted)
{
    if (state == Saved_or_new) {
        ssids.on_increment(delta, is_shifted);
        draw();
    }
}
void rppicomidi::Wifi_manual_setup_view::on_decrement(uint32_t delta, bool is_shifted)
{
    if (state == Saved_or_new) {
        ssids.on_decrement(delta, is_shifted);
        draw();
    }
}

void rppicomidi::Wifi_manual_setup_view::on_left(uint32_t delta, bool is_shifted)
{
    if (state == Entering_ssid) {
        ssid.on_left(delta, is_shifted);
    }
}

void rppicomidi::Wifi_manual_setup_view::on_right(uint32_t delta, bool is_shifted)
{
    if (state == Entering_ssid) {
        ssid.on_right(delta, is_shifted);
    }
}

 void rppicomidi::Wifi_manual_setup_view::on_key(uint8_t key_code, uint8_t modifiers, bool pressed)
 {
    if (state == Entering_ssid) {
        ssid.on_key(key_code, modifiers, pressed);
    }
 }

void rppicomidi::Wifi_manual_setup_view::draw()
{
    if (state == Saved_or_new) {
        screen.clear_canvas();
        size_t idx = ssids.get_current_item_idx();
        if (idx < known_ssids.size())
            screen.center_string_on_two_lines(font, known_ssids[idx].ssid.c_str(), 0);
        else if (idx == known_ssids.size()) {
            screen.center_string_on_two_lines(font, "New SSID with WPA2 Security", 0);
        }
        else if (idx == known_ssids.size()+1) {
            screen.center_string_on_two_lines(font, "New SSID with WPA Security", 0);
        }
        else {
            screen.center_string_on_two_lines(font, "New SSID with OPEN Security", 0);
        }
        ssids.draw();
    }
    else if (state == Entering_ssid) {
        ssid.draw();
    }
}