#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define ACQ_IOC_MAGIC   'A'
#define ACQ_IOC_START   _IO(ACQ_IOC_MAGIC, 1)
#define ACQ_IOC_STOP    _IO(ACQ_IOC_MAGIC, 2)

#define NUM_CHANNELS 8
#define CHANNEL_SIZE 4096

static uint32_t calc_checksum(const void *data, size_t len)
{
    const uint32_t *ptr = (const uint32_t *)data;
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i < len / sizeof(uint32_t); i++)
        sum += ptr[i];

    return sum;
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-d /dev/pcie_acq] [-n loops]\n", prog);
}

int main(int argc, char **argv)
{
    const char *devnode = "/dev/pcie_acq";
    int loops = 1;
    int opt;
    int fd;
    size_t total = NUM_CHANNELS * CHANNEL_SIZE;
    uint8_t *buf;
    int i;

    while ((opt = getopt(argc, argv, "d:n:h")) != -1) {
        switch (opt) {
        case 'd':
            devnode = optarg;
            break;
        case 'n':
            loops = atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (loops <= 0)
        loops = 1;

    fd = open(devnode, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", devnode, strerror(errno));
        return 1;
    }

    if (ioctl(fd, ACQ_IOC_START) < 0) {
        fprintf(stderr, "ioctl(START) failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    buf = malloc(total);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        close(fd);
        return 1;
    }

    for (i = 0; i < loops; i++) {
        ssize_t rd = read(fd, buf, total);
        if (rd < 0) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            break;
        }

        printf("read %zd bytes (loop %d)\n", rd, i + 1);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            uint32_t sum = calc_checksum(buf + ch * CHANNEL_SIZE, CHANNEL_SIZE);
            printf("  ch%d checksum: 0x%08x\n", ch, sum);
        }
    }

    if (ioctl(fd, ACQ_IOC_STOP) < 0)
        fprintf(stderr, "ioctl(STOP) failed: %s\n", strerror(errno));

    free(buf);
    close(fd);
    return 0;
}
