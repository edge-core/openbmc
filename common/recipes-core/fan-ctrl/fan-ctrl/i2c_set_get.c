#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

static void print_err_usage(char *name) {
  fprintf(stderr, "usage: %s <i2c-bus> <i2c-addr> <write-cnt> <read-cnt> [<write-byte0> <write-byte-1> ...]\n", name);
} 
static void print_err_msg(char *msg) {
  fprintf(stderr, "%s\n", msg);
}

static int open_i2c_dev(int i2cbus, char *filename, size_t size, int quiet)
{
  int file;

  snprintf(filename, size, "/dev/i2c/%d", i2cbus);
  filename[size - 1] = '\0';
  file = open(filename, O_RDWR);

  if (file < 0 && (errno == ENOENT || errno == ENOTDIR)) {
    sprintf(filename, "/dev/i2c-%d", i2cbus);
    file = open(filename, O_RDWR);
  }

  if (file < 0 && !quiet) {
    if (errno == ENOENT) {
      fprintf(stderr, "Error: Could not open file "
                      "`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
                      i2cbus, i2cbus, strerror(ENOENT));
    } else {
      fprintf(stderr, "Error: Could not open file "
                      "`%s': %s\n", filename, strerror(errno));
      if (errno == EACCES) {
        fprintf(stderr, "Run as root?\n");
      }
    }
  }
  return file;
}

static int set_slave_addr(int file, int address, int force)
{
  /* With force, let the user read from/write to the registers
     even when a driver is also running */
  if (ioctl(file, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
    fprintf(stderr,
            "Error: Could not set address to 0x%02x: %s\n",
            address, strerror(errno));
    return -errno;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int i, err, bus, wr_cnt, rd_cnt, file;
  int i2c_addr;
  unsigned char wr_buf[64];
  unsigned char rd_buf[64];
  char filename[32];
  struct i2c_rdwr_ioctl_data i2c_rdwr_data;
  struct i2c_msg i2c_msg_data[2];

  if (argc < 5) {
    print_err_usage(argv[0]);
    exit(1);
  }
  bus = atoi(argv[1]);
  i2c_addr = (int)strtoul(argv[2], NULL, 0);
  if (i2c_addr > 0x7f) {
    print_err_usage("bad i2c-addr");
    exit(1);
  }
  wr_cnt = atoi(argv[3]);
  rd_cnt = atoi(argv[4]);
  if (wr_cnt > 64 || rd_cnt > 64 || (wr_cnt == 0 && rd_cnt == 0)) {
    print_err_usage("bad write-cnt and/or read-cnt");
    exit(1);
  }
  if (argc < (5 + wr_cnt)) {
    print_err_usage("Error: not enough arguments");
    print_err_usage(argv[0]);
    exit(1);
  }
  for (i = 0 ; i < wr_cnt; i++) {
    wr_buf[i] = (unsigned char)strtoul(argv[5 + i], NULL, 0);
  }
  file = open_i2c_dev(bus, filename, sizeof(filename), 0);
  if (file < 0 || set_slave_addr(file, i2c_addr, 1)) {
    print_err_usage("Error: cannot open i2c device file");
  }

  /* populate i2c_msg structures */
  memset(i2c_msg_data, 0 , sizeof(i2c_msg_data));
  i = 0;
  if (wr_cnt) {
    i2c_msg_data[i].addr = (unsigned short)i2c_addr;
    i2c_msg_data[i].flags = 0; /* write type cycle */
    i2c_msg_data[i].len = wr_cnt;
    i2c_msg_data[i].buf = wr_buf;
    i++;
  }
  if (rd_cnt) {
    i2c_msg_data[i].addr = (unsigned short)i2c_addr;
    i2c_msg_data[i].flags = I2C_M_RD; /* read type cycle */
    i2c_msg_data[i].len = rd_cnt;
    i2c_msg_data[i].buf = rd_buf;
    i++;
  }
  i2c_rdwr_data.msgs = i2c_msg_data;
  i2c_rdwr_data.nmsgs = (unsigned int)i;
  err = ioctl(file, I2C_RDWR, &i2c_rdwr_data);
  if (err == -1) {
    perror("i2c_rdwrd ioctl error ");
    exit(1);
  }
  if (rd_cnt) {
    for (i = 0; i < rd_cnt; i++) {
      printf("0x%02x ", rd_buf[i]);
    }
    printf("\n");
  }
  exit(0);
}

