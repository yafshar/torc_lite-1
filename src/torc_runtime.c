/*
 *  torc_runtime.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

#define WAIT_COUNT 2

#define A0
#define A1 args[0]
#define A2 A1, args[1]
#define A3 A2, args[2]
#define A4 A3, args[3]
#define A5 A4, args[4]
#define A6 A5, args[5]
#define A7 A6, args[6]
#define A8 A7, args[7]
#define A9 A8, args[8]
#define A10 A9, args[9]
#define A11 A10, args[10]
#define A12 A11, args[11]
#define A13 A12, args[12]
#define A14 A13, args[13]
#define A15 A14, args[14]
#define A16 A15, args[15]
#define A17 A16, args[16]
#define A18 A17, args[18]
#define A19 A15, args[19]
#define A20 A16, args[20]
#define A21 A17, args[21]
#define A22 A16, args[22]
#define A23 A17, args[23]

#define DO_CASE(x)        \
    case x:               \
    {                     \
        desc->work(A##x); \
        break;            \
    }

//! MPI communicator
extern MPI_Comm comm_out;

void _torc_core_execution(torc_t *desc)
{
    VIRT_ADDR args[MAX_TORC_ARGS];
    int i;

    if (desc->work_id >= 0)
        desc->work = getfuncptr((long)desc->work_id);

    if (torc_node_id() == desc->homenode)
    {
        for (i = 0; i < desc->narg; i++)
        {
            if (desc->callway[i] == CALL_BY_COP)
            {
                args[i] = (VIRT_ADDR)&desc->localarg[i]; /* pointer to the private copy */
            }
            else
            {
                args[i] = desc->localarg[i];
            }
        }
    }
    else
    {
        for (i = 0; i < desc->narg; i++)
        {
            if (desc->callway[i] == CALL_BY_COP)
            {
                args[i] = (VIRT_ADDR)&desc->temparg[i];
            }
            else
            {
                args[i] = desc->temparg[i];
            }
        }
    }

    switch (desc->narg)
    {
        DO_CASE(0);
        DO_CASE(1);
        DO_CASE(2);
        DO_CASE(3);
        DO_CASE(4);
        DO_CASE(5);
        DO_CASE(6);
        DO_CASE(7);
        DO_CASE(8);
        DO_CASE(9);
        DO_CASE(10);
        DO_CASE(11);
        DO_CASE(12);
        DO_CASE(13);
        DO_CASE(14);
        DO_CASE(15);
        DO_CASE(16);
        DO_CASE(17);
        DO_CASE(18);
        DO_CASE(19);
        DO_CASE(20);
        DO_CASE(21);
        DO_CASE(22);
        DO_CASE(23);
    default:
        Error("Function with more than 24 arguments..!");
        break;
    }
}

void _torc_reset_statistics()
{
    memset(created, 0, MAX_NVPS * sizeof(unsigned long));
    memset(executed, 0, MAX_NVPS * sizeof(unsigned long));
}

void _torc_print_statistics()
{
    unsigned long total_created = 0;
    unsigned long total_executed = 0;

    /* Runtime statistics */
    for (unsigned int i = 0; i < kthreads; i++)
    {
        total_created += created[i];
        total_executed += executed[i];
    }

    printf("[%2d] steals served/attempts/hits = %-3ld/%-3ld/%-3ld created = %3ld, executed = %3ld:(", torc_node_id(),
           steal_served, steal_attempts, steal_hits, total_created, total_executed);

    for (unsigned int i = 0; i < kthreads - 1; i++)
    {
        printf("%3ld,", executed[i]);
    }
    printf("%3ld)\n", executed[kthreads - 1]);
    fflush(0);
}

void _torc_stats(void)
{
#if defined(TORC_STATS)
    _torc_print_statistics();
#endif
}

torc_t *_torc_self()
{
    return (torc_t *)_torc_get_currt();
}

void _torc_depadd(torc_t *desc, int ndeps)
{
    _lock_acquire(&desc->lock);
    desc->ndep += ndeps;
    _lock_release(&desc->lock);
}

static void _torc_end(void)
{
    int finalized = 0;

    MPI_Finalized(&finalized);
    if (finalized == 1)
    {
        _torc_stats();
        _exit(0);
    }

    appl_finished = 1;

    //! notify the rest of the nodes
    if (torc_num_nodes() > 1)
    {
        terminate_workers();
    }

    _torc_md_end();
}

void torc_finalize() { _torc_end(); }

void _torc_execute(void *arg)
{
    torc_t *me = _torc_self();
    torc_t *desc = (torc_t *)arg;

    long vp = me->vp_id;

    desc->vp_id = vp;

#ifdef TORC_STATS
    if (desc->rte_type == 1)
    {
        executed[vp]++;
    }
#endif

    _torc_set_currt(desc);
    _torc_core_execution(desc);
    _torc_cleanup(desc);

    _torc_set_currt(me);
}

int _torc_block(void)
{
    torc_t *desc = _torc_self();

    _lock_acquire(&desc->lock);
    --desc->ndep;
    if (desc->ndep < 0)
    {
        desc->ndep = 0;
    }
    _lock_release(&desc->lock);

    while (1)
    {
        _lock_acquire(&desc->lock);
        int const remdeps = desc->ndep;
        if (remdeps <= 0)
        {
            _lock_release(&desc->lock);
            return 1;
        }
        _lock_release(&desc->lock);

        _torc_scheduler_loop(1);
    }

    return 0;
}

/* Q: What did I do here? - A: Block until no more work exists at the cluster-layer. Useful for SPMD-like barriers */
int _torc_block2(void)
{
    torc_t *desc = _torc_self();

    _lock_acquire(&desc->lock);
    --desc->ndep;
    if (desc->ndep < 0)
    {
        desc->ndep = 0;
    }
    _lock_release(&desc->lock);

    while (1)
    {
        if (desc->ndep > 0)
        {
            _lock_acquire(&desc->lock);
            int const remdeps = desc->ndep;
            if (remdeps <= 0)
            {
                desc->ndep = 0;
                _lock_release(&desc->lock);
                return 1;
            }
            _lock_release(&desc->lock);
        }

        int work = _torc_scheduler_loop(1);
        if ((desc->ndep == 0) && (!work))
        {
            return 0;
        }
    }

    return 0;
}

void _torc_set_work_routine(torc_t *desc, void (*work)())
{
    desc->work = work;

    desc->work_id = getfuncnum(work);

    if (-1 == desc->work_id)
    {
        printf("Internode function %p not registered\n", work);
    }
}

int _torc_depsatisfy(torc_t *desc)
{
    int deps;

    _lock_acquire(&desc->lock);
    deps = --desc->ndep;
    _lock_release(&desc->lock);

    return !deps;
}

/**
 * @brief 
 * This is the new interface which would take the communicator
 * 
 * @param argc 
 * @param argv 
 * @param comm_in  Input communicator (default value is MPI_COMM_WORLD)
 */
void __torc_opt(int argc, char *argv[], MPI_Comm comm_in)
{
    char **largv = argv;

    //! in case argv cannot be NULL (HPMPI)
    if (argc == 0)
    {
        char *llargv = "";
        largv = (char **)&llargv;
    }

    {
        int initialized;
        int provided;

        MPI_Initialized(&initialized);

        if (initialized)
        {
            MPI_Query_thread(&provided);
        }
        else
        {
            int largc = argc;
            /* If the process is multithreaded, multiple threads may call MPI at once with no restrictions. */
            MPI_Init_thread(&largc, &largv, MPI_THREAD_MULTIPLE, &provided);
        }

        thread_safe = provided == MPI_THREAD_MULTIPLE;
    }

    kthreads = TORC_DEF_CPUS;

    {
        int val;

        char *s = (char *)getenv("OMP_NUM_THREADS");
        if (s != 0 && sscanf(s, "%d", &val) == 1 && val > 0)
        {
            kthreads = val;
        }

        s = (char *)getenv("TORC_WORKERS");
        if (s != 0 && sscanf(s, "%d", &val) == 1 && val > 0)
        {
            kthreads = val;
        }

        yieldtime = TORC_DEF_YIELDTIME;
        s = (char *)getenv("TORC_YIELDTIME");
        if (s != 0 && sscanf(s, "%d", &val) == 1 && val > 0)
        {
            yieldtime = val;
        }

        throttling_factor = -1;
        s = (char *)getenv("TORC_THROTTLING_FACTOR");
        if (s != 0 && sscanf(s, "%d", &val) == 1 && val > 0)
        {
            throttling_factor = val;
        }
    }

    MPI_Comm_rank(comm_in, &mpi_rank);
    MPI_Comm_size(comm_in, &mpi_nodes);

    MPI_Barrier(comm_in);

    {
        int namelen;
        char name[MPI_MAX_PROCESSOR_NAME];

        MPI_Get_processor_name(name, &namelen);
        printf("TORC_LITE ... rank %d of %d on host %s\n", mpi_rank, mpi_nodes, name);
    }

    if (mpi_rank == 0)
    {
        printf("The MPI implementation IS%s thread safe!\n", (thread_safe) ? "" : " NOT");
    }

    MPI_Comm_dup(comm_in, &comm_out);
    MPI_Barrier(comm_out);

    _torc_comm_pre_init();
}

void _torc_opt(int argc, char *argv[])
{
    __torc_opt(argc, argv, MPI_COMM_WORLD);
}

/**
 * @brief Package initialization
 * 
 */
void _torc_env_init(void)
{
    //! Initialization of ready queues
    rq_init();

    //! Initialize workers
    _torc_md_init();

    //! Initialize the communicator
    _torc_comm_init();
}

torc_t *get_next_task()
{
    torc_t *desc_next = torc_i_pq_dequeue();

    if (desc_next == NULL)
    {
        for (int i = 9; i >= 0; i--)
        {
            if (desc_next == NULL)
            {
                desc_next = torc_i_rq_dequeue(i);
            }
            else
            {
                break;
            }
        }

        if (internode_stealing)
        {
            int const self_node = torc_node_id();
            int const nnodes = torc_num_nodes();
            int node = (self_node + 1) % nnodes;

            while ((desc_next == NULL) && (node != self_node))
            {
                desc_next = direct_synchronous_stealing_request(node);
                if (desc_next != NULL)
                {
                    break;
                }
                node = (node + 1) % nnodes;
            }

            if (desc_next == NULL)
            {
                internode_stealing = 0;
            }
        }
    }

    return desc_next;
}

int torc_fetch_work()
{
    torc_t *task = get_next_task();

    if (task != NULL)
    {
        torc_to_i_pq_end(task);

        return 1;
    }

    return 0;
}

void _torc_cleanup(torc_t *desc)
{
    if (desc->homenode != torc_node_id())
    {
#if DEBUG
        printf("[%d] sending an answer to %d\n", torc_node_id(), desc->homenode);
#endif
        send_descriptor(desc->homenode, desc, TORC_ANSWER);
    }
    else
    {
#if DEBUG
        printf("[%d] satisfying deps on local inter-node desc\n", torc_node_id());
        fflush(0);
#endif

        for (int i = 0; i < desc->narg; i++)
        {
            if ((desc->callway[i] == CALL_BY_COP2) && (desc->quantity[i] > 1))
            {
                if ((void *)desc->localarg[i] != NULL)
                {
                    free((void *)desc->localarg[i]);
                }
            }
        }

        if (desc->parent)
        {
            _torc_depsatisfy(desc->parent);
        }
    }

    _torc_put_reused_desc(desc);
}

int _torc_scheduler_loop(int once)
{
    torc_t *desc_next;

    while (1)
    {
        desc_next = get_next_task();

        while (desc_next == NULL)
        {
            //! Checking for program completion
            if (appl_finished == 1)
            {
                _torc_md_end();
            }

            thread_sleep(yieldtime);

            desc_next = get_next_task();
            if (desc_next == NULL)
            {
                if (once)
                {
                    return 0;
                }
                thread_sleep(yieldtime);
            }
        }

        /* Execute selected task */
        _torc_execute(desc_next);

        if (once)
        {
            return 1;
        }
    }
}

/* WTH is this? Maybe just a backup of the code? */
int _torc_scheduler_loop2(int once)
{
    while (1)
    {
        torc_t *desc_next = get_next_task();

        while (desc_next == NULL)
        {
            //! Checking for program completion
            if (appl_finished == 1)
            {
                _torc_md_end();
            }

            int wait_count = WAIT_COUNT;
            while (--wait_count)
            {
                sched_yield();
            }

            desc_next = get_next_task();
            if (desc_next == NULL)
            {
                if (once)
                {
                    return 0;
                }
            }
        }

        /* Execute selected task */
        _torc_execute(desc_next);

        if (once)
        {
            return 0; // what is this?
        }
    }
}
