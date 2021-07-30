# zromfs

A simple &amp; portable romfs driver.

打算自己实现一个romfs的库, 主要用于在spiflash里放点字库、图片、声音之类方便些.  以前做过一个管理spiflash文件的库, 链接： https://github.com/tomzbj/flasher.py

后来觉得要不光能在pc上管理, 最好在mcu上用着也方便一点, 还需要扩充, 不过再扩充就成fs了. 既然这样, 不如还是找个现成的fs用起来, 只读即可, 简单点最好. fatfs和spiffs功能过于强大, 没必要. romfs则刚好合适, 只读, 文件内容连续储存这一点对mcu很友好, 只是没找到合适的库. 

github上找了一个, https://github.com/litwr2/a-romfs-driver, 试了一下能用. 它的缺点一是只能从地址空间读取, 直接把需要的资源编译到mcu内部flash用没问题, 放spiflash就不行了, 得把底层的读功能改成回调函数, 由用户提供. 二是不支持子目录. 所以还是得自己重新实现一遍.

目前实现了以下api:

    int zr_init(zr_fs_t* fs);
    int zr_open(zr_fs_t* fs, const char* path);
    int zr_close(zr_fs_t* fs, int fd);
    int zr_read(zr_fs_t* fs, int fd, void* buff, zr_u32_t nbytes);
    int zr_lseek(zr_fs_t* fs, int fd, zr_u32_t offset);
    zr_u32_t zr_tell(zr_fs_t* fs, int fd);
    int zr_stat(zr_fs_t* fs, const char* path, zr_finfo_t* finfo);

    int zr_opendir(zr_fs_t* fs, zr_dir_t* dir, const char* path);
    int zr_readdir(zr_fs_t* fs, zr_dir_t* dir, zr_finfo_t* finfo);

以及一个简单的demo, 在mingw环境下编译运行, 命令行界面,  支持ls, ll, pwd, cd, cat, stat, hexview, crc32, export, help命令.

晚些时候我再移植到stm32+spiflash上试试.

