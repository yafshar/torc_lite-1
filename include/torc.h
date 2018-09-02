/*
 *  torc.h
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */

#ifndef _torc_included
#define _torc_included

#include <pthread.h>
#include <mpi.h>

/* Exported interface */
#ifdef __cplusplus
extern "C"
{
#endif

// CALL_BY_COP  requires that the pointer of the corresponding argument is passed to the routine
#define CALL_BY_COP (int)(0x0001)   /* IN    - By copy, through pointer to private copy (C) */ 
#define CALL_BY_REF (int)(0x0002)   /* INOUT - By reference */
#define CALL_BY_RES (int)(0x0003)   /* OUT   - By result */
#define CALL_BY_PTR (int)(0x0004)   /* IN    - By value, from address */
#define CALL_BY_VAL (int)(0x0001)   /* IN    - By value, from address (4: C, 0: Fortran */
#define CALL_BY_COP2 (int)(0x0005)  /* IN    - By copy, through pointer to private copy (C) */
#define CALL_BY_VAD (int)(0x0006)   /* IN    - By address - For Fortran Routines (Fortran) */

    /**
     * @brief Initializes the TORC execution environment 
     * Initilize the TORC library from MPI_COMM_WORLD
     * 
     * @param argc     Pointer to the number of arguments. 
     * @param argv     Argument vector. 
     * @param comm_in  Communicator (handle). 
     */
    void torc_init(int argc, char *argv[]);

    /**
     * @brief Initializes the TORC execution environment 
     * Initilize the TORC library from a communicator other than MPI_COMM_WORLD
     * 
     * @param argc     Pointer to the number of arguments. 
     * @param argv     Argument vector. 
     * @param comm_in  Communicator (handle). 
     */
    void torc_init_comm(int argc, char *argv[], MPI_Comm comm_in);

    void torc_reset_statistics(void);

    typedef double torc_time_t;
    torc_time_t torc_gettime(void);

    int torc_i_worker_id(void);
    int torc_i_num_workers(void);
    int torc_worker_id(void);
    int torc_num_workers(void);
    int torc_getlevel(void);

    void torc_enable_stealing(void);
    void torc_disable_stealing(void);
    void torc_i_enable_stealing(void);
    void torc_i_disable_stealing(void);
    void start_server_thread(void);
    void shutdown_server_thread(void);

    void torc_taskinit(void);
    void torc_waitall(void);
    void torc_waitall2(void);
    void torc_waitall3(void);
    void torc_tasksync(void);
    int torc_scheduler_loop(int);

    void torc_task(int queue, void (*f)(), int narg, ...);
    void torc_task_detached(int queue, void (*f)(), int narg, ...);
    void torc_task_ex(int queue, int invisible, void (*f)(), int narg, ...);
    void torc_task_direct(int queue, void (*f)(), int narg, ...);

#define torc_create torc_task
#define torc_create_detached torc_task_detached
#define torc_create_ex torc_task_ex
#define torc_create_direct torc_task_direct

    int torc_node_id(void);
    int torc_num_nodes(void);
    void torc_broadcast(void *a, long count, MPI_Datatype dtype);
    void torc_broadcast_ox(void *a, long count, int dtype);
    void thread_sleep(int ms);
    void torc_finalize(void);
    int torc_fetch_work(void);
    void torc_register_task(void *f);

#ifdef __cplusplus
}
#endif

#endif
