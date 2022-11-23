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
#include "diskio.h"
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

    lfs_file_t file;
    error_code = lfs_file_open(&file, current_preset_filename, LFS_O_RDONLY);
    if (error_code == LFS_ERR_OK) {
        char curr_preset[LFS_NAME_MAX+1];
        lfs_ssize_t nread = lfs_file_read(&file, curr_preset, LFS_NAME_MAX);
        if (nread > 0) {
            curr_preset[nread] = '\0'; // just in case
            current_preset_name = std::string(curr_preset);
        }
    }
    else {
        printf("Error %s opening file %s for reading\r\n", pico_errmsg(error_code), current_preset_filename);
    }
    if (pico_unmount() != 0) {
        printf("Failed to unmount the flash file system\r\n");
    }
    msc_fat_init();
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
    assert(embeddedCliAddBinding(cli, {
        "f-cd",
        "change current directory on the USB flash drive. usage: f-cd <path>",
        true,
        this,
        static_fatfs_cd
    }));
    assert(embeddedCliAddBinding(cli, {
        "f-chdrive",
        "change current drive number for the USB flash drive. usage: f-chdrive <drive number 0-3>",
        true,
        this,
        static_fatfs_chdrive
    }));
    assert(embeddedCliAddBinding(cli, {
        "f-ls",
        "list contents of the current directory on the current USB flash drive. usage: f-ls [path]",
        false,
        this,
        static_fatfs_ls
    }));
    assert(embeddedCliAddBinding(cli, {
        "f-pwd",
        "print the current directory path of the current USB flash drive. usage: f-pwd",
        false,
        this,
        static_fatfs_pwd
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
    bool result = true;

    // only need to do stuff if the preset_name is not the current_preset_name
    if (preset_name != current_preset_name) {
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
    }
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


void rppicomidi::Preset_manager::print_fat_date(WORD wdate)
{
    uint16_t year = 1980 + ((wdate >> 9) & 0x7f);
    uint16_t month = (wdate >> 5) & 0xf;
    uint16_t day = wdate & 0x1f;
    printf("%02u/%02u/%04u\t", month, day, year);
}

void rppicomidi::Preset_manager::print_fat_time(WORD wtime)
{
    uint8_t hour = ((wtime >> 11) & 0x1f);
    uint8_t min = ((wtime >> 5) & 0x3F);
    uint8_t sec = ((wtime &0x1f)*2);
    printf("%02u:%02u:%02u\t", hour, min, sec);
}

FRESULT rppicomidi::Preset_manager::scan_files(const char* path)
{
    FRESULT res;
    DIR dir;
    static FILINFO fno;

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            printf("%lu\t",fno.fsize);
            print_fat_date(fno.fdate);
            print_fat_time(fno.ftime);
            printf("%s%c\r\n",fno.fname, (fno.fattrib & AM_DIR) ? '/' : ' ');
        }
        f_closedir(&dir);
    }

    return res;
}

void rppicomidi::Preset_manager::static_fatfs_ls(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
    const char* path = ".";
    uint16_t argc = embeddedCliGetTokenCount(args);
    if (argc == 1) {
        path = embeddedCliGetToken(args, 1);
    }
    else if (argc > 1) {
        printf("usage: fatls [path]\r\n");
    }
    FRESULT res = instance().scan_files(path);
    if (res != FR_OK) {
        printf("Error %u listing files on drive\r\n", res);
    }
}

#if 0
void rppicomidi::Preset_manager::static_fatfs_backup(EmbeddedCli *cli, char *args, void *)
{
    FRESULT res = instance().backup_all_presets();
    if (res != FR_OK) {
        printf("Error %u backing up files on drive\r\n", res);
    }
}

void rppicomidi::Preset_manager::static_fatfs_restore(EmbeddedCli* cli, char* args, void*)
{
    (void)cli;
    uint16_t argc = embeddedCliGetTokenCount(args);
    FRESULT res;
    char path[256];
    if (argc == 1) {
        strncpy(path, embeddedCliGetToken(args, 1), sizeof(path)-1);
        res = Preset_manager::instance().restore_presets(path);
    }
    else {
        printf("usage: fatcd <new path>\r\n");
        return;
    }
    if (res != FR_OK) {
        printf("error %u restoring presets from %s\r\n", res, path);
    }
}
#endif

void rppicomidi::Preset_manager::static_fatfs_cd(EmbeddedCli* cli, char* args, void* context)
{
    (void)cli;
    (void)context;
    uint16_t argc = embeddedCliGetTokenCount(args);
    FRESULT res;
    char temp_cwd[256] = {'/', '\0'};
    if (argc == 0) {
        res = f_chdir(temp_cwd);
    }
    else if (argc == 1) {
        strncpy(temp_cwd, embeddedCliGetToken(args, 1), sizeof(temp_cwd)-1);
        temp_cwd[sizeof(temp_cwd)-1] = '\0';
        res = f_chdir(temp_cwd);
    }
    else {
        printf("usage: fatcd <new path>\r\n");
        return;
    }
    if (res != FR_OK) {
        printf("error %u setting cwd to %s\r\n", res, temp_cwd);
    }
    res = f_getcwd(temp_cwd, sizeof(temp_cwd));
    if (res == FR_OK)
        printf("cwd=\"%s\"\r\n", temp_cwd);
    else
        printf("error %u getting cwd\r\n", res);
}

void rppicomidi::Preset_manager::static_fatfs_chdrive(EmbeddedCli *cli, char *args, void *)
{
    (void)cli;
    uint16_t argc = embeddedCliGetTokenCount(args);
    FRESULT res;
    char dstr[] = "0:";
    if (argc != 1) {
        printf("usage chdrive drive_number(0-3)");
    }
    else {
        int drive = atoi(embeddedCliGetToken(args, 1));
        if (drive >=0 && drive <= 3) {
            dstr[0] += drive;
            res = f_chdrive(dstr);
            if (res != FR_OK) {
                printf("error %u setting drive to %d\r\n", res, drive);
            }
        }
        else {
            printf("usage chdrive drive_number(0-3)");
        }
    }
}

void rppicomidi::Preset_manager::static_fatfs_pwd(EmbeddedCli *, char *, void *)
{
    char cwd[256];
    FRESULT res = f_getcwd(cwd, sizeof(cwd));
    if (res != FR_OK) {
        printf("error %u reading the current working directory\r\n", res);
    }
    else {
        printf("cwd=%s\r\n", cwd);
    }
}
//-------------MSC/FATFS IMPLEMENTATION -------------//
static scsi_inquiry_resp_t inquiry_resp;
static FATFS fatfs[FF_VOLUMES];
static_assert(FF_VOLUMES == CFG_TUH_DEVICE_MAX);

bool inquiry_complete_cb(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
    if (csw->status != 0) {
        printf("Inquiry failed\r\n");
        return false;
    }

    // Print out Vendor ID, Product ID and Rev
    printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

    // Get capacity of device
    uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
    uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

    printf("Disk Size: %lu MB\r\n", block_count / ((1024*1024)/block_size));
    printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    uint8_t pdrv = mmc_map_next_pdrv(dev_addr);

    assert(pdrv < FF_VOLUMES);
    msc_fat_plug_in(pdrv);
    uint8_t const lun = 0;
    tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb);
    char path[3] = "0:";
    path[0] += pdrv;
    if ( f_mount(&fatfs[pdrv],path, 0) != FR_OK ) {
        printf("mount failed\r\n");
        return;
        if (f_chdrive(path) != FR_OK) {
            printf("f_chdrive(%s) failed\r\n", path);
        }
    }
    printf("Mass Storage drive %u is mounted\r\n", pdrv);
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    uint8_t pdrv = mmc_unmap_pdrv(dev_addr);
    char path[3] = "0:";
    path[0] += pdrv;

    f_mount(NULL, path, 0); // unmount disk
    msc_fat_unplug(pdrv);
    printf("Mass Storage drive %u is unmounted\r\n", pdrv);
}

void main_loop_task()
{
    rppicomidi::Midi2usbhub::instance().task();
}