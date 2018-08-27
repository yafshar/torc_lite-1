/*
 *  torc_thread.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

typedef void (*sched_f)();

pthread_mutex_t __m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t al = PTHREAD_MUTEX_INITIALIZER;

unsigned int __created = 0;

static int active_workers;

void *_torc_worker(void *arg)
{
    long vp_id = (long)arg;

    torc_t *desc = (torc_t *)calloc(1, sizeof(torc_t));

    desc->vp_id = vp_id;

    _lock_init(&desc->lock);

    desc->work = (sched_f)_torc_scheduler_loop;

#if DEBUG
    printf("[RTE %p]: NODE %d: WORKER THREAD %ld --> 0x%lx\n", desc, torc_node_id(), desc->vp_id, pthread_self());
    fflush(0);
#endif

    {
        pthread_mutex_lock(&__m);
        __created++;
        pthread_mutex_unlock(&__m);
    }

    worker_thread[desc->vp_id] = pthread_self();

    _torc_set_vpid(vp_id);

    _torc_set_currt(desc);

    int repeat;

    while (__created < kthreads)
    {
        {
            pthread_mutex_lock(&__m);
            repeat = (__created < kthreads);
            pthread_mutex_unlock(&__m);
        }

        if (repeat)
        {
            thread_sleep(10);
        }
        else
        {
            break;
        }
    }

    if (vp_id == 0)
    {
        enter_comm_cs();
        MPI_Barrier(comm_out);
        leave_comm_cs();
    }

    if ((torc_node_id() == 0) && (vp_id == 0))
    {
        return 0;
    }

    _torc_scheduler_loop(0); /* never returns */

    return 0;
}

/**
 * @brief Initilize detached workers on each thread to execute independently from the thread handle 
 * 
 * @param id Thread ID
 */
void start_worker(long id)
{
    pthread_t pth;
    pthread_attr_t attr;

    {
        pthread_attr_init(&attr);
        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }

    int res = pthread_create(&pth, &attr, _torc_worker, (void *)id);

    if (res == 0)
    {
        worker_thread[id] = pth;
    }
    else
    {
        Error("pthread_create failed");
    }
}

// void shutdown_worker(int id)
// {
//     int const this_node = torc_node_id();

//     node_info[this_node].nworkers--;
// }

/**
 * @brief Initilize the workers
 * 
 */
void _torc_md_init()
{
    pthread_key_create(&vp_key, NULL);
    pthread_key_create(&currt_key, NULL);

    if (torc_num_nodes() > 1)
    {
        start_server_thread();
    }

    //! For each thread create a detached worker
    for (unsigned int i = 1; i < kthreads; i++)
    {
        start_worker((long)i);
    }

    active_workers = (int)kthreads;
}

/**
 * @brief Stop the workers
 * 
 */
void _torc_md_end()
{
    unsigned int my_vp = _torc_get_vpid();

#if DEBUG
    printf("worker_thread %ud exits\n", my_vp);
    fflush(0);
#endif

    if (my_vp)
    {
        pthread_mutex_lock(&al);
        active_workers--;
        pthread_mutex_unlock(&al);

        pthread_exit(0);
    }

    if (!my_vp)
    {
        while (1)
        {
            pthread_mutex_lock(&al);
            if (active_workers == 1)
            {
                pthread_mutex_unlock(&al);
                break;
            }
            else
            {
                pthread_mutex_unlock(&al);
                sched_yield();
            }
        }

        //! We need a barrier here to avoid potential deadlock problems
        enter_comm_cs();
        MPI_Barrier(comm_out);
        leave_comm_cs();

        if (torc_num_nodes() > 1)
        {
            shutdown_server_thread();
        }

        _torc_stats();

        MPI_Barrier(comm_out);
        MPI_Finalize();
        exit(0);
    }
}

void thread_sleep(int ms)
{
    struct timespec req, rem;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1E6;

    nanosleep(&req, &rem);
}

void _torc_set_vpid(long vp)
{
    pthread_setspecific(vp_key, (void *)vp);
}

long _torc_get_vpid()
{
    return (long)pthread_getspecific(vp_key);
}

void _torc_set_currt(torc_t *task)
{
    pthread_setspecific(currt_key, (void *)task);
}

torc_t *_torc_get_currt()
{
    return (torc_t *)pthread_getspecific(currt_key);
}

void F77_FUNC_(torc_sleep, TORC_SLEEP)(int *ms)
{
    thread_sleep(*ms);
}
