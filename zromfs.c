#include "zromfs.h"

#include <stdio.h> // debug
#include <string.h>

typedef struct {
    zr_u32_t word0;
    zr_u32_t word1;
    zr_u32_t size;
    zr_u32_t checksum;
//    char name[0]; /* volume name */
} zr_super_block_t;

typedef struct {
    zr_u32_t next;
    zr_u32_t spec;
    zr_u32_t size;
    zr_u32_t checksum;
//    char name[0];
} zr_inode_t;

static struct {
    struct {
        zr_u32_t size, curr_pos, offset;
    } fds[ZR_MAX_OPENED_FILES + 3];     // keep 0, 1, 2
    struct {
        zr_fs_t* fs;
        int id, mounted;
    } volume[ZR_MAX_VOLUMNS];
} g;

#define _mk4(d,c,b,a) \
( (((zr_u32_t)(a)) << 24) | (((zr_u32_t)(b)) << 16) |\
 (((zr_u32_t)(c)) << 8) | (((zr_u32_t)(d)) ) )

#define __ftype(inode) (__le(inode.next) & 0x7)
#define __next(inode) (__le(inode.next) & (~0xf))

static zr_u32_t __le(zr_u32_t v)
{
#ifdef ZR_ENDIAN_LITTLE
    return (v >> 24) | (v << 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000);
#else
    return v;
#endif
}

static zr_u32_t __checksum(zr_fs_t* fs)
{
    zr_u32_t buf[128];
    zr_u32_t sum = 0;
    zr_u32_t chksum_size = fs->size >= 512 ? 512 : fs->size;
    fs->read_f(fs->start, buf, chksum_size);
    {
        int i;
        for(i = 0; i < chksum_size / 4; i++)
            sum += __le(buf[i]);
    }
    return sum;
}

ZR_RESULT zr_mount(zr_fs_t* fs, int volume_id)
{
    zr_super_block_t super;
    fs->read_f(fs->start, &super, sizeof(super));

    if(memcmp(&super, "-rom1fs-", 8) != 0)
        return ZR_NO_FILESYSTEM;
    fs->size = __le(super.size);
    if(__checksum(fs) != 0)
        return ZR_DISK_ERR;

    return ZR_OK;
}

/*
 int zr_init(zr_fs_t* fs)
 {
 zr_super_block_t super;
 fs->read_f(fs->start, &super, sizeof(super));

 if(memcmp(&super, "-rom1fs-", 8) != 0)
 return ZR_NO_FILESYSTEM;
 fs->size = __le(super.size);
 if(__checksum(fs) != 0)
 return ZR_DISK_ERR;

 return ZR_OK;
 }
 */

static zr_u32_t __skip_name(zr_fs_t* fs, zr_u32_t offset)
{
    char buf[16];
    do {
        fs->read_f(offset, buf, 16);
        offset += 16;
    } while(buf[15] != '\0');
    return offset;
}

static zr_s32_t __seek_fname(zr_fs_t* fs, zr_u32_t offset, const char* path)
{
    while(path[0] == '/')
        path++;
    if(strlen(path) == 0)
        return offset;

    while(1) {
        zr_inode_t inode;
        char fname[16];

        fs->read_f(offset, &inode, sizeof(inode));
        fs->read_f(offset + 16, fname, sizeof(fname));    // fname, only 16 chars available
        if(strcmp(path, fname) == 0) {    // path found
            if(__ftype(inode) == ZR_FTYPE_REGULAR
                || __ftype(inode) == ZR_FTYPE_DIR)
                return offset;
            else if(__ftype(inode) == ZR_FTYPE_HARDLINK) {
                return __le(inode.spec);
            }
            else
                return ZR_FILETYPE_NOT_SUPPORTED;
        }
        if(strstr(path, fname) == path && path[strlen(fname)] == '/') {

            if(__ftype(inode) == ZR_FTYPE_HARDLINK) {    //hard link to . or ..
                return ZR_FILETYPE_NOT_SUPPORTED;
            }
            else if(__ftype(inode) == ZR_FTYPE_DIR) {    //directory
                zr_inode_t t;

                fs->read_f(offset, &t, sizeof(t));
                fs->read_f(offset + 16, fname, sizeof(fname));    // fname, only 16 chars available
                offset = __seek_fname(fs, __le(t.spec) & ~0xf,
                    path + strlen(fname) + 1);
                if(offset > 0)
                    return offset;
            }
            else if(__ftype(inode) == ZR_FTYPE_SYMBOL_LINK) {    //symbolic link to a directory
                return ZR_FILETYPE_NOT_SUPPORTED;
            }
        }
        offset = __le(inode.next);
        offset &= ~0xf;
        if(offset == 0)
            return ZR_FILE_NOT_FOUND;    // not found
    }
}

int zr_opendir(zr_fs_t* fs, zr_dir_t* dir, const char* path)
{
    zr_s32_t offset;
    zr_inode_t inode;
    dir->offset = fs->start + 16;
    dir->offset = __skip_name(fs, dir->offset);
    offset = __seek_fname(fs, dir->offset, path);
    if(offset == -1)
        return ZR_DIR_NOT_FOUND;
    fs->read_f(offset, &inode, sizeof(inode));

    if((__le(inode.next) & 0x7) != ZR_FTYPE_DIR)
        return ZR_NOT_A_DIR;

    dir->offset = __le(inode.spec);    //offset;
    return ZR_OK;
}

int zr_stat(zr_fs_t* fs, const char* path, zr_finfo_t* finfo)
{
    zr_inode_t inode;
    zr_s32_t offset = fs->start + 16;
    offset = __skip_name(fs, offset);
    offset = __seek_fname(fs, offset, path);
    if(offset < 0)
        return ZR_FILE_NOT_FOUND;
    fs->read_f(offset, &inode, sizeof(inode));
    fs->read_f(offset + 16, finfo->fname, sizeof(finfo->fname));

    finfo->fsize = __le(inode.size);
    finfo->spec = __le(inode.spec);
    finfo->offset = offset;
    finfo->next = __le(inode.next) & (~0xf);
    finfo->ftype = __le(inode.next) & 0x7;

    return ZR_OK;
}

int zr_readdir(zr_fs_t* fs, zr_dir_t* dir, zr_finfo_t* finfo)
{
    zr_inode_t inode;

    if(dir->offset == fs->start)
        return ZR_NO_FILE;
    fs->read_f(dir->offset, &inode, sizeof(inode));
    fs->read_f(dir->offset + 16, finfo->fname, sizeof(finfo->fname));

    finfo->fsize = __le(inode.size);
    finfo->spec = __le(inode.spec);
    finfo->offset = dir->offset;
    finfo->next = __le(inode.next) & (~0xf);
    finfo->ftype = __le(inode.next) & 0x7;
    dir->offset = __le(inode.next) & (~0xf);

    return ZR_OK;
}

int __find_free_fd(void)
{
    int i;
    for(i = 3; i < ZR_MAX_OPENED_FILES + 3; i++) {
        if(g.fds[i].offset == 0)
            return i;
    }
    return -1;
}

int zr_open(zr_fs_t* fs, const char* path)
{
    int fd;
    zr_finfo_t finfo;
    int ret = zr_stat(fs, path, &finfo);
    if(ret != ZR_OK)
        return ret;

    fd = __find_free_fd();
    if(fd < 0)
        return fd;
    g.fds[fd].size = finfo.fsize;
    g.fds[fd].offset = finfo.offset + 16;
    g.fds[fd].offset = __skip_name(fs, g.fds[fd].offset);
    g.fds[fd].curr_pos = 0;
    return fd;    //skips system FDs
}

int zr_close(zr_fs_t* fs, int fd)
{
    if(g.fds[fd].offset == 0)
        return ZR_FILE_NOT_OPENED;
    g.fds[fd].curr_pos = 0;
    g.fds[fd].offset = 0;
    return ZR_OK;
}

int zr_read(zr_fs_t* fs, int fd, void* buf, zr_u32_t nbytes)
{
    int max_to_read;
    if(g.fds[fd].offset == 0)
        return ZR_FILE_NOT_OPENED;

    max_to_read = g.fds[fd].size - g.fds[fd].curr_pos;
    if(max_to_read < 0)
        return 0;
    if(max_to_read > nbytes)
        max_to_read = nbytes;
    fs->read_f(g.fds[fd].offset + g.fds[fd].curr_pos, buf, max_to_read);
    g.fds[fd].curr_pos += max_to_read;

    return max_to_read;
}

zr_u32_t zr_tell(zr_fs_t* fs, int fd)
{
    return g.fds[fd].curr_pos;
}

int zr_lseek(zr_fs_t* fs, int fd, zr_u32_t offset)
{
    if(g.fds[fd].size >= offset)
        g.fds[fd].offset = g.fds[fd].size - 1;
    else
        g.fds[fd].curr_pos = offset;
    return ZR_OK;
}
