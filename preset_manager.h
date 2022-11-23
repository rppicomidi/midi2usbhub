/**
 * @file preset_manager.h
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
#pragma once
#include <string>
#include "pico_hal.h"
#include "embedded_cli.h"
#include "ff.h"
namespace rppicomidi
{
class Preset_manager
{
public:
    // Singleton Pattern

    /**
     * @brief Get the Instance object
     *
     * @return the singleton instance
     */
    static Preset_manager &instance()
    {
        static Preset_manager _instance; // Guaranteed to be destroyed.
                                         // Instantiated on first use.
        return _instance;
    }
    Preset_manager(Preset_manager const &) = delete;
    void operator=(Preset_manager const &) = delete;
    ~Preset_manager()=default;
    void init_cli(EmbeddedCli* cli);
    /**
     * @brief Save the current settings to an LFS file called preset_name and
     * change the current preset to preset name
     * 
     * @param preset_name the new preset name
     * @return true if successful, false otherwise
     */
    bool save_current_preset(std::string preset_name);

    /**
     * @brief load the preset file named preset_name and update the
     * current settings as much as possible based on what devices
     * are currently plugged in, then change the current preset
     * to preset_name
     * 
     * @param preset_name the name of the preset to load
     */
    bool load_preset(std::string preset_name);

    /**
     * @brief Get the current preset name
     * 
     * @param preset_name is set to the current preset name
     */
    void get_current_preset_name(std::string& preset_name) { preset_name = current_preset_name; }
private:
    Preset_manager();
    /**
     * @brief set raw_settings_ptr to point to the data contained in the settings
     * file fn.
     *
     * The caller of this function must delete the memory allocated to raw_settings_ptr
     * using delete[];
     * @param fn the file name in lfs flash that contains the preset settings
     * @param raw_settings_ptr will point to a new block of memory initialized to
     * contain the preset settings
     * @param mount is true if the lfs filesystem needs to be mounted on entry
     * and unmounted on exit
     * @return int the number of bytes in the settings string, or a negative
     * LFS error code if there was an error.
     */
    int load_settings_string(const char* fn, char** raw_settings_ptr, bool mount=true);

    bool update_current_preset(std::string& preset_name, bool mount = true);
    /**
     * @brief See https://github.com/littlefs-project/littlefs/issues/2
     * 
     * @param lfs pointer to lfs object
     * @param path root path string
     * @return int 0 if no error, < 0 if there is an error
     */

    int lfs_ls(const char *path);
    /**
     * @brief remove a file from the lfs filesystem
     *
     * @param filename the full path to the file
     * @param mount is true to mount the filesystem on entry and unmount it on exit
     * @return int LFS_ERR_OK if successful, a negative error code if not
     */
    int delete_file(const char* filename, bool mount=true);

    void print_fat_date(WORD wdate);
    void print_fat_time(WORD wtime);
    FRESULT scan_files(const char* path);
    static void static_file_system_format(EmbeddedCli*, char*, void*);
    static void static_file_system_status(EmbeddedCli*, char*, void*);
    static void static_list_files(EmbeddedCli* cli, char* args, void* context);
    static void static_print_file(EmbeddedCli* cli, char* args, void* context);
    static void static_delete_file(EmbeddedCli* cli, char* args, void*);
    static void static_save_current_preset(EmbeddedCli* cli, char* args, void*);
    static void static_load_preset(EmbeddedCli* cli, char* args, void*);
    static void static_fatfs_ls(EmbeddedCli *cli, char *args, void *context);
    static void static_fatfs_cd(EmbeddedCli* cli, char* args, void* context);
    static void static_fatfs_chdrive(EmbeddedCli *cli, char *args, void *);
    static void static_fatfs_pwd(EmbeddedCli *, char *, void *);
    static void static_fatfs_backup(EmbeddedCli *cli, char *args, void *context);
    static void static_fatfs_restore(EmbeddedCli* cli, char* args, void*);

    std::string current_preset_name;
    static constexpr const char* preset_dir_name = "/rppicomidi-midi2usbhub";
    static constexpr const char* current_preset_filename = "current-preset";
};
}