/*
 *  torc_comm.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

//! Node Inofrmation
struct node_info *node_info;

//! Number of registered tasks(functions)
static int number_of_functions = 0;

//! Table of tasks
static func_t internode_function_table[MAX_TORC_TASKS];

//! Communication mutex object
pthread_mutex_t comm_m = PTHREAD_MUTEX_INITIALIZER;

//! External flag to register the function even after torc_init
extern int torc_initialized;

/**
 * @brief locks the mutex before the scope, in case of MPI implementation is not thread safe
 * 
 */
void enter_comm_cs()
{
    if (!thread_safe)
    {
        _lock_acquire(&comm_m);
    }
}

/**
 * @brief unlocks the mutex after the scope, in case of MPI implementation is not thread safe
 * 
 */
void leave_comm_cs()
{
    if (!thread_safe)
    {
        _lock_release(&comm_m);
    }
}

/************************  INTER-NODE ROUTINES   *************************/

/**
 * @brief Register a task
 * We have to register different tasks
 * 
 * @param f Input task(function)
 */
void torc_register_task_internal(long long *F)
{
    void *f = (void *)*F;
    internode_function_table[number_of_functions] = (func_t)f;
    number_of_functions++;
}

/**
 * @brief Register a task
 * We have to register different tasks
 * 
 * @param f Input task(function)
 */
void torc_register_task(void *f)
{
    if (torc_initialized)
    {
        long long F = (long long)f;
        for (int i = 0; i < torc_num_nodes(); i++)
        {
            torc_create_ex(i * torc_i_num_workers(), 1, torc_register_task_internal, 1,
                           1, MPI_LONG_LONG_INT, CALL_BY_COP,
                           &F);
        }
        torc_waitall();
        return;
    }

    internode_function_table[number_of_functions] = (func_t)f;
    number_of_functions++;
}

/**
 * @brief Get the Index of the task in the table
 * 
 * @param f inquired task
 * @return int Index of the task in the table
 */
int getfuncnum(func_t f)
{
    for (int i = 0; i < number_of_functions; i++)
    {
        if (f == internode_function_table[i])
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Get the function pointer from the table
 * 
 * @param pos Index of a function in the table
 * 
 * @return func_t Functin pointer 
 * In case of a failure, it returns NULL
 */
func_t getfuncptr(int pos)
{
    return pos < MAX_TORC_TASKS ? internode_function_table[pos] : NULL;
}

/**********************   INITIALIZATION ROUTINES   **********************/

/**
 * @brief this routine allocate the memory fr node information prior to initialization
 * 
 */
void _torc_comm_pre_init()
{
    static int already_called = -1;

    already_called++;
    if (already_called)
    {
        printf("_rte_comm_pre_init has been called already\n");
        return;
    }

    node_info = (struct node_info *)calloc(1, MAX_NODES * sizeof(struct node_info));
}

/**
 * @brief Initialize the communicator
 * 
 */
void _torc_comm_init()
{
    MPI_Request request;

    int workers[MAX_NODES];
    int workers_me = kthreads;

    enter_comm_cs();
    MPI_Iallgather(&workers_me, 1, MPI_INT, workers, 1, MPI_INT, comm_out, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
    MPI_Barrier(comm_out);
    leave_comm_cs();

    //! Workers have been started. The node_info array must be combined by all nodes

    for (int i = 0; i < torc_num_nodes(); i++)
    {
        node_info[i].nworkers = workers[i]; /* SMP */
    }

    //! Synchronize execution of workers
    enter_comm_cs();
    MPI_Barrier(comm_out);
    leave_comm_cs();

#if DEBUG
    printf("[%d/%d] Node is up\n", torc_node_id(), torc_num_nodes());
    fflush(0);
#endif
}

/******   EXPICLIT COMMUNICATION FOR DESCRIPTORS (SEND, RECEIVE)    ******/

static int _torc_thread_id()
{ //! Check if the identifier of the current thread is server_thread
    if (pthread_equal(pthread_self(), server_thread))
    {
        return MAX_NVPS;
    }
    else
    {
        return _torc_get_vpid();
    }
}

/**
 * @brief Sending arguments (nonblocking send)
 * 
 * @param node Rank of destination node
 * @param tag  Message tag 
 * @param desc TORC descriptor
 */
void send_arguments(int node, int tag, torc_t *desc)
{
    MPI_Request *requests = (MPI_Request *)malloc(desc->narg * sizeof(MPI_Request));
    int nrequests = -1;

    if (!thread_safe)
    {
        for (int i = 0; i < desc->narg; i++)
        {
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            // By copy || By address
            if ((desc->callway[i] == CALL_BY_COP) || (desc->callway[i] == CALL_BY_VAD))
            {
                /* do not send anything - the value is in the descriptor */
                if (desc->quantity[i] == 1)
                {
                    continue;
                }

                pthread_mutex_lock(&comm_m);
                nrequests++;
                if (desc->homenode != desc->sourcenode)
                {
                    MPI_Isend(&desc->temparg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                else
                {
                    MPI_Isend(&desc->localarg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                pthread_mutex_unlock(&comm_m);
            }
            // By reference || By value || By copy
            else if ((desc->callway[i] == CALL_BY_REF) || (desc->callway[i] == CALL_BY_PTR) || (desc->callway[i] == CALL_BY_COP2))
            {
                pthread_mutex_lock(&comm_m);
                nrequests++;
                if (desc->homenode != desc->sourcenode)
                {
                    MPI_Isend((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                else
                {
                    MPI_Isend((void *)desc->localarg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                pthread_mutex_unlock(&comm_m);
            }
            //! CALL_BY_RES, do not send anything
        }

        if (nrequests > -1)
        {
            pthread_mutex_lock(&comm_m);
            MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
            pthread_mutex_unlock(&comm_m);
        }
    }
    else
    {
        for (int i = 0; i < desc->narg; i++)
        {
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            // By copy || By address
            if ((desc->callway[i] == CALL_BY_COP) || (desc->callway[i] == CALL_BY_VAD))
            {
                /* do not send anything - the value is in the descriptor */
                if (desc->quantity[i] == 1)
                {
                    continue;
                }
                {
                    pthread_mutex_lock(&comm_m);
                    nrequests++;
                    pthread_mutex_unlock(&comm_m);
                }
                if (desc->homenode != desc->sourcenode)
                {
                    MPI_Isend(&desc->temparg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                else
                {
                    MPI_Isend(&desc->localarg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
            }
            // By reference || By value || By copy
            else if ((desc->callway[i] == CALL_BY_REF) || (desc->callway[i] == CALL_BY_PTR) || (desc->callway[i] == CALL_BY_COP2))
            {
                {
                    pthread_mutex_lock(&comm_m);
                    nrequests++;
                    pthread_mutex_unlock(&comm_m);
                }
                if (desc->homenode != desc->sourcenode)
                {
                    MPI_Isend((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
                else
                {
                    MPI_Isend((void *)desc->localarg[i], desc->quantity[i], desc->dtype[i], node, tag, comm_out, &requests[nrequests]);
                }
            }
            //! CALL_BY_RES, do not send anything
        }

        if (nrequests > -1)
        {
            MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
        }
    }

    free(requests);
}

/**
 * @brief Sending descriptor (nonblocking send)
 * Send a descriptor to the target node - always to a server thread
 * 
 * @param node Rank of destination node 
 * @param desc TORC descriptor 
 * @param type Request type, which is one of 
 *      \b DIRECT_SYNCHRONOUS_STEALING_REQUEST
 *      \b TORC_BCAST
 *      \b TORC_ANSWER
 *      \b TORC_NORMAL_ENQUEUE
 */
void send_descriptor(int node, torc_t *desc, int type)
{
    int const tag = _torc_thread_id();

#if DEBUG
    printf("[%d] - sending to node [%d] desc -> homenode [%d], type = %d\n", torc_node_id(), node, desc->homenode, type);
#endif

    desc->sourcenode = torc_node_id();
    //! who sends this
    desc->sourcevpid = tag;
    desc->type = type;

    {
        MPI_Request request;

        enter_comm_cs();
        MPI_Isend(desc, torc_desc_size, MPI_CHAR, node, MAX_NVPS, comm_out, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        leave_comm_cs();
    }

    if (desc->type == DIRECT_SYNCHRONOUS_STEALING_REQUEST)
    {
        return;
    }
    else if (desc->type == TORC_BCAST)
    {
        return;
    }
    else if (desc->type == TORC_ANSWER)
    {
        MPI_Request *requests = (MPI_Request *)malloc(desc->narg * sizeof(MPI_Request));
        int nrequests = -1;

        if (!thread_safe)
        {
            /* in case of call by reference send the data back */
            for (int i = 0; i < desc->narg; i++)
            {
                if (desc->quantity[i] == 0)
                {
                    continue;
                }

                if ((desc->callway[i] == CALL_BY_COP2) && (desc->quantity[i] > 1))
                {
                    free((void *)desc->temparg[i]);
                }

                //! send the result back
                if ((desc->callway[i] == CALL_BY_REF) || (desc->callway[i] == CALL_BY_RES))
                {
                    pthread_mutex_lock(&comm_m);
                    nrequests++;
                    MPI_Isend((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], desc->homenode, tag, comm_out, &requests[nrequests]);
                    pthread_mutex_unlock(&comm_m);

                    if (desc->quantity[i] > 1)
                    {
                        free((void *)desc->temparg[i]);
                    }
                }
            }

            if (nrequests > -1)
            {
                pthread_mutex_lock(&comm_m);
                MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
                pthread_mutex_unlock(&comm_m);
            }
        }
        else
        {
            /* in case of call by reference send the data back */
            for (int i = 0; i < desc->narg; i++)
            {
                if (desc->quantity[i] == 0)
                {
                    continue;
                }

                if ((desc->callway[i] == CALL_BY_COP2) && (desc->quantity[i] > 1))
                {
                    free((void *)desc->temparg[i]);
                }

                //! send the result back
                if ((desc->callway[i] == CALL_BY_REF) || (desc->callway[i] == CALL_BY_RES))
                {
                    {
                        pthread_mutex_lock(&comm_m);
                        nrequests++;
                        pthread_mutex_unlock(&comm_m);
                    }

                    MPI_Isend((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], desc->homenode, tag, comm_out, &requests[nrequests]);

                    if (desc->quantity[i] > 1)
                    {
                        free((void *)desc->temparg[i]);
                    }
                }
            }

            if (nrequests > -1)
            {
                MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
            }
        }

        free(requests);

        return;
    }
    //! TORC_NORMAL_ENQUEUE
    else
    {
        if (desc->homenode == node)
        {
            return;
        }

        send_arguments(node, tag, desc);
        return;
    }
}

void direct_send_descriptor(int dummy, int sourcenode, int sourcevpid, torc_t *desc)
{
    MPI_Request request;

    desc->sourcenode = torc_node_id();

    //! the server thread responds to a stealing request from a worker
    desc->sourcevpid = MAX_NVPS;

    //! response to server's thread direct stealing request
    if (sourcevpid == MAX_NVPS)
    {
        sourcevpid = MAX_NVPS + 1;
    }

    int const tag = sourcevpid + 100;

    enter_comm_cs();
    //! if sourcevpid == MAX_NVPS
    MPI_Isend(desc, torc_desc_size, MPI_CHAR, sourcenode, tag, comm_out, &request);
    MPI_Wait(&request, MPI_STATUS_IGNORE);
    leave_comm_cs();

    if (desc->homenode == sourcenode)
    {
        return;
    }

    send_arguments(sourcenode, tag, desc);
}

void receive_arguments(torc_t *desc, int tag)
{
    MPI_Request *requests = (MPI_Request *)malloc(desc->narg * sizeof(MPI_Request));
    int nrequests = -1;

    if (!thread_safe)
    {
        for (int i = 0; i < desc->narg; i++)
        {
#if DEBUG
            printf("reading arg %d (%d - %d)\n", i, desc->quantity[i], desc->callway[i]);
            fflush(0);
#endif
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            if ((desc->quantity[i] > 1) || ((desc->callway[i] != CALL_BY_COP) && (desc->callway[i] != CALL_BY_VAD)))
            {

                desc->dtype[i] = _torc_b2mpi_type(desc->btype[i]);

                int typesize;
                MPI_Type_size(desc->dtype[i], &typesize);

                char *mem = (char *)calloc(1, desc->quantity[i] * typesize);

                desc->temparg[i] = (INT64)mem;

                //! All the types except by result
                if (desc->callway[i] != CALL_BY_RES)
                {
                    pthread_mutex_lock(&comm_m);
                    nrequests++;
                    MPI_Irecv((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], desc->sourcenode, tag, comm_out, &requests[nrequests]);
                    pthread_mutex_unlock(&comm_m);
                }
            }
            else
            {
                desc->temparg[i] = desc->localarg[i];
            }

#if DEBUG
            printf("read+++ arg %d (%d - %d)\n", i, desc->quantity[i], desc->callway[i]);
            fflush(0);
#endif
        }

        if (nrequests > -1)
        {
            pthread_mutex_lock(&comm_m);
            MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
            pthread_mutex_unlock(&comm_m);
        }
    }
    else
    {
        for (int i = 0; i < desc->narg; i++)
        {
#if DEBUG
            printf("reading arg %d (%d - %d)\n", i, desc->quantity[i], desc->callway[i]);
            fflush(0);
#endif
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            if ((desc->quantity[i] > 1) || ((desc->callway[i] != CALL_BY_COP) && (desc->callway[i] != CALL_BY_VAD)))
            {

                desc->dtype[i] = _torc_b2mpi_type(desc->btype[i]);

                int typesize;
                MPI_Type_size(desc->dtype[i], &typesize);

                char *mem = (char *)calloc(1, desc->quantity[i] * typesize);

                desc->temparg[i] = (INT64)mem;

                //! All the types except by result
                if (desc->callway[i] != CALL_BY_RES)
                {
                    {
                        pthread_mutex_lock(&comm_m);
                        nrequests++;
                        pthread_mutex_unlock(&comm_m);
                    }

                    MPI_Irecv((void *)desc->temparg[i], desc->quantity[i], desc->dtype[i], desc->sourcenode, tag, comm_out, &requests[nrequests]);
                }
            }
            else
            {
                desc->temparg[i] = desc->localarg[i];
            }

#if DEBUG
            printf("read+++ arg %d (%d - %d)\n", i, desc->quantity[i], desc->callway[i]);
            fflush(0);
#endif
        }

        if (nrequests > -1)
        {
            MPI_Waitall(nrequests + 1, requests, MPI_STATUSES_IGNORE);
        }
    }

    free(requests);
}

int receive_descriptor(int node, torc_t *rte)
{
    MPI_Request request;

    int istat;
    int const tag = _torc_thread_id() + 100;

    if (thread_safe)
    {
        istat = MPI_Irecv(rte, torc_desc_size, MPI_CHAR, node, tag, comm_out, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
    }
    else
    {
        /* irecv for non thread-safe MPI libraries */
        int flag = 0;

        enter_comm_cs();
        istat = MPI_Irecv(rte, torc_desc_size, MPI_CHAR, node, tag, comm_out, &request);
        leave_comm_cs();

        while (1)
        {
            if (appl_finished == 1)
            {
                rte->type = TORC_NO_WORK;
                return 1;
            }

            enter_comm_cs();
            istat = MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
            leave_comm_cs();

            if (flag == 1)
            {
                break;
            }
            else
            {
                thread_sleep(yieldtime);
            }
        }
    }

    if (istat != MPI_SUCCESS)
    {
        rte->type = TORC_NO_WORK;
        return 1;
    }

    if (rte->type == TORC_NO_WORK)
    {
        return 1;
    }

    //! homenode == torc_node_id() -> the descriptor is stolen by its owner node
    if (rte->homenode != torc_node_id())
    {
        receive_arguments(rte, tag);
    }

    return 0;
}

/**************************  THREAD MANAGEMENT  **************************/

/**
 * @brief 
 * 
 * @param node_id 
 * @return int 
 */
static int _torc_num_threads(int node_id)
{
    //! kthreads
    return node_info[node_id].nworkers;
}

int _torc_total_num_threads()
{
    int sum_vp = 0;
    for (int i = 0; i < torc_num_nodes(); i++)
    {
        sum_vp += _torc_num_threads(i);
    }
    return sum_vp;
}

int global_thread_id_to_node_id(int global_thread_id)
{
#if DEBUG
    printf("global_thread_id = %d\n", global_thread_id);
#endif

    int sum_vp = 0;
    for (int i = 0; i < torc_num_nodes(); i++)
    {
        sum_vp += _torc_num_threads(i);
        if (global_thread_id < sum_vp)
            return i;
    }

    //! never reached
    Error("target_to_node failed");
    return -1;
}

int local_thread_id_to_global_thread_id(int local_thread_id)
{
    int sum_vp = 0;
    for (int i = 0; i < torc_node_id(); i++)
    {
        sum_vp += _torc_num_threads(i);
    }
    return sum_vp + local_thread_id;
}

int global_thread_id_to_local_thread_id(int global_thread_id)
{
    int mynode = global_thread_id_to_node_id(global_thread_id);

    int sum_vp = 0;
    for (int i = 0; i < mynode; i++)
    {
        sum_vp += _torc_num_threads(i);
    }
    return global_thread_id - sum_vp;
}

/**************************    BROADCASTING     **************************/

void torc_broadcast(void *a, long count, MPI_Datatype datatype)
{
    long mynode = torc_node_id();

    torc_t mydata;
    int nnodes = torc_num_nodes();
    int tag = _torc_thread_id();

#if DEBUG
    printf("Broadcasting data ...\n");
    fflush(0);
#endif

    memset(&mydata, 0, sizeof(torc_t));

    mydata.localarg[0] = (INT64)mynode;
    mydata.localarg[1] = (INT64)a;
    mydata.localarg[2] = (INT64)count;
    mydata.localarg[3] = (INT64)_torc_mpi2b_type(datatype);

    mydata.homenode = mydata.sourcenode = mynode;

    for (int node = 0; node < nnodes; node++)
    {
        if (node != mynode)
        {
            /* OK. This descriptor is a stack variable */
            send_descriptor(node, &mydata, TORC_BCAST);

            enter_comm_cs();
            MPI_Ssend(a, count, datatype, node, tag, comm_out);
            leave_comm_cs();
        }
    }
}

int _torc_mpi2b_type(MPI_Datatype dtype)
{
    if (dtype == MPI_CHAR)
        return T_MPI_CHAR;
    else if (dtype == MPI_INT)
        return T_MPI_INT;
    else if (dtype == MPI_LONG)
        return T_MPI_LONG;
    else if (dtype == MPI_LONG_LONG)
        return T_MPI_LONG_LONG;
    else if (dtype == MPI_UNSIGNED)
        return T_MPI_UNSIGNED;
    else if (dtype == MPI_UNSIGNED_LONG_LONG)
        return T_MPI_UNSIGNED_LONG_LONG;
    else if (dtype == MPI_FLOAT)
        return T_MPI_FLOAT;
    else if (dtype == MPI_DOUBLE)
        return T_MPI_DOUBLE;
    else if (dtype == MPI_DOUBLE_PRECISION)
        return T_MPI_DOUBLE_PRECISION;
    else if (dtype == MPI_INTEGER)
        return T_MPI_INTEGER;
    else
        Error("unsupported MPI data type");

    //! never reached
    return 0;
}

MPI_Datatype _torc_b2mpi_type(int btype)
{
    switch (btype)
    {
    case T_MPI_CHAR:
        return MPI_CHAR;
        break;
    case T_MPI_INT:
        return MPI_INT;
        break;
    case T_MPI_LONG:
        return MPI_LONG;
        break;
    case T_MPI_LONG_LONG:
        return MPI_LONG_LONG;
        break;
    case T_MPI_UNSIGNED:
        return MPI_UNSIGNED;
        break;
    case T_MPI_UNSIGNED_LONG_LONG:
        return MPI_UNSIGNED_LONG_LONG;
        break;
    case T_MPI_FLOAT:
        return MPI_FLOAT;
        break;
    case T_MPI_DOUBLE:
        return MPI_DOUBLE;
        break;
    case T_MPI_DOUBLE_PRECISION:
        return MPI_DOUBLE_PRECISION;
        break;
    case T_MPI_INTEGER:
        return MPI_INTEGER;
        break;
    default:
        printf("btype = %d\n", btype);
        Error("unsupported MPI datatype");
        break;
    }

    //! never reached
    return 0;
}

#define f77fun 1
#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
void F77_FUNC_(torc_register_task, TORC_REGISTER_TASK)(void *f)
{
    torc_register_task(f);
}
#endif
#undef f77fun
