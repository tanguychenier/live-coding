#ifndef POOL_H
#define POOL_H

// the picture is made of rows that do not depend on each other, so the
// passes over it are cut in bands and handed to threads that wait for
// work. the caller takes a band too, and waits for the others
#define POOL_WORKERS_MAX  7

// a job works on the rows from first to last, last excluded
typedef void (*job_fn)(int first, int last, void *data);

void pool_open(void);
void pool_close(void);
// runs the job over count rows, cut in as many bands as there are threads
void pool_run(int count, job_fn job, void *data);

#endif
