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
#include <cstring>
#include <cstdio>
#include "home_screen.h"

rppicomidi::Home_screen::Home_screen(Mono_graphics& screen_, Pico_w_connection_manager* wifi_, Preset_manager* pm_, View_manager* vm_) :
    View{screen_, screen_.get_clip_rect()},
    font{screen.get_font_12()},
    menu{screen, static_cast<uint8_t>(font.height*2+4), font},
    wifi{wifi_}, pm{pm_}, vm{vm_}, wifi_setup{screen, wifi, vm}
{
    ipaddr_item = new Callback_menu_item("",screen, font, this, ipaddr_callback);
    menu.add_menu_item(ipaddr_item);
    auto item = new View_launch_menu_item(wifi_setup, "Wi-Fi setup...", screen, font);
    menu.add_menu_item(item);
    preset_item = new Menu_item("No Preset",screen, font);
    menu.add_menu_item(preset_item);
}

void rppicomidi::Home_screen::entry()
{
    wifi->register_link_up_callback(link_up_cb, this);
    wifi->register_link_down_callback(link_down_cb, this);
    wifi->register_link_error_callback(link_err_cb, this);
    update_ipaddr_menu_item();
    menu.entry();
}

void rppicomidi::Home_screen::exit()
{
    wifi->register_link_up_callback(nullptr, nullptr);
    wifi->register_link_down_callback(nullptr, nullptr);
    wifi->register_link_error_callback(nullptr, nullptr);
}

void rppicomidi::Home_screen::link_up_cb(void* context_)
{
    auto me = reinterpret_cast<Home_screen*>(context_);
    me->update_ipaddr_menu_item();
    me->draw();
}

void rppicomidi::Home_screen::link_down_cb(void* context_)
{
    auto me = reinterpret_cast<Home_screen*>(context_);
    me->update_ipaddr_menu_item();
    me->draw();
}

void rppicomidi::Home_screen::link_err_cb(void* context_, const char*)
{
    auto me = reinterpret_cast<Home_screen*>(context_);
    me->update_ipaddr_menu_item();
    me->draw();
}

void rppicomidi::Home_screen::ipaddr_callback(View* context, View**)
{
    auto me = reinterpret_cast<Home_screen*>(context);
    auto state = me->wifi->get_state();
    switch (state) {
    case Pico_w_connection_manager::DEINITIALIZED:
        // turn Wi-Fi on
        me->wifi->initialize();
        break;
    case Pico_w_connection_manager::INITIALIZED:
    {
        std::string ssid;
        me->wifi->get_current_ssid(ssid);
        if (ssid.size() > 0) {
            me->wifi->autoconnect();
            me->update_ipaddr_menu_item();
            me->draw();
        }
        else
            me->wifi->deinitialize();
    }
        break;
    case Pico_w_connection_manager::CONNECTED:
        me->wifi->disconnect();
        break;
    case Pico_w_connection_manager::CONNECTION_REQUESTED:
    case Pico_w_connection_manager::SCAN_REQUESTED:
    case Pico_w_connection_manager::SCANNING:
    case Pico_w_connection_manager::SCAN_COMPLETE:
    default:
        break;
    }
    me->update_ipaddr_menu_item();
    me->draw();
}

void rppicomidi::Home_screen::update_ipaddr_menu_item()
{
    std::string ipaddr;
    
    auto state = wifi->get_state();
    switch (state) {
    case Pico_w_connection_manager::DEINITIALIZED:
        ipaddr = "Wi-Fi Off";
        break;
    case Pico_w_connection_manager::CONNECTION_REQUESTED:
        ipaddr = "Connecting...";
        break;
    case Pico_w_connection_manager::INITIALIZED:
    {
        std::string ssid;
        wifi->get_current_ssid(ssid);
        if (ssid.size() > 0) {
            const char* err = wifi->get_last_link_error();
            if (strlen(err) > 0) {
                ipaddr = err;
            }
            else {
                ipaddr = "Connect";
            }
        }
        else
            ipaddr = "Wi-Fi On";
    }
        break;
    case Pico_w_connection_manager::CONNECTED:
        wifi->get_ip_address_string(ipaddr);
        break;
    case Pico_w_connection_manager::SCAN_REQUESTED:
    case Pico_w_connection_manager::SCANNING:
    case Pico_w_connection_manager::SCAN_COMPLETE:
        ipaddr = "Scanning...";
        break;
    default:
        ipaddr = "Error";
        break;
    }
    ipaddr = "IP:" + ipaddr;
    ipaddr_item->set_text(ipaddr.c_str());
}

void rppicomidi::Home_screen::draw()
{
    std::string ssid;
    wifi->get_current_ssid(ssid);
    if (ssid.size() == 0) {
        ssid=std::string("No SSID");
    }
    screen.clear_canvas();
    screen.center_string_on_two_lines(font, ssid.c_str(), 0);
    menu.draw();
}

rppicomidi::Home_screen::Select_result rppicomidi::Home_screen::on_select(View** new_view)
{
    return menu.on_select(new_view);
}
