/* CPU Burner which uses Mersenne Twister */
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/*
 * Mersenne Twister algorithm  implementation is below.
 * It were modified for multithreading
 */
/*
   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#define MAX_THREADS 32

/* Period parameters */
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned long mt[MAX_THREADS][N]; /* the array for the state vector  */
static int mti[MAX_THREADS]={N+1, N+1, N+1, N+1, N+1, N+1, N+1, N+1,
                             N+1, N+1, N+1, N+1, N+1, N+1, N+1, N+1,
                             N+1, N+1, N+1, N+1, N+1, N+1, N+1, N+1,
                             N+1, N+1, N+1, N+1, N+1, N+1, N+1, N+1,};
/* mti==N+1 means mt[N] is not initialized */

/* initializes mt[N] with a seed */
void init_genrand(int tid, unsigned long s)
{
  mt[tid][0]= s & 0xffffffffUL;
  for (mti[tid]=1; mti[tid]<N; mti[tid]++) {
    mt[tid][mti[tid]] =
	    (1812433253UL * (mt[tid][mti[tid]-1] ^ (mt[tid][mti[tid]-1] >> 30)) + mti[tid]);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
    mt[tid][mti[tid]] &= 0xffffffffUL;
    /* for >32 bit machines */
  }
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
void init_by_array(int tid, unsigned long init_key[], int key_length)
{
  int i, j, k;
  init_genrand(tid, 19650218UL);
  i=1; j=0;
  k = (N>key_length ? N : key_length);
  for (; k; k--) {
    mt[tid][i] = (mt[tid][i] ^ ((mt[tid][i-1] ^ (mt[tid][i-1] >> 30)) * 1664525UL))
                 + init_key[j] + j; /* non linear */
    mt[tid][i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
    i++; j++;
    if (i>=N) { mt[tid][0] = mt[tid][N-1]; i=1; }
    if (j>=key_length) j=0;
  }
  for (k=N-1; k; k--) {
    mt[tid][i] = (mt[tid][i] ^ ((mt[tid][i-1] ^ (mt[tid][i-1] >> 30)) * 1566083941UL))
                 - i; /* non linear */
    mt[tid][i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
    i++;
    if (i>=N) { mt[tid][0] = mt[tid][N-1]; i=1; }
  }

  mt[tid][0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
}

/* generates a random number on [0,0xffffffff]-interval */
unsigned long genrand_int32(int tid)
{
  unsigned long y;
  static unsigned long mag01[2]={0x0UL, MATRIX_A};
  /* mag01[x] = x * MATRIX_A  for x=0,1 */

  if (mti[tid] >= N) { /* generate N words at one time */
    int kk;

    if (mti[tid] == N+1)   /* if init_genrand() has not been called, */
      init_genrand(tid, 5489UL); /* a default initial seed is used */

    for (kk=0;kk<N-M;kk++) {
      y = (mt[tid][kk]&UPPER_MASK)|(mt[tid][kk+1]&LOWER_MASK);
      mt[tid][kk] = mt[tid][kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (;kk<N-1;kk++) {
      y = (mt[tid][kk]&UPPER_MASK)|(mt[tid][kk+1]&LOWER_MASK);
      mt[tid][kk] = mt[tid][kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (mt[tid][N-1]&UPPER_MASK)|(mt[tid][0]&LOWER_MASK);
    mt[tid][N-1] = mt[tid][M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

    mti[tid] = 0;
  }

  y = mt[tid][mti[tid]++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680UL;
  y ^= (y << 15) & 0xefc60000UL;
  y ^= (y >> 18);

  return y;
}

/* CPU Burner implementation */
#define WRK_FREQ    10 //Hz

unsigned long time_msec = 0;
volatile bool stop = false;
unsigned long thread_stat[MAX_THREADS];
/* To mess up with optimizer */
int utilization = 100;
int tloop = 0;
int twait = 0;

static long int usecdiff(struct timespec *a, struct timespec *b)
{
  return (b->tv_sec - a->tv_sec) * 1000000 +
      (b->tv_nsec - a->tv_nsec) / 1000;
}

static void* worker(void *arg)
{
  int idx = (int)arg;
  unsigned long number;
  long int tdiff;
  struct timespec tstart, tend;

  while (!stop) {
    clock_gettime(CLOCK_REALTIME, &tstart);

    while (true) {
      for (long int i = 0; i < 10000; i++)
        number = genrand_int32(idx);

      thread_stat[idx]++;

      clock_gettime(CLOCK_REALTIME, &tend);
      tdiff = usecdiff(&tstart, &tend);

      if (tdiff >= tloop) {
        usleep(twait);
        break;
      }
    }
  }

  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
  unsigned long init[4]={0x123, 0x234, 0x345, 0x456}, length=4;
  int nsecs = 10;
  int nthreads = 1;
  pthread_t threads[MAX_THREADS];
  struct timespec tstart, tend;
  long int time_spent;
  long int total_stats;
  signed char opt;

  while ((opt = getopt(argc, argv, "t:u:c:")) != -1) {
    switch (opt) {
      case 't':
        nsecs = atoi(optarg);
        break;
      case 'u':
        utilization = atoi(optarg);
        break;
      case 'c':
        nthreads = atoi(optarg);
        break;
      default: /* '?' */
        fprintf(stderr, "Usage: %s [-t nsecs] [-u utilization] [-c ncores]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (nthreads > MAX_THREADS) {
    fprintf(stderr, "Error: no more than %d threads supported\n", MAX_THREADS );
        exit(EXIT_FAILURE);
  }

  if (nsecs < 1) {
    fprintf(stderr, "Error: Positive time is required\n");
        exit(EXIT_FAILURE);
  }

  if (utilization > 100 || utilization < 0) {
    fprintf(stderr, "Error: invalid CPU utilization value: %d\n", utilization);
        exit(EXIT_FAILURE);
  }

  tloop = (1000000/WRK_FREQ) * utilization / 100;
  twait = (1000000/WRK_FREQ) - tloop;

  printf("Starting CPU burner for %d seconds, %d core(s), with %d%% CPU utilization\n",
         nsecs, nthreads, utilization);

  /* Initialize MT */
  for (int i = 0; i < nthreads; i++)
    init_by_array(i, init, length);

  clock_gettime(CLOCK_REALTIME, &tstart);
  for (int i = 0; i < nthreads; i++)
  {
    if (pthread_create(threads + i, NULL, worker, (void*)i))
    {
      fprintf(stderr, "Error: failed to spawn a thread\n");
      exit(EXIT_FAILURE);
    }
  }

  sleep(nsecs);
  stop = true;

  for (int i = 0; i < nthreads; i++)
    pthread_join(threads[i], NULL);

  clock_gettime(CLOCK_REALTIME, &tend);
  time_spent = usecdiff(&tstart, &tend);

  total_stats = 0;
  for (int i = 0; i < nthreads; i++)
  {
    printf("Stat[%d] = %ld\n", i, thread_stat[i]);
    total_stats += thread_stat[i];
  }

  printf("Total time passed: %f s\nTotal: %ld cycles, %f Kcycles/s\n",
         (double)time_spent / 1000000, total_stats,
         (double)total_stats * 1000 / time_spent);

}
