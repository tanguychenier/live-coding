#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pool.h"

static struct {
	pthread_t thread[POOL_WORKERS_MAX];
	pthread_mutex_t lock;
	pthread_cond_t wake, done;
	int workers, running;
	// the job of the moment, its rows, and how many bands are still at it
	job_fn job;
	void *data;
	int count, bands, band_next, band_left;
	unsigned int generation;
} pool;

// the band a worker takes, from the next one not yet taken
static int take_band(int *first, int *last)
{
	if (pool.band_next >= pool.bands)
		return 0;
	int band = pool.band_next++;
	*first = pool.count * band / pool.bands;
	*last = pool.count * (band + 1) / pool.bands;
	return 1;
}

static void *work(void *unused)
{
	(void)unused;
	unsigned int seen = 0;
	pthread_mutex_lock(&pool.lock);
	while (pool.running) {
		if (pool.generation == seen) {
			pthread_cond_wait(&pool.wake, &pool.lock);
			continue;
		}
		seen = pool.generation;
		int first, last;
		while (take_band(&first, &last)) {
			pthread_mutex_unlock(&pool.lock);
			pool.job(first, last, pool.data);
			pthread_mutex_lock(&pool.lock);
			if (--pool.band_left == 0)
				pthread_cond_broadcast(&pool.done);
		}
	}
	pthread_mutex_unlock(&pool.lock);
	return NULL;
}

void pool_open(void)
{
	memset(&pool, 0, sizeof pool);
	pthread_mutex_init(&pool.lock, NULL);
	pthread_cond_init(&pool.wake, NULL);
	pthread_cond_init(&pool.done, NULL);
	// one thread per core but the caller's own, or the number the bench
	// gives. zero threads and the caller does all the work itself
	long cores = sysconf(_SC_NPROCESSORS_ONLN);
	const char *asked = getenv("TEC_THREADS");
	if (asked)
		cores = atol(asked) + 1;
	pool.workers = (int)(cores > POOL_WORKERS_MAX + 1 ? POOL_WORKERS_MAX : cores - 1);
	if (pool.workers < 0)
		pool.workers = 0;
	pool.running = 1;
	for (int i = 0; i < pool.workers; i++)
		if (pthread_create(&pool.thread[i], NULL, work, NULL) != 0) {
			pool.workers = i;
			break;
		}
}

void pool_close(void)
{
	pthread_mutex_lock(&pool.lock);
	pool.running = 0;
	pthread_cond_broadcast(&pool.wake);
	pthread_mutex_unlock(&pool.lock);
	for (int i = 0; i < pool.workers; i++)
		pthread_join(pool.thread[i], NULL);
}

void pool_run(int count, job_fn job, void *data)
{
	if (pool.workers == 0 || count < 2) {
		job(0, count, data);
		return;
	}
	pthread_mutex_lock(&pool.lock);
	pool.job = job;
	pool.data = data;
	pool.count = count;
	pool.bands = pool.workers + 1;
	pool.band_next = 0;
	pool.band_left = pool.bands;
	pool.generation++;
	pthread_cond_broadcast(&pool.wake);
	// the caller works too, then waits for the last band
	int first, last;
	while (take_band(&first, &last)) {
		pthread_mutex_unlock(&pool.lock);
		job(first, last, data);
		pthread_mutex_lock(&pool.lock);
		pool.band_left--;
	}
	while (pool.band_left > 0)
		pthread_cond_wait(&pool.done, &pool.lock);
	pthread_mutex_unlock(&pool.lock);
}
