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

//! Number of created workers
static unsigned int created_workers = 0;

//! Number of active workers
static unsigned int active_workers;

//! Created worker mutex
pthread_mutex_t created_workers_m = PTHREAD_MUTEX_INITIALIZER;

//! Active workers mutex
pthread_mutex_t active_workers_m = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Create a new worker
 * 
 * @param arg Virtual processor ID
 * @return void* 
 */
void *_torc_worker(void *arg)
{
    //! Size of the data structure
    static unsigned long torc_size = sizeof(torc_t);

    long vp_id = (long)arg;

    torc_t *desc = (torc_t *)calloc(1, torc_size);

    desc->vp_id = vp_id;

    _lock_init(&desc->lock);

    desc->work = (func_t)_torc_scheduler_loop;

#if DEBUG
    printf("[RTE %p]: NODE %d: WORKER THREAD %ld --> 0x%lx\n", desc, torc_node_id(), desc->vp_id, pthread_self());
    fflush(0);
#endif

    {
        pthread_mutex_lock(&created_workers_m);
        created_workers++;
        pthread_mutex_unlock(&created_workers_m);
    }

    worker_thread[desc->vp_id] = pthread_self();

    _torc_set_vpid(vp_id);

    _torc_set_currt(desc);

    int repeat;

    while (created_workers < kthreads)
    {
        {
            pthread_mutex_lock(&created_workers_m);
            repeat = (created_workers < kthreads);
            pthread_mutex_unlock(&created_workers_m);
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
        Error("pthread_create failed!");
    }
}

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

    active_workers = kthreads;
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
        pthread_mutex_lock(&active_workers_m);
        active_workers--;
        pthread_mutex_unlock(&active_workers_m);

        //! Terminates the calling thread
        pthread_exit(0);
    }

    if (!my_vp)
    {
        while (1)
        {
            pthread_mutex_lock(&active_workers_m);
            if (active_workers == 1)
            {
                pthread_mutex_unlock(&active_workers_m);
                break;
            }
            else
            {
                pthread_mutex_unlock(&active_workers_m);
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

/**
 * @brief high-resolution sleep
 * Suspends the execution of the calling thread until at least the time specified elapsed
 * 
 * @param ms Time specified in miliseconds 
 */
void thread_sleep(int ms)
{
    struct timespec req, rem;

    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1E6;

    nanosleep(&req, &rem);
}

/**
 * @brief Set the TORC descriptor bound to the vp_key on behalf of the calling thread 
 * 
 * @param vp 
 */
void _torc_set_vpid(long vp)
{
    pthread_setspecific(vp_key, (void *)vp);
}

/**
 * @brief Get the virtual processor ID which is the value currently bound to the vp_key on behalf of the calling thread
 * 
 * @return long 
 */
long _torc_get_vpid()
{
    return (long)pthread_getspecific(vp_key);
}

/**
 * @brief Set the TORC descriptor bound to the currt_key on behalf of the calling thread 
 * 
 * @param desc 
 */
void _torc_set_currt(torc_t *desc)
{
    pthread_setspecific(currt_key, (void *)desc);
}

/**
 * @brief Get the TORC descriptor bound to the currt_key on behalf of the calling thread
 * 
 * @return torc_t* 
 */
torc_t *_torc_get_currt()
{
    return (torc_t *)pthread_getspecific(currt_key);
}

void F77_FUNC_(torc_sleep, TORC_SLEEP)(int *ms)
{
    thread_sleep(*ms);
}
