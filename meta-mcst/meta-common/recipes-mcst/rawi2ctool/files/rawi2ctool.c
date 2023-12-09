#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>

enum mode_t
{
    MODE_IOCTL,
    MODE_READWRITE,
    MODE_SMBUS,
    MODE_SMBUS_QUICK
};

enum op_t
{
    OP_SEND,
    OP_RECV,
};

#define MAX_DATA 2
int parse_cmdline(int argc, char *argv[], int *bus, int *slave, enum mode_t *mode, enum op_t *op, char *data, size_t *size)
{
    int verbose = 0;
    int quit = 0;
    int bus_set = 0;
    int slave_set = 0;
    int data_set = 0;

    *mode = MODE_IOCTL;
    *op = OP_RECV;
    *size = 1;

    if (argc < 2) goto usage;

    for (int i = 1; i < argc; ++i)
    {
             if (strcmp(argv[i], "-v") == 0) ++verbose;
        else if (strcmp(argv[i], "-q") == 0) ++quit;
        else if (strcmp(argv[i], "-w") == 0) *size = 2;
        else if (strcmp(argv[i], "-i") == 0) *mode = MODE_IOCTL;
        else if (strcmp(argv[i], "-r") == 0) *mode = MODE_READWRITE;
        else if (strcmp(argv[i], "-s") == 0) *mode = MODE_SMBUS;
        else if (strcmp(argv[i], "-R") == 0) { *mode = MODE_SMBUS_QUICK; *op = OP_RECV; }
        else if (strcmp(argv[i], "-W") == 0) { *mode = MODE_SMBUS_QUICK; *op = OP_SEND; }
        else
        {
            char *nptr;
            long conv = strtol(argv[i], &nptr, 0);
            if (*nptr)
            {
                fprintf(stderr, "Incorrect argument value: '%s'\n", argv[i]);
                goto usage;
            }

            if (data_set || (slave_set && *mode == MODE_SMBUS_QUICK))
            {
                fprintf(stderr, "Unexpected argument value: '%s'\n", argv[i]);
                goto usage;
            }
            else if (slave_set)
            {
                *op = OP_SEND;
                data[0] = conv & 0xFF;
                data[1] = (conv & 0xFF00) >> 8;
                ++data_set;
            }
            else if (bus_set)
            {
                *slave = conv;
                ++slave_set;
            }
            else
            {
                *bus = conv;
                ++bus_set;
            }
        }
    }

    if (!bus_set || !slave_set)
    {
        fprintf(stderr, "You should specify bus and slave arguments\n");
        goto usage;
    }

    if (verbose)
    {
        printf
        (
            "Performing %s %s on i2c-%d, address 0x%02x",
            (
                (*mode == MODE_SMBUS_QUICK) ? "SMBus quick" :
                (*mode == MODE_SMBUS) ? "SMBus" :
                (*mode == MODE_READWRITE) ? "generic" : "IOCTL"
            ),
            ((*op == OP_RECV) ? "RECV" : "SEND"),
            *bus, *slave
        );
        if (*op == OP_SEND && *mode != MODE_SMBUS_QUICK)
        {
            printf(", using data 0x%02x", data[0] & 0xFF);
            if (*size > 1) printf(" 0x%02x", data[1] & 0xFF);
        }
        printf("\n");
    }
    return quit ? 255 : 0;

usage:
    printf("\
    Usage:\n\
        %s [-v] [-q] [-i|-r|-s] [-w] <bus_num> <slave_num> [<data>]\n\
            -i : use IOCTL I2C_RDWR call (default).\n\
            -r : use generic read()/write() calls.\n\
            -s : use SMBus functions read_byte()/write_byte().\n\
            -w : read/write word instead of byte (only for I2C_RDWR or generic operations).\n\
                 <data> should be LE word in that case (e.g. 0x1234 sends bytes 0x34, then 0x12).\n\
            -v : enable verbose mode (show what actually is to be done).\n\
            -q : quit without actually doing anything, just parse arguments.\n\
        %s [-v] [-q] {-R|-W} <bus_num> <slave_num>\n\
            -R : only detect slave's existence using SMBus, sending address with R/W = 1.\n\
            -W : only detect slave's existence using SMBus, sending address with R/W = 0.\n\
        Options can't be combined (e.g. -ivw). Latter option (e.g. -i -s) takes precedence.\n\
", argv[0], argv[0]);
    return 1;
}

int main(int argc, char *argv[])
{
    int bus, slave, rv;
    char data[MAX_DATA];
    enum mode_t mode;
    enum op_t op;
    size_t size;

    if((rv = parse_cmdline(argc, argv, &bus, &slave, &mode, &op, data, &size)) != 0) return rv;

    char dev[50];
    snprintf(dev, 50, "/dev/i2c-%d", bus);

    int file = open(dev, O_RDWR);
    if (file == -1)
    {
        perror(dev);
        exit(1);
    }

    if (mode == MODE_IOCTL)
    {
        struct i2c_msg message =
        {
            .addr = slave,
            .flags = ((op == OP_RECV) ? 1 : 0),
            .len = size,
            .buf = data
        };
        struct i2c_rdwr_ioctl_data packet =
        {
            .msgs = &message,
            .nmsgs = 1
        };

        if (ioctl(file, I2C_RDWR, &data) < 0)
        {
            perror("Failed ioctl I2C_RDWR");
            exit(1);
        }
    }
    else
    {
        if (ioctl(file, I2C_SLAVE, slave) < 0)
        {
            perror("Failed ioctl I2C_SLAVE");
            exit(1);
        }

        switch(mode)
        {
            case MODE_READWRITE:
                switch(op)
                {
                    case OP_SEND:
                        if (write(file, data, size) != size)
                        {
                            perror("Failed to send to i2c bus");
                            exit(1);
                        }
                    break;
                    case OP_RECV:
                        if (read(file, data, size) != size)
                        {
                            perror("Failed to recv from i2c bus");
                            exit(1);
                        }
                    break;
                }
            break;
            case MODE_SMBUS_QUICK:
                switch(op)
                {
                    case OP_SEND:
                        if (i2c_smbus_write_quick(file, I2C_SMBUS_WRITE) != 0)
                        {
                            perror("Failed to quick send to SMBus");
                            exit(1);
                        }
                    break;
                    case OP_RECV:
                        if ((rv = i2c_smbus_write_quick(file, I2C_SMBUS_READ)) != 0)
                        {
                            perror("Failed to quick recv from SMBus");
                            exit(1);
                        }
                    break;
                }
            break;
            case MODE_SMBUS:
                if (size != 1)
                {
                    fprintf(stderr, "SMBus SEND/RECV can be only 1 byte long\n");
                    return 2;
                }
                switch(op)
                {
                    case OP_SEND:
                        if (i2c_smbus_write_byte(file, data[0]) != 0)
                        {
                            perror("Failed to send to SMBus");
                            exit(1);
                        }
                    break;
                    case OP_RECV:
                        if ((rv = i2c_smbus_read_byte(file)) < 0)
                        {
                            perror("Failed to recv from SMBus");
                            exit(1);
                        }
                        data[0] = rv & 0xFF;
                    break;
                }
            break;
        }
    }
    close(file);

    if (op == OP_RECV && mode != MODE_SMBUS_QUICK)
    {
        for (int i = 0; i < size; ++i)
        {
            printf("%02x ", ((unsigned char*)data)[i]);
        }
        printf("\n");
    }
    return 0;
}
