/*
 *  torc_internal.h
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */

#ifndef _torc_internal_included
#define _torc_internal_included

#include <unistd.h>
#include <pthread.h>
#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#include <torc_config.h>

//! Maximum number of virtual processors
#ifndef MAX_NVPS
#define MAX_NVPS 64
#endif

//! Maximum number of arguments
#ifndef MAX_TORC_ARGS
#define MAX_TORC_ARGS 24
#endif

//! Maximum number of the tasks that the TORC library can register
#ifndef MAX_TORC_TASKS
#define MAX_TORC_TASKS 128
#endif

//! Run sequentialy
#define TORC_DEF_CPUS 1

//! 10ms default yield-time
#define TORC_DEF_YIELDTIME 10

typedef int INT32;
typedef long long INT64;
typedef unsigned long VIRT_ADDR;
typedef void (*func_t)();

#include "utils.h"

struct torc_desc;

typedef struct torc_desc
{
    //! Mutex
    _lock_t lock;

    //! pointers for the double linked-list queue
    struct torc_desc *prev;
    struct torc_desc *next;
    //!
    struct torc_desc *parent;
    //! Virtual processor ID
    long vp_id;
    //! Function pointer
    func_t work;
    //!
    int work_id;
    //! Number of arguments of the function
    int narg;
    //!
    int ndep;
    //!
    int homenode;
    //!
    int sourcenode;
    //!
    int sourcevpid;
    //!
    int target_queue;
    //!
    int inter_node;
    //!
    int insert_in_front;
    //!
    int insert_private;
    //!
    int rte_type;
    //! message type
    int type;
    //!
    int level;
    //! TORC type of each arguments of Function pointer
    int btype[MAX_TORC_ARGS];
    //! MPI_Datatype of each arguments of Function pointer
    MPI_Datatype dtype[MAX_TORC_ARGS];
    //! Number of each data of each arguments of Function pointer
    int quantity[MAX_TORC_ARGS];
    //! 
    int callway[MAX_TORC_ARGS];
    //! data (address / value) in the owner node
    INT64 localarg[MAX_TORC_ARGS]; 
    //! data (address / value) in the remote node
    INT64 temparg[MAX_TORC_ARGS];
} torc_t;

/* Internal */
torc_t *_torc_self(void);
torc_t *_torc_get_currt(void);

int _torc_depsatisfy(torc_t *);

void _torc_depadd(torc_t *, int);
void _torc_core_execution(torc_t *);
void _torc_set_work_routine(torc_t *, void (*)());
void _torc_switch(torc_t *, torc_t *, int);
void _torc_cleanup(torc_t *);
void _torc_set_currt(torc_t *);

int _torc_block(void);
int _torc_block2(void);
int _torc_scheduler_loop(int);

void _torc_stats(void);
void _torc_md_init(void);
void _torc_md_end(void);
void _torc_reset_statistics(void);
void _torc_env_init(void);
void _torc_opt(int, char **);
void __torc_opt(int, char **, MPI_Comm);
void *_torc_worker(void *arg);
void _torc_execute(void *);
void _torc_set_vpid(long);
long _torc_get_vpid(void);

/* Exported interface */
#include "torc_queue.h"
#include "torc_data.h"
#include "torc_mpi_internal.h"

/**
 * @brief Helper internal library function
 * 
 * @param F function pointer to register 
 */
void torc_register_task_internal(long long *F);

//! static flag for TORC initialization
extern int torc_initialized;

#endif
