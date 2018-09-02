/*
 *  torc_mpi_internal.h
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */

#ifndef _torc_mpi_internal_included
#define _torc_mpi_internal_included

#ifndef MAX_NODES
#define MAX_NODES 1024
#endif

#define TERMINATE_LOCAL_SERVER_THREAD 120
#define TERMINATE_WORKER_THREADS 121
#define DIRECT_SYNCHRONOUS_STEALING_REQUEST 123
#define DISABLE_INTERNODE_STEALING 124
#define ENABLE_INTERNODE_STEALING 125
#define RESET_STATISTICS 126

#define TORC_NORMAL 139
#define TORC_ANSWER 140
#define TORC_NORMAL_ENQUEUE 141
#define TORC_NO_WORK 142
#define TORC_BCAST 145

enum
{
    /* C types */
    T_MPI_CHAR = 0,
    T_MPI_SIGNED_CHAR,
    T_MPI_UNSIGNED_CHAR,
    T_MPI_BYTE,
    T_MPI_WCHAR,
    T_MPI_SHORT,
    T_MPI_UNSIGNED_SHORT,
    T_MPI_INT,
    T_MPI_UNSIGNED,
    T_MPI_LONG,
    T_MPI_UNSIGNED_LONG,
    T_MPI_FLOAT,
    T_MPI_DOUBLE,
    T_MPI_LONG_DOUBLE,
    T_MPI_LONG_LONG_INT,
    T_MPI_LONG_LONG = T_MPI_LONG_LONG_INT,
    T_MPI_UNSIGNED_LONG_LONG,

    /* Fortran types */
    T_MPI_COMPLEX,
    T_MPI_DOUBLE_COMPLEX,
    T_MPI_LOGICAL,
    T_MPI_REAL,
    T_MPI_DOUBLE_PRECISION,
    T_MPI_INTEGER,
    T_MPI_2INTEGER,
    T_MPI_CHARACTER
};

extern MPI_Comm comm_out;

struct node_info
{
    int nprocessors;
    int nworkers;
};

extern struct node_info *node_info;

#define Error(msg)                                  \
    {                                               \
        printf("ERROR in %s: %s\n", __func__, msg); \
        MPI_Abort(comm_out, 1);                     \
    }

#define Error1(msg, arg)                  \
    {                                     \
        printf("ERROR in %s:", __func__); \
        printf(msg, arg);                 \
        printf("\n");                     \
        MPI_Abort(comm_out, 1);           \
    }

#define Warning1(msg, arg)                  \
    {                                       \
        printf("WARNING in %s:", __func__); \
        printf(msg, arg);                   \
        printf("\n");                       \
    }


int global_thread_id_to_node_id(int global_thread_id);
int local_thread_id_to_global_thread_id(int local_thread_id);
int global_thread_id_to_local_thread_id(int global_thread_id);
int _torc_total_num_threads(void);
void enter_comm_cs(void);
void leave_comm_cs(void);
void _torc_comm_pre_init(void);
void _torc_comm_init(void);
void *server_loop(void *arg);
//! notify_appl_finished()
void terminate_workers(void);
void send_descriptor(int, torc_t *, int);
void direct_send_descriptor(int dummy, int sourcenode, int sourcevpid, torc_t *desc);
void receive_arguments(torc_t *work, int tag);
void receive_descriptor(int node, torc_t *work);
torc_t *direct_synchronous_stealing_request(int target_node);
func_t getfuncptr(int funcpos);
int getfuncnum(func_t f);
int _torc_mpi2b_type(MPI_Datatype dtype);
MPI_Datatype _torc_b2mpi_type(int btype);

#endif
