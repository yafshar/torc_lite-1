/*
 *  torc_server.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

static torc_t no_work_desc;

//! Server thread mutex object 
static pthread_mutex_t server_thread_m = PTHREAD_MUTEX_INITIALIZER;

//! Internode mutex
pthread_mutex_t internode_m = PTHREAD_MUTEX_INITIALIZER;

//! Voletile flag to indicate the termination
volatile int termination_flag = 0;

//! Indicator if the server is still alive
static int server_thread_alive = 0;

int process_a_received_descriptor(torc_t *desc)
{
    MPI_Request request;

    desc->next = NULL;

#if DEBUG
    if (torc_node_id() == desc->sourcenode)
    {
        printf("Node id %d = sourcenode %d\n", torc_node_id(), desc->sourcenode);
    }
#endif

    int tag = desc->sourcevpid;

    if ((tag < 0) || (tag > MAX_NVPS))
    {
        printf("...Invalid message tag %d from node %d [type=%d]\n", tag, desc->sourcenode, desc->type);
        fflush(0);

        MPI_Abort(comm_out, 1);
        return 1;
    }

    switch (desc->type)
    {
    case TORC_ANSWER:
    {
#if DEBUG
        printf("Server %d accepted from %d, narg = %d [ANSWER]\n", torc_node_id(), desc->sourcenode, desc->narg);
        fflush(stdout);
#endif
        //! receive the results, if any
        for (int i = 0; i < desc->narg; i++)
        {
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            if ((desc->callway[i] == CALL_BY_RES) || (desc->callway[i] == CALL_BY_REF))
            {
                enter_comm_cs();
                desc->dtype[i] = _torc_b2mpi_type(desc->btype[i]);
                MPI_Irecv((void *)desc->localarg[i], desc->quantity[i], desc->dtype[i], desc->sourcenode, tag, comm_out, &request);
                MPI_Wait(&request, MPI_STATUS_IGNORE);
                leave_comm_cs();
            }
#if DEBUG
            printf("received data from %d\n", desc->sourcenode);
            fflush(0);
#endif
        }

        for (int i = 0; i < desc->narg; i++)
        {
            if (desc->quantity[i] == 0)
            {
                continue;
            }

            if (desc->callway[i] == CALL_BY_COP2)
            {
                free((void *)desc->localarg[i]);

                desc->localarg[i] = 0;
            }
        }

        if (desc->parent)
        {
            _torc_depsatisfy(desc->parent);
        }

        return 1;
    }
    break;

    case TORC_NORMAL_ENQUEUE:
    {
        if (desc->homenode == torc_node_id())
        {
            torc_to_i_rq(desc);

            return 0;
        }
        else
        {
#if DEBUG
            printf("Server %d accepted from %d, narg = %d [WORK]\n", torc_node_id(), desc->sourcenode, desc->narg);
            fflush(stdout);
#endif
            receive_arguments(desc, tag);

            //! direct execution
            if (desc->rte_type == 20)
            {
                _torc_core_execution(desc);

                send_descriptor(desc->homenode, desc, TORC_ANSWER);

                return 0;
            }

#if DEBUG
            printf("SERVER: ENQ : Queue = %d Private = %d Front = %d\n", desc->target_queue, desc->insert_private, desc->insert_in_front);
#endif

            //! 1: per node - 2 : per virtual processor
            if (desc->insert_private)
            {
                if (desc->insert_in_front)
                {
                    torc_to_i_pq(desc);
                }
                else
                {
                    torc_to_i_pq_end(desc);
                }
            }
            //! 0: public queues
            else
            {
                if (desc->insert_in_front)
                {
                    torc_to_i_rq(desc);
                }
                else
                {
                    torc_to_i_rq_end(desc);
                }
            }

            return 0;
        }
    }
    break;

    case DIRECT_SYNCHRONOUS_STEALING_REQUEST:
    {
#if DEBUG
        printf("Server %d received request for synchronous stealing\n", torc_node_id());
        fflush(0);
#endif
        steal_attempts++;

        torc_t *stolen_work = torc_i_rq_dequeue(0);

        for (int i = 1; i < 10; i++)
        {
            if (stolen_work == NULL)
                stolen_work = torc_i_rq_dequeue(i);
            else
                break;
        }

        if (stolen_work != NULL)
        {
            direct_send_descriptor(DIRECT_SYNCHRONOUS_STEALING_REQUEST, desc->sourcenode, desc->sourcevpid, stolen_work);
            steal_served++;
        }
        else
        {
            direct_send_descriptor(DIRECT_SYNCHRONOUS_STEALING_REQUEST, desc->sourcenode, desc->sourcevpid, &no_work_desc);
        }
        return 0;
    }
    break;

    case TERMINATE_LOCAL_SERVER_THREAD:
    {
        pthread_exit(0);
        return 1;
    }
    break;

    case TERMINATE_WORKER_THREADS:
    {
        termination_flag = 1;

        if (desc->localarg[0] != torc_node_id())
        {
            appl_finished++;
        }
#if DEBUG
        printf("Server %d will exit\n", torc_node_id());
        fflush(0);
#endif
        return 1;
    }
    break;

    case ENABLE_INTERNODE_STEALING:
    {
        internode_stealing = 1;

        return 1;
    }
    break;

    case DISABLE_INTERNODE_STEALING:
    {
        internode_stealing = 0;
        return 1;
    }
    break;

    case RESET_STATISTICS:
    {
        torc_reset_statistics();
        return 1;
    }
    break;

    case TORC_BCAST:
    {
        void *va = (void *)desc->localarg[1];

        int count = desc->localarg[2];

        MPI_Datatype dtype = _torc_b2mpi_type(desc->localarg[3]);

        printf("TORC_BCAST: %p %d\n", va, count);
        fflush(0);

        enter_comm_cs();
        MPI_Irecv((void *)va, count, dtype, desc->sourcenode, tag, comm_out, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        leave_comm_cs();

        return 1;
    }
    break;

    default:
        Error1("Unkown descriptor type on node %d", torc_node_id());
        break;
    }

    return 1;
}

/**
 * @brief server thread loop routine with arg as its sole argument
 * 
 * @param arg 
 * @return void* 
 */
void *server_loop(void *arg)
{
    torc_t *desc;
    int reuse = 0;

    MPI_Request request;

#if DEBUG
    printf("Server %d begins....\n", torc_node_id());
    fflush(stdout);
#endif

    memset(&no_work_desc, 0, sizeof(torc_t));

    no_work_desc.type = TORC_NO_WORK;

    while (1)
    {
        if (!reuse)
        {
            desc = _torc_get_reused_desc();
        }

        memset(desc, 0, sizeof(torc_t)); /* lock ..?*/

#if DEBUG
        printf("Server %d waits for a descriptor ....\n", torc_node_id());
        fflush(0);
#endif

        if (thread_safe)
        {
            MPI_Irecv(desc, torc_desc_size, MPI_CHAR, MPI_ANY_SOURCE, MAX_NVPS, comm_out, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
        }
        else
        {
            if (termination_flag >= 1)
            {
                pthread_exit(0);
            }

            enter_comm_cs();
            MPI_Irecv(desc, torc_desc_size, MPI_CHAR, MPI_ANY_SOURCE, MAX_NVPS, comm_out, &request);
            leave_comm_cs();

            int flag = 0;
            while (1)
            {
                if (termination_flag >= 1)
                {
                    printf("server threads exits!\n");
                    fflush(0);
                    pthread_exit(0);
                }

                enter_comm_cs();
                MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
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

        reuse = process_a_received_descriptor(desc);
        if (reuse)
        {
            _torc_put_reused_desc(desc);
            reuse = 0;
        }
    }

    return 0;
}

/**
 * @brief If number of nodes > 1 then TORC starts a server thread
 * 
 */
void start_server_thread()
{
    //! Thread attribute
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    {
        pthread_mutex_lock(&server_thread_m);

        if (server_thread_alive)
        {
            pthread_mutex_unlock(&server_thread_m);
            return;
        }

        //! The thread is created executing server_loop with NULL as its sole argument.
        int res = pthread_create(&server_thread, &attr, server_loop, NULL);
        if (res != 0)
        {
            Error("server thread was not created!\n");
        }

        server_thread_alive = 1;

        pthread_mutex_unlock(&server_thread_m);
    }
}

void shutdown_server_thread()
{
    static torc_t mydata;

#if DEBUG
    printf("[%d]: Terminating local server thread....\n", torc_node_id());
    fflush(0);
#endif

    {
        pthread_mutex_lock(&server_thread_m);

        if (server_thread_alive == 0)
        {
            pthread_mutex_unlock(&server_thread_m);
            return;
        }

        memset(&mydata, 0, sizeof(mydata));

        if (thread_safe)
        {
            send_descriptor(torc_node_id(), &mydata, TERMINATE_LOCAL_SERVER_THREAD);
        }
        else
        {
            termination_flag = 1;
        }

        pthread_join(server_thread, NULL);

        server_thread_alive = 0;

        pthread_mutex_unlock(&server_thread_m);
    }
}

/**********************     THREAD MANAGEMENT      **********************/
void terminate_workers()
{
#if DEBUG
    printf("Terminating worker threads ...\n");
#endif

    torc_t mydata;
    memset(&mydata, 0, sizeof(torc_t));

    int const mynode = torc_node_id();

    mydata.localarg[0] = mynode;
    mydata.homenode = mynode;

    for (int node = 0; node < torc_num_nodes(); node++)
    {
        if (node != mynode)
        {
            send_descriptor(node, &mydata, TERMINATE_WORKER_THREADS);
        }
    }
}

/**********************     INTERNODE STEALING      **********************/
torc_t *direct_synchronous_stealing_request(int target_node)
{
    if (termination_flag)
    {
        return NULL;
    }

    torc_t *desc;

    {
        int vp = target_node;

        pthread_mutex_lock(&internode_m);

        desc = _torc_get_reused_desc();

#if DEBUG
        printf("[%d] Synchronous stealing request ...\n", torc_node_id());
        fflush(0);
#endif

        torc_t mydata;
        memset(&mydata, 0, sizeof(mydata));

        mydata.localarg[0] = torc_node_id();
        mydata.homenode = torc_node_id();

        send_descriptor(vp, &mydata, DIRECT_SYNCHRONOUS_STEALING_REQUEST);

        receive_descriptor(vp, desc);

        desc->next = NULL;

        pthread_mutex_unlock(&internode_m);
    }

    if (desc->type == TORC_NO_WORK)
    {
        usleep(100 * 1000);

        _torc_put_reused_desc(desc);

        return NULL;
    }

    steal_hits++;

    return desc;
}

void torc_disable_stealing()
{
#if DEBUG
    printf("Disabling internode stealing ...\n");
    fflush(0);
#endif

    internode_stealing = 0;

    torc_t mydata;
    memset(&mydata, 0, sizeof(torc_t));

    int const mynode = torc_node_id();

    mydata.localarg[0] = (INT64)mynode;
    mydata.homenode = mydata.sourcenode = mynode;

    for (int node = 0; node < torc_num_nodes(); node++)
    {
        if (node != mynode)
        {
            //! OK. This descriptor is a stack variable
            send_descriptor(node, &mydata, DISABLE_INTERNODE_STEALING);
        }
    }
}

void torc_enable_stealing()
{
#if DEBUG
    printf("Enabling internode stealing ...\n");
    fflush(0);
#endif

    internode_stealing = 1;

    torc_t mydata;
    memset(&mydata, 0, sizeof(torc_t));

    int const mynode = torc_node_id();

    mydata.localarg[0] = (INT64)mynode;
    mydata.homenode = mydata.sourcenode = mynode;

    for (int node = 0; node < torc_num_nodes(); node++)
    {
        if (node != mynode)
        {
            //! OK. This descriptor is a stack variable
            send_descriptor(node, &mydata, ENABLE_INTERNODE_STEALING);
        }
    }
}

void torc_i_enable_stealing()
{
    internode_stealing = 1;
}

void torc_i_disable_stealing()
{
    internode_stealing = 0;
}

/**
 * @brief reset the statistics
 * 
 */
void torc_reset_statistics()
{
#if DEBUG
    printf("Reseting statistics ...\n");
#endif

    torc_t mydata;
    memset(&mydata, 0, sizeof(torc_t));

    int const mynode = torc_node_id();

    for (int node = 0; node < torc_num_nodes(); node++)
    {
        if (node != mynode)
        {
            //!  OK. This descriptor is a stack variable
            send_descriptor(node, &mydata, RESET_STATISTICS);
        }
        else
        {
            _torc_reset_statistics();
        }
    }
}
