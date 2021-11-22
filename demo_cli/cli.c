#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../zromfs.h"
#include "crc32.h"
#include <sys/stat.h>
#include <errno.h>

#define MSG_LEN 256
#define GEN_BUF_SIZE 32
#define ok() printf("ok\n");

static struct {
    char pwd[64];
    char gen_buf[GEN_BUF_SIZE];
} g = {.pwd = "/", };

extern zr_fs_t fs;
const char* ftype[] = {"Hardlink", "Dir", "Regular", "Sym. Link", "Blk. Dev",
    "Char. Dev", "Socket", "Fifo"};

static void cmd_ls(char* const tokens[])
{
    zr_dir_t dir;
    zr_finfo_t finfo;
    int n = 0, ll = 0, tot_size = 0;
    if(strcasecmp(tokens[0], "ll") == 0)
        ll = 1;

    if(zr_opendir(&dir, g.pwd) != ZR_OK)
        return;
    if(ll)
        printf("%-8s %-8s %-8s %-8s %-10s %-16s\n", "Offset", "Spec", "Next",
            "Size", "Type", "Filename");
    while(zr_readdir(&dir, &finfo) == ZR_OK) {
        n++;
        tot_size += finfo.fsize;
        if(ll)
            printf("%-8lX %-8lX %-8lX %-8lu %-10s %-16s\n", finfo.offset,
                finfo.spec, finfo.next, finfo.fsize, ftype[finfo.ftype],
                finfo.fname);
        else
            printf("%s\t", finfo.fname);
    }
    printf("\n");
    if(ll)
        printf("\t%d files, %d bytes in total.\n", n, tot_size);
    printf("\n");
}

static void cmd_pwd(char* const tokens[])
{
    printf("%s\n\n", g.pwd);
}

static void cmd_stat(char* const tokens[])
{
    zr_finfo_t finfo;
    char path[256];
    strcpy(path, g.pwd);
    strcat(path, "/");
    strcat(path, tokens[1]);

    if(zr_stat(path, &finfo) != ZR_OK) {
        printf("    File %s not found.\n\n", tokens[1]);
        return;
    }
    printf("%-8s %-8s %-8s %-8s %-10s %-16s\n", "Offset", "Spec", "Next",
        "Size", "Type", "Filename");
    printf("%-8lX %-8lX %-8lX %-8lu %-10s %-16s\n\n", finfo.offset, finfo.spec,
        finfo.next, finfo.fsize, ftype[finfo.ftype], finfo.fname);
}

const char* str_help =
    "ls:    list directory contents.\n\
ll:    list directory contents in detail.\n\
pwd:   show working directory.\n\
cd:    change working directory.\n\
cat:   show file content.\n\
stat:  show file infomation.\n\
help:  show help.\n";

static void cmd_help(char* const tokens[])
{
    printf("%s\n", str_help);
}

static int __open_file(const char* fname, int* size)
{
    zr_finfo_t finfo;
    char path[256];
    strcpy(path, g.pwd);
    strcat(path, "/");
    strcat(path, fname);

    if(zr_stat(path, &finfo) != ZR_OK) {
        printf("    File %s not found.\n\n", fname);
        return -1;
    }

    int fd = zr_open(path);
    if(fd < 0) {
        printf("    Failed to open file %s.\n\n", fname);
        return -1;
    }
    *size = finfo.fsize;
    return fd;
}

static void cmd_cat(char* const tokens[])
{
    int size;
    int fd = __open_file(tokens[1], &size);
    if(fd < 0)
        return;

    while(size > 0) {
        int n = GEN_BUF_SIZE;
        memset(g.gen_buf, 0, GEN_BUF_SIZE);
        n = zr_read(fd, g.gen_buf, n);
        if(n <= 0)
            break;
        fwrite(g.gen_buf, n, 1, stdout);
        fflush(stdout);
        size -= n;
    }
    printf("\n\n");
    zr_close(fd);
}

static void cmd_hexview(char* const tokens[])
{
    int size;
    int fd = __open_file(tokens[1], &size);
    if(fd < 0)
        return;
    for(int i = 0; i < 16; i++)
        printf("%02X ", i);
    printf("\n");

    while(size > 0) {
        int n = 16;
        memset(g.gen_buf, 0, 16);
        printf("%08lX ", zr_tell(fd));
        n = zr_read(fd, g.gen_buf, n);
//        printf("n=%d\n", n);
        if(n <= 0)
            break;
        for(int i = 0; i < n; i++)
            printf("%02X ", g.gen_buf[i]);
        printf("\n");
//        fflush(stdout);
        size -= n;
    }
    printf("\n\n");
    zr_close(fd);
}

static void cmd_crc32(char* const tokens[])
{
    int size;
    int fd = __open_file(tokens[1], &size);
    if(fd < 0)
        return;
    unsigned long crc = 0xffffffff;

    while(size > 0) {
        int n = GEN_BUF_SIZE;
        memset(g.gen_buf, 0, GEN_BUF_SIZE);
        n = zr_read(fd, g.gen_buf, n);
        if(n <= 0)
            break;
        crc = crc32(crc, g.gen_buf, n);
        size -= n;
    }
    printf("%08lX\n\n", ~crc);
    zr_close(fd);
}

static void cmd_export(char* const tokens[])
{
    int size;
    int fd = __open_file(tokens[1], &size);
    if(fd < 0)
        return;

    struct stat st;
    errno = 0;
//    int ret =
    stat(tokens[1], &st);

    if(errno != ENOENT) {    // file exists
        printf("File %s already exists.\n\n", tokens[1]);
        zr_close(fd);
        return;
    }
    FILE* fp = fopen(tokens[1], "wb");

    while(size > 0) {
        int n = GEN_BUF_SIZE;
        memset(g.gen_buf, 0, GEN_BUF_SIZE);
        n = zr_read(fd, g.gen_buf, n);
        if(n <= 0)
            break;
        fwrite(g.gen_buf, n, 1, fp);
        size -= n;
    }
    fclose(fp);

    printf("File %s exported.\n\n", tokens[1]);
    zr_close(fd);
}

static void cmd_cd(char* const tokens[])
{
    zr_dir_t dir;
    int ret = zr_opendir(&dir, tokens[1]);
    if(ret == ZR_OK) {
        if(strcmp(tokens[1], "/") == 0)
            strcpy(g.pwd, "/");
        else if(strcmp(tokens[1], ".") == 0)
            return;
        else if(strcmp(tokens[1], "..") == 0) {
            if(strcmp(g.pwd, "/") == 0)
                return;
            else {
                do {
                    g.pwd[strlen(g.pwd) - 1] = '\0';
                } while(g.pwd[strlen(g.pwd) - 1] != '/');
            }
        }
        else {
            if(g.pwd[strlen(g.pwd) - 1] != '/')
                strcat(g.pwd, "/");
            strcat(g.pwd, tokens[1]);
        }
    }
    else if(ret == ZR_DIR_NOT_FOUND)
        printf("%s: No such file or directory.\n\n", tokens[1]);
    else if(ret == ZR_NOT_A_DIR)
        printf("%s: Not a directory.\n\n", tokens[1]);
}

const struct {
    void (*func)(char* const []);
    const char* name;
    int n_args;
} cmds[] = {
//
    {cmd_ls, "ls", 1},    //
    {cmd_ls, "ll", 1},    //
    {cmd_pwd, "pwd", 1},    //
    {cmd_cd, "cd", 2},    //
    {cmd_cat, "cat", 2},    //
    {cmd_stat, "stat", 2},    //
    {cmd_hexview, "hexview", 2},    //
    {cmd_crc32, "crc32", 2},    //
    {cmd_export, "export", 2},    //
    {cmd_help, "help", 1},    //
    {cmd_help, "?", 1},    //
    };

static void Parse(char* const tokens[], int count)
{
    if(strcasecmp(tokens[0], "TEST") == 0) {
        ok();
    }

    for(int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if(strcasecmp(tokens[0], cmds[i].name) == 0) {
            if(count == cmds[i].n_args)
                cmds[i].func(tokens);
            else {
                printf("Usage: %s ", cmds[i].name);
                for(int j = 1; j < cmds[i].n_args; j++) {
                    printf("arg%d ", j);
                }
                printf("\n");
            }
        }
    }
}

void CLI_Parse(const void* pmsg, int size, int source)
{
    char* tokens[8], * token;
    char seps[] = "? ,#\r\n", string[MSG_LEN];
    int len, count = 0;
    char* msg = (char*)pmsg;

    memset(string, 0, MSG_LEN);
    strncpy(string, msg, size);
    len = strlen(string);
    while(string[len - 1] == '\n' || string[len - 1] == '\r') {
        string[len - 1] = '\0';
        len--;
    }

    token = strtok(string, seps);
    while(token != NULL) {

        tokens[count] = token;
        count++;
        token = strtok(NULL, seps);    // Get next token:
    }
    if(count == 0)
        return;

    Parse(tokens, count);
}

void CLI_Prompt(void)
{
    printf("%s # ", g.pwd);
    fflush(stdout);
}
