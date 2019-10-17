#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include "i2cbusses.h"
#include "util.h"
#include "../version.h"

#define BUS 4
#define ADDR_CA57 0x7F
#define ADDR_SOC 0x7C
#define LSB_V   14 //mV
#define LSB_A   26880 //nV
#define R       5 //mOhm

static int to12bit(int val)
{
  return ((val >> 8) + ((val & 0xFF) << 8)) >> 4;
}

static double process(char addr, const char *name, int file, bool print)
{
  int res;
  long int mV, uA;
  double P;

  res = set_slave_addr(file, addr, 0);
  if (res < 0)
    exit(1);

  /* Read voltage */
  res = i2c_smbus_write_byte_data(file, 0xA, 0x3);
  if (res < 0)
    exit(1);

  usleep(3000);
  res = i2c_smbus_read_word_data(file, 0x2);
  if (res < 0)
    exit(1);
  mV = LSB_V * to12bit(res);

  usleep(3000);
  /* Read current */
  res = i2c_smbus_write_byte_data(file, 0xA, 0x1);
  if (res < 0)
    exit(1);

  usleep(3000);
  res = i2c_smbus_read_word_data(file, 0x0);
  if (res < 0)
    exit(1);

  uA = to12bit(res) * LSB_A / R;
  P = ((double)mV) * ((double)uA) / 1000000000;
  if (print)
    printf("%s U=%f V I=%f A P=%f W\n", name, ((double)mV) / 1000,
           ((double)uA) / 1000000, P);

  return P;
}

static long int usecdiff(struct timespec *a, struct timespec *b)
{
  return (b->tv_sec - a->tv_sec) * 1000000 +
      (b->tv_nsec - a->tv_nsec) / 1000;
}

int main(int argc, char *argv[])
{
  char filename[32];
  int file;
  int span;
  long int uspan;
  struct timespec cur, prev;
  double totalE_SOC = 0;
  double totalE_A57 = 0;

  file = open_i2c_dev(BUS, filename, sizeof(filename), 0);
  if (file < 0)
    exit(1);

  if ( argc == 1 )
  {
    process(ADDR_SOC, "SOC ", file, true);
    process(ADDR_CA57, "CA57", file, true);
    exit(0);
  }

  span = atoi(argv[1]);
  printf("Measuring for %d seconds\n", span);

  uspan = span * 1000000;
  clock_gettime(CLOCK_REALTIME, &prev);

  while (uspan > 0)
  {
    long int diff;
    double E_SOC, E_A57;
    clock_gettime(CLOCK_REALTIME, &cur);

    diff = usecdiff(&prev, &cur);
    uspan -= diff;
    E_SOC = process(ADDR_SOC, "SOC ", file, false);
    E_A57 = process(ADDR_CA57, "CA57", file, false);

    totalE_SOC += E_SOC * diff / 1000000;
    totalE_A57 += E_A57 * diff / 1000000;

    prev = cur;
    usleep(100 * 1000);
  }

  double total_time = (((double)span * 1000000) - uspan) / 1000000;
  printf("Total time: %f SOC: %f J (%f W),    CA57: %f J (%f W)\n",
         total_time,
         totalE_SOC, totalE_SOC/total_time,
         totalE_A57, totalE_A57/total_time);
  printf("Total energy: %f J mean power: %f W\n", totalE_SOC + totalE_A57,
         (totalE_SOC + totalE_A57)/total_time);
}
