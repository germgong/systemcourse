#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>
#include <numa.h>
#include <numaif.h>

#define C_MEMCPY "C library: memcpy"
#define SINGLE_THREAD "Singlethreading"
#define MULTI_THREAD "Multithreading"
#define MULTI_AFFINITY "Multithreading with affinity"
#define MEM_LOCAL "Multithreading with numa_alloc_local"
#define MEM_INTER "Multithreading with numa_alloc_interleaved"

/* You may need to define struct here */
typedef struct {
  float *dst;
  float *src;
  size_t size;
} param_t;


/*!
 * \brief subroutine function
 *
 * \param arg, input arguments pointer
 * \return void*, return pointer
 */
void *mt_memcpy(void *arg) {
    /* TODO: Your code here. */
  param_t *mt = (param_t *)arg;

  float *dst = mt->dst;
  float *src = mt->src;
  size_t size = mt->size;

  //float *in = (float *)src;
  //float *out = (float *)dst;
  //memcpy(out, in, size * sizeof(float));
  
  memcpy(dst, src, size * sizeof(float));
  
  //memcpy(dst, src, len * sizeof(float));


  return NULL;
}

/*!
 * \brief wrapper function
 *
 * \param dst, destination pointer
 * \param src, source pointer
 * \param size, copy bytes
 */
void multi_thread_memcpy(void *dst, const void *src, size_t size, int k) {
    /* TODO: Your code here. */

  float *in = (float *)src;
  float *out = (float *)dst;

  //compute chunk size
  int chunk_size = size / k;
  int r = size % k;
  param_t args[k];
  int lo = *src, hi = *src;
  //int lo = 0, hi = 0;
  for (int i = 0; i < k; ++i) {
      lo = hi;
      hi += chunk_size;
      args[i] = (param_t){out + lo, in + lo, hi - lo};
  }
  //args[k - 1].dst += r;
  if (r) { //extend the range of last chunk
      args[k - 1].size += r;
  }

  pthread_t ph[k];
  for (int i = 0; i < k; ++i) {
    if ( pthread_create(&ph[i], NULL, mt_memcpy, (void *)&args[i]) != 0 ) {
        fprintf(stderr, "pthread_create failed.\n");
        exit(1);
    }
  }
  for (int i = 0; i < k; ++i) {
    if ( pthread_join(ph[i], NULL) != 0 ) {
      fprintf(stderr, "pthread_join failed.\n");
      exit(1);
    }
  }
}

/*!
 * \brief bind new threads to the same NUMA node as the main
 * thread and then do multithreading memcy
 *
 * \param dst, destination pointer
 * \param src, source pointer
 * \param size, copy bytes
 */
void multi_thread_memcpy_with_affinity(void *dst, const void *src, size_t size, int k) {
    /* TODO: Your code here. */

  //set CPU affinity
  cpu_set_t cpu_set[k];
  pthread_attr_t attr[k];
  pthread_t ph[k];
  int cpu;
  assert( syscall(SYS_getcpu, &cpu, NULL, NULL) == 0 );
  printf("Main thread runs on CPU %d\n", cpu);
  int start = cpu % 2;

  printf("Set affinity mask to include CPUs (%d, %d, %d, ...  %s)\n", start,
           start + 2, start + 4, start ? "2n+1" : "2n");
  size_t nprocs = get_nprocs();
  int i = 0, j = start;
  while (i < k) {
    if (j == cpu) {
      j += 2;
      continue;
    }
    if (j >= nprocs) {
      j = start;
    }

    CPU_ZERO(&cpu_set[i]);
    CPU_SET(j, &cpu_set[i]);
    pthread_attr_setaffinity_np(&attr[i], sizeof(cpu_set_t), &cpu_set[i]);
    ++i;
    j += 2;
  }

  float *in = (float *)src;
  float *out = (float *)dst;

  //compute chunk size
  int chunk_size = size / k;
  int r = size % k;
  param_t args[k];
  //int lo = *in, hi = *in;
  int lo = *src, hi = *src;
  //int lo = 0, hi = 0;
  for (int i = 0; i < k; ++i) {
      lo = hi;
      hi += chunk_size;
      args[i] = (param_t){out + lo, in + lo, hi - lo};
  }
  //args[k - 1].dst += r;
  if (r) { //extend the range of last chunk
      args[k - 1].size += r;
  }

  for (int i = 0; i < k; ++i) {
    if ( pthread_create(&ph[i], &attr[i], mt_memcpy, (void *)&args[i]) != 0 ) {
      fprintf(stderr, "pthread_create failed.\n");
      exit(1);
    }
  }
  for (int i = 0; i < k; ++i) {
    if ( pthread_join(ph[i], NULL) != 0 ) {
      fprintf(stderr, "pthread_join failed.\n");
      exit(1);
    }
  }
}

/** You may would like to define a new struct for
 *  the bonus question here.
**/

#ifdef BUILD_BONUS
/*!
 * \brief (Bonus Question) subroutine function for the
 * bonus question.
 *
 * \param arg, input arguments pointer
 * \return void*, return pointer
 */
void *mt_page_memcpy(void *data) {
    /* TODO: (Bonus Question) Your code here. */
}

/*!
 * \brief (Bonus Question) bind new threads to different 
 * NUMA nodes. E.g., for 32 threads, you bind each node
 * with 16 threads. Run your code with two memory policies,
 * 1) *local*, 2) *interleave*.
 *
 * \param dst, destination pointer
 * \param src, source pointer
 * \param size, size of the data
 * \param k, # of threads
 */
void multi_thread_memcpy_with_interleaved_affinity(void *dst, const void *src, size_t size, int k) {
  /* TODO: (Bonus Question) Your code here. */
}
#endif

/* benchmark: single-threaded version */
void single_thread_memcpy(void *dst, const void *src, size_t size) {
  float *in = (float *)src;
  float *out = (float *)dst;

  for (size_t i = 0; i < size / 4; ++i) {
    out[i] = in[i];
  }
  if (size % 4) {
    memcpy(out + size / 4, in + size / 4, size % 4);
  }
}

int execute(const char *command, int len, int k)
{
  /* allocate memory */
  float *dst = (float *) malloc( len * sizeof(float) );
  float *src = (float *) malloc( len * sizeof(float) );
  assert(dst != NULL);
  assert(src != NULL);

  /* warmup */
  memcpy(dst, src, len * sizeof(float));

  /* timing the memcpy */
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  if ( strcmp(command, C_MEMCPY)==0 )
  {
    memcpy(dst, src, len*sizeof(float));
  }
  else if ( strcmp(command, SINGLE_THREAD)==0 )
  {
    single_thread_memcpy(dst, src, len*sizeof(float));
  }
  else if ( strcmp(command, MULTI_THREAD)==0 )
  {
    multi_thread_memcpy(dst, src, len*sizeof(float), k);
  }
  else if ( strcmp(command, MULTI_AFFINITY)==0 )
  {
    multi_thread_memcpy_with_affinity(dst, src, len*sizeof(float), k);
  }
  else
  {
    fprintf(stderr, "execution failure.\n");
    goto out;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  /* check correctness (with "warmup" disabled) */
  assert( memcmp(src, dst, len*sizeof(float)) == 0 );

  float delta_us = (end.tv_sec - start.tv_sec) * 1.0e6 +
                     (end.tv_nsec - start.tv_nsec) * 1.0e-3;
  printf("[%s]\tThe throughput is %.2f Gbps.\n",
          command, len*sizeof(float)*8 / (delta_us*1000.0) );

out: 
  /* free the memory */
  free(dst);
  free(src);

  return 0;
}

#ifdef BUILD_BONUS
int execute_numa(const char *command, int len, int k)
{
  /* allocate memory */
  float *dst, *src;
  if ( strcmp(command, MEM_LOCAL)==0 )
  {
    /* allocate memory (`*src`, `*dst`) locally on the current node */
    //TODO: (Bonus) Your code here.
  }
  else if ( strcmp(command, MEM_INTER)==0 )
  {
    /* allocate memory (`*src`, `*dst`) interleaved on each node */
    //TODO: (Bonus) Your code here.
  }
  else
  {
    fprintf(stderr, "numa execution failure.\n");
    return -1;
  }
  assert(dst != NULL);
  assert(src != NULL);

  /* warmup */
  memcpy(dst, src, len * sizeof(float));

  /* timing the memcpy */
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  multi_thread_memcpy_with_interleaved_affinity(dst, src, len*sizeof(float), k);
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  /* check correctness (with "warmup" disabled) */
  assert( memcmp(src, dst, len*sizeof(float)) == 0 );

  float delta_us = (end.tv_sec - start.tv_sec) * 1.0e6 +
                     (end.tv_nsec - start.tv_nsec) * 1.0e-3;
  printf("[%s]\tThe throughput is %.2f Gbps.\n",
          command, len*sizeof(float)*8 / (delta_us*1000.0) );
  
  /* free `*dst` and `*src` */
  //TODO: (Bonus) Your code here.

  return 0;
}
#endif

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr,
            "Error: The program accepts exact 2 intergers.\n The first is the "
            "vector size and the second is the number of threads.\n");
    exit(1);
  }
  const int len = atoi(argv[1]);
  const int k = atoi(argv[2]);
  if (len < 0 || k < 1) {
    fprintf(stderr, "Error: invalid arguments.\n");
    exit(1);
  }
  // printf("Vector size=%d\tthreads len=%d.\n", len, k);

  /* C library's memcpy (1 byte) */
  execute(C_MEMCPY, len, k);
  /* single-threaded memcpy (4 bytes) */
  execute(SINGLE_THREAD, len, k);
  /* multi-threaded memcpy */
  execute(MULTI_THREAD, len, k);
  /* multi-threaded memcpy with affinity set */
  execute(MULTI_AFFINITY, len, k);

#ifdef BUILD_BONUS
  /* Bonus: multi-threaded memcpy with local NUMA memory policy */
  execute_numa(MEM_LOCAL, len, k);
  /* Bonus: multi-threaded memcpy with interleaved NUMA memory policy */
  execute_numa(MEM_INTER, len, k);
#endif

  return 0;
}
