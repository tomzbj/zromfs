#ifndef __ZROMFS_H
#define __ZROMFS_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// configurations
#define ZR_ENDIAN_LITTLE
//#define ZR_ENDIAN_BIG
#define ZR_MAX_OPENED_FILES 8

typedef unsigned char zr_u8_t;
typedef unsigned short zr_u16_t;
typedef unsigned long zr_u32_t;

enum {
    ZR_OK = 0, ZR_NO_FILESYSTEM = -1, ZR_DISK_ERR = -2, ZR_FILE_NOT_FOUND = -3,
    ZR_DIR_NOT_FOUND = -4, ZR_NO_FILE = -5, ZR_NOT_A_DIR = -6,
    ZR_FILETYPE_NOT_SUPPORTED = -7, ZR_FILE_NOT_OPENED = -8
};

enum {
    ZR_FTYPE_HARDLINK, ZR_FTYPE_DIR, ZR_FTYPE_REGULAR, ZR_FTYPE_SYMBOL_LINK,
    ZR_FTYPE_BLOCK_DEV, ZR_FTYPE_CHAR_DEV, ZR_FTYPE_SOCKET, ZR_FTYPE_FIFO
};

typedef struct {
    zr_u32_t start;
    void (*read_f)(zr_u32_t offset, void* buf, zr_u32_t size);
    zr_u32_t size;
} zr_fs_t;

typedef struct {
    zr_u32_t offset;
} zr_dir_t;

typedef struct {
    char fname[16];
    zr_u32_t fsize;
    zr_u32_t spec;
    zr_u32_t offset;
    zr_u32_t next;
    zr_u32_t ftype;
} zr_finfo_t;

int zr_init(zr_fs_t* fs);
int zr_open(zr_fs_t* fs, const char* path);
int zr_close(zr_fs_t* fs, int fd);
int zr_read(zr_fs_t* fs, int fd, void* buff, zr_u32_t nbytes);
int zr_lseek(zr_fs_t* fs, int fd, zr_u32_t offset);
zr_u32_t zr_tell(zr_fs_t* fs, int fd);
int zr_stat(zr_fs_t* fs, const char* path, zr_finfo_t* finfo);

int zr_opendir(zr_fs_t* fs, zr_dir_t* dir, const char* path);
int zr_readdir(zr_fs_t* fs, zr_dir_t* dir, zr_finfo_t* finfo);

#ifdef __cplusplus
}
#endif

#endif 
