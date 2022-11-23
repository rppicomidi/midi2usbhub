/**
 * @file preset_manager.cpp
 * @brief this file supports loading and storing presets in
 * non-volatile storage
 *
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
#include "preset_manager.h"
#include "midi2usbhub.h"
rppicomidi::Preset_manager::Preset_manager()
{
    // Make sure the flash filesystem is working
    int error_code = pico_mount(false);
    if (error_code != 0) {
        printf("Error %s mounting the flash file system\r\nFormatting...", pico_errmsg(error_code));
        error_code = pico_mount(true);
        if (error_code != 0) {
            printf("Error %s mounting the flash file system\r\nFormatting...", pico_errmsg(error_code));
            exit(-1);
        }
        else {
            printf("completed\r\n");
        }
    }
    printf("successfully mounted flash file system\r\n");

    if (pico_unmount() != 0) {
        printf("Failed to unmount the flash file system\r\n");
    }
}

void rppicomidi::Preset_manager::init_cli(EmbeddedCli* cli)
{
    assert(embeddedCliAddBinding(cli, {
        "cat",
        "print file contents. usage: cat <fn>",
        true,
        this,
        static_print_file
    }));
    assert(embeddedCliAddBinding(cli, {
        "format",
        "format settings file system",
        false,
        this,
        static_file_system_format
    }));
    assert(embeddedCliAddBinding(cli, {
        "fsstat",
        "display settings file system status",
        false,
        this,
        static_file_system_status
    }));
    assert(embeddedCliAddBinding(cli, {
        "load",
        "load the current preset from the preset file name. usage: load <preset name>",
        true,
        this,
        static_load_preset
    }));
    assert(embeddedCliAddBinding(cli, {
        "ls",
        "list all settings files",
        true,
        this,
        static_list_files
    }));
    assert(embeddedCliAddBinding(cli, {
        "rm",
        "delete. usage: rm <fn>",
        true,
        this,
        static_delete_file
    }));
    assert(embeddedCliAddBinding(cli, {
        "save",
        "save the current configuration as a preset. usage: save <preset name>",
        true,
        this,
        static_save_current_preset
    }));
}

bool rppicomidi::Preset_manager::save_current_preset(std::string preset_name)
{
    // mount the lfs
    int error_code = pico_mount(false);
    if (error_code != 0) {
        printf("Error %s mounting the flash file system\r\n", pico_errmsg(error_code));
        return false;
    }

    // TODO serialize the preset and save it
    current_preset_name = preset_name;
    std::string ser;
    Midi2usbhub::instance().serialize(ser);
    lfs_file_t file;
    std::string filename = preset_name;
    error_code = lfs_file_open(&file, filename.c_str(),
                                LFS_O_WRONLY | LFS_O_TRUNC | LFS_O_CREAT); // open for write, truncate if it exists and create if it doesn't
    if (error_code != LFS_ERR_OK) {
        pico_unmount();
        printf("error %s opening file %s\r\n", pico_errmsg(error_code), filename.c_str());
        return false;
    }
    lfs_ssize_t size = lfs_file_write(&file, ser.c_str(), ser.length());
    error_code = lfs_file_close(&file);
    if (size < 0 || size != static_cast<lfs_ssize_t>(ser.length())) {
        printf("error %s writing preset data to file %s\r\n", pico_errmsg(size), filename.c_str());
        if (error_code != LFS_ERR_OK) {
            printf("error %s closing to file %s\r\n", pico_errmsg(error_code), filename.c_str());
        }
        pico_unmount();
        return false;
    }
    // preset data is written. Now need to update the current preset file
    bool result = update_current_preset(preset_name, false);
    pico_unmount();
    return result;
}

bool rppicomidi::Preset_manager::update_current_preset(std::string& preset_name, bool mount)
{
    if (mount) {
        int err = pico_mount(false);
        if (err != 0) {
            printf("Error %s mounting the flash file system\r\n", pico_errmsg(err));
            return false;
        }
    }
    lfs_file_t file;
    int error_code = lfs_file_open(&file, current_preset_filename,
                                LFS_O_WRONLY | LFS_O_TRUNC | LFS_O_CREAT); // open for write, truncate if it exists and create if it doesn't
    if (error_code != LFS_ERR_OK) {
        if (mount)
            pico_unmount();
        printf("error %s opening file %s\r\n", pico_errmsg(error_code), current_preset_filename);
        return false;
    }
    auto size = lfs_file_write(&file, preset_name.c_str(), preset_name.length());
    error_code = lfs_file_close(&file);
    bool result = true;
    if (size < 0 || size != static_cast<lfs_ssize_t>(preset_name.length())) {
        printf("error %s writing preset name to file %s\r\n", pico_errmsg(size), current_preset_filename);
        if (error_code != LFS_ERR_OK) {
            printf("error %s closing to file %s\r\n", pico_errmsg(error_code), current_preset_filename);
        }
        result = false;
    }
    else { 
        current_preset_name = preset_name;
    }
    if (mount)
        pico_unmount();
    return result;
}

int rppicomidi::Preset_manager::lfs_ls(const char *path)
{
    lfs_dir_t dir;
    int err = lfs_dir_open(&dir, path);
    if (err) {
        return err;
    }

    struct lfs_info info;
    while (true) {
        int res = lfs_dir_read(&dir, &info);
        if (res < 0) {
            return res;
        }

        if (res == 0) {
            break;
        }

        switch (info.type) {
            case LFS_TYPE_REG: printf("reg "); break;
            case LFS_TYPE_DIR: printf("dir "); break;
            default:           printf("?   "); break;
        }

        static const char *prefixes[] = {"", "K", "M", "G"};
        for (int i = sizeof(prefixes)/sizeof(prefixes[0])-1; i >= 0; i--) {
            if (info.size >= static_cast<size_t>((1 << 10*i)-1)) {
                printf("%*lu%sB ", 4-(i != 0), info.size >> 10*i, prefixes[i]);
                break;
            }
        }

        printf("%s\n", info.name);
    }

    err = lfs_dir_close(&dir);
    if (err) {
        return err;
    }

    return 0;
}

void rppicomidi::Preset_manager::static_file_system_format(EmbeddedCli*, char*, void*)
{
    printf("formatting settings file system then mounting it\r\n");
    int error_code = pico_mount(true);
    if (error_code != LFS_ERR_OK) {

    }
    else {
        error_code = pico_unmount();
        if (error_code != LFS_ERR_OK) {
            printf("unexpected error %s unmounting settings file system\r\n", pico_errmsg(error_code));
        }
        else {
            printf("File system successfully formated and unmounted\r\n");
        }
    }
}

void rppicomidi::Preset_manager::static_file_system_status(EmbeddedCli*, char*, void*)
{
    int error_code = pico_mount(false);
    if (error_code != LFS_ERR_OK) {
        printf("can't mount settings file system\r\n");
        return;
    }
    struct pico_fsstat_t stat;
    if (pico_fsstat(&stat) == LFS_ERR_OK) {
        printf("FS: blocks %d, block size %d, used %d\n", (int)stat.block_count, (int)stat.block_size,
                (int)stat.blocks_used);
    }
    else {
        printf("could not read file system status\r\n");
    }
    error_code = pico_unmount();
    if (error_code != LFS_ERR_OK) {
        printf("can't unmount settings file system\r\n");
        return;
    }
}


void rppicomidi::Preset_manager::static_list_files(EmbeddedCli* cli, char* args, void* context)
{
    (void)cli;
    (void)args;
    char path[256] = {'/','\0'};
    uint16_t argc = embeddedCliGetTokenCount(args);
    if (argc == 1) {
        strncpy(path, embeddedCliGetToken(args, 1), sizeof(path)-1);
        path[sizeof(path)-1] = '\0';
    }
    else {
        printf("usage: ls [path]\r\n");
    }
    auto me = reinterpret_cast<Preset_manager*>(context);
    int error_code = pico_mount(false);
    if (error_code != LFS_ERR_OK) {
        printf("can't mount settings file system\r\n");
        return;
    }
 
    error_code = me->lfs_ls(path);
    if (error_code != LFS_ERR_OK) {
        printf("error listing path \"/\"\r\n");
    }
    error_code = pico_unmount();
    if (error_code != LFS_ERR_OK) {
        printf("can't unmount settings file system\r\n");
        return;
    }
}

void rppicomidi::Preset_manager::static_delete_file(EmbeddedCli* cli, char* args, void*)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 1) {
        printf("usage: rm <filename>\r\n");
        return;
    }
    const char* fn=embeddedCliGetToken(args, 1);
    Preset_manager::instance().delete_file(fn);
}

int rppicomidi::Preset_manager::delete_file(const char* filename, bool mount)
{
    int error_code = LFS_ERR_OK;
    if (mount)
        error_code = pico_mount(false);
    if (error_code == LFS_ERR_OK) {
        error_code = pico_remove(filename);
        printf("error %s deleting file %s\r\n", pico_errmsg(error_code), filename);
        if (mount)
            error_code = pico_unmount();
        if (error_code != LFS_ERR_OK) {
            printf("Unexpected Error %s unmounting settings file system\r\n", pico_errmsg(error_code));
        }
    }
    return error_code;
}

void rppicomidi::Preset_manager::static_print_file(EmbeddedCli* cli, char* args, void* context)
{
    (void)cli;
    auto me = reinterpret_cast<Preset_manager*>(context);
    if (embeddedCliGetTokenCount(args) != 1) {
        printf("usage: cat <filename>\r\n");
        return;
    }
    const char* fn=embeddedCliGetToken(args, 1);
    char* raw_settings;
    int error_code = me->load_settings_string(fn, &raw_settings);
    if (error_code > 0) {
        printf("File: %s\r\n%s\r\n", fn, raw_settings);
        delete[] raw_settings;
    }
    else {
        switch(error_code) {
        case LFS_ERR_NOENT:
            printf("File %s does not exist\r\n", fn);
            break;
        case LFS_ERR_ISDIR:
            printf("%s is a directory\r\n",fn);
            break;
        case LFS_ERR_CORRUPT:
            printf("%s is corrupt\r\n",fn);
            break;
        case LFS_ERR_IO:
            printf("IO Error reading %s\r\n", fn);
            break;
        default:
            printf("Unexpected Error %s reading %s\r\n", pico_errmsg(error_code), fn);
            break;
        }
    }
}

int rppicomidi::Preset_manager::load_settings_string(const char* settings_filename, char** raw_settings_ptr, bool mount)
{
    int error_code = 0;
    if (mount)
        error_code = pico_mount(false);
    if (error_code != 0) {
        printf("unexpected error %s mounting flash\r\n", pico_errmsg(error_code));
        return error_code;
    }
    int file = pico_open(settings_filename, LFS_O_RDONLY);
    if (file < 0) {
        // file isn't there
        if (mount)
            pico_unmount();
        return file; // the error code
    }
    else {
        // file is open. Read the whole contents to a string
        auto flen = pico_size(file);
        if (flen < 0) {
            // Something went wrong
            if (mount)
                pico_unmount();
            return flen;
        }
        // create a string long enough
        *raw_settings_ptr = new char[flen+1];
        if (!raw_settings_ptr) {
            pico_close(file);
            if (mount)
                pico_unmount();
            return LFS_ERR_NOMEM; // new failed
        }
        auto nread = pico_read(file, *raw_settings_ptr, flen);
        pico_close(file);
        if (mount)
            pico_unmount();
        if (nread == (lfs_size_t)flen) {
            // Success!
            (*raw_settings_ptr)[flen]='\0'; // just in case, add null termination
            return nread;
        }
        if (nread > 0) {
            delete[] *raw_settings_ptr;
            *raw_settings_ptr = nullptr;
            nread = LFS_ERR_IO;
        }
        return nread;
    }
}

void rppicomidi::Preset_manager::static_save_current_preset(EmbeddedCli* cli, char* args, void*)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 1) {
        printf("usage: save <filename>\r\n");
        return;
    }
    if (instance().save_current_preset(std::string(embeddedCliGetToken(args, 1)))) {
        printf("Saved preset %s as current preset\r\n",embeddedCliGetToken(args, 1));
    }
}

bool rppicomidi::Preset_manager::load_preset(std::string preset_name)
{
    char* raw_preset_string;
    int error_code = load_settings_string(preset_name.c_str(), &raw_preset_string);
    bool result = false;
    if (error_code > 0) {
        std::string settings = std::string(raw_preset_string);
        delete[] raw_preset_string;
        if (Midi2usbhub::instance().deserialize(settings)) {
            if (update_current_preset(preset_name)) {
                printf("load preset %s successful\r\n", preset_name.c_str());
                result = true;
            }
        }
        else {
            printf("error deserilizating the preset\r\n");
        }        
    }
    else {
        printf("error %s loading settings %s\r\n", pico_errmsg(error_code), preset_name.c_str());
    }
    return result;
}

void rppicomidi::Preset_manager::static_load_preset(EmbeddedCli* cli, char* args, void*)
{
    (void)cli;
    if (embeddedCliGetTokenCount(args) != 1) {
        printf("usage: load <filename>\r\n");
        return;
    }
    if (instance().load_preset(std::string(embeddedCliGetToken(args, 1)))) {
        printf("Loaded preset %s as current preset\r\n",embeddedCliGetToken(args, 1));
    }
    else {
        printf("Failed to load preset %s\r\n", embeddedCliGetToken(args, 1));
    }
}
