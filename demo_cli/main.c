#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "../zromfs.h"
#include "cli.h"

static struct {
    int fd;
} g;

void read_func(zr_u32_t offset, void* buf, zr_u32_t size)
{
    lseek(g.fd, offset, SEEK_SET);
    read(g.fd, buf, size);
}

int main(int argc, char* argv[])
{
    static zr_fs_t fs;
    if(argc != 2) {
        puts("Usage: zr_cli img_file");
        exit(1);
    }
    g.fd = open(argv[1], O_RDONLY | _O_BINARY);

    fs.start = 0;
    fs.read_f = read_func;

    int ret = zr_mount(&fs);
    printf("%d\n", ret);
    if(ret < 0)
        exit(1);
    zr_select_volume(0);

    while(1) {
        char buf[256];
        CLI_Prompt();
        fgets(buf, 256, stdin);
        if(strcasecmp(buf, "exit\n") == 0)
            break;
        CLI_Parse(buf, 256, 0);
    }
    close(g.fd);
    return 0;
}
