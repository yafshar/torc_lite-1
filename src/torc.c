/*
 *  torc.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

#ifdef TORC_STATS
static int invisible_flag = 0;
#endif

//! Initialize the TORC static flag (Global variable)
int torc_initialized = 0;

//! Global variable TORC data
struct torc_data *torc_data;

/**
 * @brief Waits for all communications dependencies to complete. 
 * 
 */
void torc_waitall()
{
    _torc_block();
}

void torc_waitall2()
{
    _torc_block2();
}

/**
 * @brief Waits for all communications dependencies to complete. 
 * 
 */
void torc_waitall3()
{
    torc_t *self = _torc_self();

    {
        _lock_acquire(&self->lock);
        --self->ndep;
        _lock_release(&self->lock);
    }

    int remdeps;

    while (1)
    {
        {
            _lock_acquire(&self->lock);
            remdeps = self->ndep;
            _lock_release(&self->lock);
        }

        if (remdeps == 0)
        {
            break;
        }

        thread_sleep(0);
    }
}

int torc_scheduler_loop(int once)
{
    return _torc_scheduler_loop(once);
}

#ifdef TORC_STATS
/**
 * @brief Set the invisible flag
 * 
 * @param flag 
 */
void torc_set_invisible(int flag)
{
    invisible_flag = flag;
}
#endif

/**
 * @brief Separates the thread of execution from the thread object, allowing execution to continue independently.  
 * 
 * @param queue 
 * @param work   Callable object to execute in the new thread
 * @param narg   Number of arguments of this callable object
 * @param ... 
 */
void torc_task_detached(int queue, void (*work)(), int narg, ...)
{
    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS!");
    }

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        //! It is a detached thread
        desc->parent = NULL;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! External
        desc->rte_type = 1;
        desc->level = 0;
    }

    va_list ap;
    va_start(ap, narg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = va_arg(ap, int);
        desc->dtype[i] = va_arg(ap, MPI_Datatype);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = va_arg(ap, int);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->quantity[i] == 0)
        {
            VIRT_ADDR dummy;
            dummy = va_arg(ap, VIRT_ADDR);
            continue;
        }
        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            void *pmem = malloc(desc->quantity[i] * typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);

            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);

            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    if (queue == -1)
    {
        torc_to_rq_end(desc);
    }
    else
    {
        //! Public local (worker) queue
        torc_to_lrq_end(queue, desc);
    }
}

/**
 * @brief Execute the task  
 * 
 * @param queue 
 * @param work   Callable object to execute in the thread
 * @param narg   Number of arguments of this callable object
 * @param ... 
 */
void torc_task(int queue, void (*work)(), int narg, ...)
{
    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS !");
    }

    torc_t *self = _torc_self();

    //! Check if rte_init has been called
    _lock_acquire(&self->lock);
    if (self->ndep == 0)
    {
        self->ndep = 1;
    }
    _lock_release(&self->lock);

    _torc_depadd(self, 1);

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        desc->parent = self;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! External
        desc->rte_type = 1;
        desc->level = self->level + 1;

#ifdef TORC_STATS
        if (invisible_flag)
        {
            //! invisible
            desc->rte_type = 2;
        }
        else
        {
            //! If it is not set to be invisible, sum all the creation
            created[self->vp_id]++;
        }
#endif
    }

    va_list ap;
    va_start(ap, narg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = va_arg(ap, int);
        desc->dtype[i] = va_arg(ap, MPI_Datatype);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = va_arg(ap, int);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->quantity[i] == 0)
        {
            VIRT_ADDR dummy;
            dummy = va_arg(ap, VIRT_ADDR);
            continue;
        }

        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            void *pmem = malloc(desc->quantity[i] * typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);

            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);

            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    if (queue == -1)
    {
        torc_to_rq_end(desc);
    }
    else
    {
        //! Public local (worker) queue
        torc_to_lrq_end(queue, desc);
    }
}

void torc_task_ex(int queue, int invisible, void (*work)(), int narg, ...)
{
    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS !");
    }

    torc_t *self = _torc_self();

    //! Check if rte_init has been called
    _lock_acquire(&self->lock);
    if (self->ndep == 0)
    {
        self->ndep = 1;
    }
    _lock_release(&self->lock);

    _torc_depadd(self, 1);

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        desc->parent = self;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! External
        desc->rte_type = 1;
        desc->level = self->level + 1;

#ifdef TORC_STATS
        if (invisible)
        {
            //! invisible
            desc->rte_type = 2;
        }
        else
        {
            //! If it is not set to be invisible, sum all the creation
            created[self->vp_id]++;
        }
#endif
    }

    va_list ap;
    va_start(ap, narg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = va_arg(ap, int);
        desc->dtype[i] = va_arg(ap, MPI_Datatype);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = va_arg(ap, int);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->quantity[i] == 0)
        {
            VIRT_ADDR dummy;
            dummy = va_arg(ap, VIRT_ADDR);
            continue;
        }

        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            void *pmem = malloc(desc->quantity[i] * typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);

            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);

            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    if (queue == -1)
    {
        torc_to_rq_end(desc);
    }
    else
    {
        //! Public local (worker) queue
        torc_to_lrq_end(queue, desc);
    }
}

void torc_task_direct(int queue, void (*work)(), int narg, ...)
{

    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS");
    }

    torc_t *self = _torc_self();

    /* Check if rte_init has been called */
    _lock_acquire(&self->lock);
    if (self->ndep == 0)
    {
        self->ndep = 1;
    }
    _lock_release(&self->lock);

    _torc_depadd(self, 1);

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        desc->parent = self;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! external - direct execution
        desc->rte_type = 20;
        desc->level = self->level + 1;
    }

    va_list ap;
    va_start(ap, narg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = va_arg(ap, int);
        desc->dtype[i] = va_arg(ap, MPI_Datatype);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = va_arg(ap, int);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->quantity[i] == 0)
        {
            VIRT_ADDR dummy;
            dummy = va_arg(ap, VIRT_ADDR);
            continue;
        }

        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            void *pmem = malloc(desc->quantity[i] * typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);

            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);

            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    if (queue == -1)
    {
        torc_to_rq_end(desc);
    }
    else
    {
        //! Public local (worker) queue
        torc_to_lrq_end(queue, desc);
    }
}

//! Return the time
double torc_gettime()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double)t.tv_sec + (double)t.tv_usec * 1.0E-6;
}

/**
 * @brief Get the level
 * 
 * @return int 
 */
int torc_getlevel()
{
    torc_t *self = _torc_self();
    return self->level;
}

/**
 * @brief MPI rank in the communicator comm_out
 * 
 * @return int 
 */
int torc_node_id()
{
    return mpi_rank;
}

/**
 * @brief Number of nodes in the communicator comm_out
 * 
 * @return int 
 */
int torc_num_nodes()
{
    return mpi_nodes;
}

/**
 * @brief Number of threads on each node
 * 
 * @return int 
 */
int torc_i_num_workers()
{
    return kthreads;
}

/**
 * @brief  Get the virtual processor ID
 * 
 * @return int 
 */
int torc_i_worker_id()
{
    return _torc_get_vpid();
}

/**
 * @brief Get the total number of threads
 * 
 * @return int 
 */
int torc_num_workers()
{
    return (torc_num_nodes() > 1) ? _torc_total_num_threads() : torc_i_num_workers();
}

/**
 * @brief Get the local thread index
 * 
 * @return int 
 */
int torc_worker_id()
{
    return (torc_num_nodes() > 1) ? local_thread_id_to_global_thread_id(_torc_get_vpid()) : _torc_get_vpid();
}

/**
 * @brief Initializes the TORC execution environment on the MPI_COMM_WORLD communicator
 * 
 * @param argc Pointer to the number of arguments. 
 * @param argv Argument vector. 
 */
void torc_init(int argc, char *argv[])
{
    if (torc_initialized)
    {
        return;
    }

    //! This registration is for safety, in case we did not register a function before torc_init
    torc_register_task((void *)torc_register_task_internal);

    torc_initialized = 1;

    torc_data = calloc(1, sizeof(struct torc_data));

    _torc_opt(argc, argv);

    _torc_env_init();

    _torc_worker(0);
}

/**
 * @brief Initializes the TORC execution environment on the comm_in communicator
 * 
 * @param argc     Pointer to the number of arguments. 
 * @param argv     Argument vector. 
 * @param comm_in  Communicator (handle). 
 */
void torc_init_comm(int argc, char *argv[], MPI_Comm comm_in)
{
    if (torc_initialized)
    {
        return;
    }

    //! This registration is for safety, in case we did not register a function before torc_init
    torc_register_task((void *)torc_register_task_internal);

    torc_initialized = 1;

    torc_data = calloc(1, sizeof(struct torc_data));

    __torc_opt(argc, argv, comm_in);

    _torc_env_init();

    _torc_worker(0);
}

/**
 * @brief Get the argument address in the current description
 * 
 * @param arg 
 * @return void* 
 */
void *torc_getarg_addr(int arg)
{
    torc_t *self = _torc_self();

    if (torc_node_id() == self->homenode)
    {
        return (self->callway[arg] == CALL_BY_COP) ? &(self->localarg[arg]) : ((void *)self->localarg[arg]);
    }
    else
    {
        return (self->callway[arg] == CALL_BY_COP) ? &(self->temparg[arg]) : ((void *)self->temparg[arg]);
    }
}

/**
 * @brief Get the number of arguments in the current description
 * 
 * @param arg 
 * @return int 
 */
int torc_get_num_args()
{
    return _torc_self()->narg;
}

/**
 * @brief 
 * 
 * @param arg 
 * @return int 
 */
int torc_getarg_callway(int arg)
{
    return _torc_self()->callway[arg];
}

/**
 * @brief Get the number of the data of the argument number arg 
 * 
 * @param arg 
 * @return int 
 */
int torc_getarg_count(int arg)
{
    return _torc_self()->quantity[arg];
}

/**
 * @brief Get the size of the data type of the argument number arg 
 * 
 * @param arg 
 * @return int 
 */
int torc_getarg_size(int arg)
{
    int typesize;

    MPI_Type_size(_torc_self()->dtype[arg], &typesize);

    return typesize;
}

/**
 * @brief FORTRAN interfaces
 * 
 */

/**
 * We define this macro f77fun to avoid compiler confusion
 * In some cases, like IBM compiler, due to the compiler name mangling, 
 * FORTRAN and C interface are the same and we do not need to define a 
 * new FORTRAN interface
 */
#define f77fun 1

void F77_FUNC_(torc_taskinit, TORC_TASKINIT)()
{
    /* nothing to do */
}

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
void F77_FUNC_(torc_waitall, TORC_WAITALL)()
{
    torc_waitall();
}
#endif

void F77_FUNC_(torc_createf, TORC_CREATEF)(int *pqueue, void (*work)(), int *pnarg, ...)
{
    int const narg = *pnarg;
    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS");
    }

    torc_t *self = _torc_self();

    //! Check if rte_init has been called
    _lock_acquire(&self->lock);
    if (self->ndep == 0)
    {
        self->ndep = 1;
    }
    _lock_release(&self->lock);

    _torc_depadd(self, 1);

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        desc->parent = self;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! External
        desc->rte_type = 1;
        desc->level = self->level + 1;

#ifdef TORC_STATS
        if (invisible_flag)
        {
            //! invisible
            desc->rte_type = 2;
        }
        else
        {
            //! If it is not set to be invisible, sum all the creation
            created[self->vp_id]++;
        }
#endif
    }

    va_list ap;
    va_start(ap, pnarg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = *va_arg(ap, int *);
        MPI_Fint dt = *va_arg(ap, MPI_Fint *);
        desc->dtype[i] = MPI_Type_f2c(dt);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = *va_arg(ap, int *);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            void *pmem = malloc(desc->quantity[i] * typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);

            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);

            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    if (*pqueue == -1)
    {
        torc_to_rq_end(desc);
    }
    else
    {
        torc_to_lrq_end(*pqueue, desc);
    }
}

// this is here to support the new pndl version
void F77_FUNC_(torc_taskf, TORC_TASKF)(void (*work)(), int *ptype, int *pnarg, ...)
{
    int const narg = *pnarg;
    if (narg > MAX_TORC_ARGS)
    {
        Error("narg > MAX_TORC_ARGS");
    }

    torc_t *self = _torc_self();

    /* Check if rte_init has been called */
    _lock_acquire(&self->lock);
    if (self->ndep == 0)
    {
        self->ndep = 1;
    }
    _lock_release(&self->lock);

    _torc_depadd(self, 1);

    int type = *ptype;

    torc_t *desc = _torc_get_reused_desc();

    {
        _lock_init(&desc->lock);
        desc->parent = self;
        desc->vp_id = -1;
        _torc_set_work_routine(desc, work);
        desc->work_id = -1;
        desc->narg = narg;
        desc->homenode = torc_node_id();
        desc->sourcenode = torc_node_id();
        desc->target_queue = -1;
        desc->inter_node = 1;
        //! External
        desc->rte_type = 1;
        desc->level = self->level + 1;

#ifdef TORC_STATS
        if (invisible_flag || type)
        {
            //! invisible
            desc->rte_type = 2;
        }
        else
        {
            //! If it is not set to be invisible, sum all the creation
            created[self->vp_id]++;
        }
#endif
    }

    va_list ap;
    va_start(ap, pnarg);

    for (int i = 0; i < narg; i++)
    {
        desc->quantity[i] = *va_arg(ap, int *);
        MPI_Fint dt = *va_arg(ap, MPI_Fint *);
        desc->dtype[i] = MPI_Type_f2c(dt);
        desc->btype[i] = _torc_mpi2b_type(desc->dtype[i]);
        desc->callway[i] = *va_arg(ap, int *);

        if ((desc->callway[i] == CALL_BY_COP) && (desc->quantity[i] > 1))
        {
            desc->callway[i] = CALL_BY_COP2;
        }

#if DEBUG
        printf("ARG %d : Q = %d, T = %d, C = %x O\n", i, desc->quantity[i], desc->dtype[i], desc->callway[i]);
        fflush(0);
#endif
    }

    for (int i = 0; i < narg; i++)
    {
        if (desc->callway[i] == CALL_BY_COP)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            switch (typesize)
            {
            case 4:
                desc->localarg[i] = *va_arg(ap, INT32 *);
                break;
            case 8:
                desc->localarg[i] = *va_arg(ap, INT64 *);
                break;
            default:
                Error("Type size is not 4 or 8!");
                break;
            }
        }
        else if (desc->callway[i] == CALL_BY_COP2)
        {
            int typesize;
            MPI_Type_size(desc->dtype[i], &typesize);

            VIRT_ADDR addr = va_arg(ap, VIRT_ADDR);
            
            void *pmem = malloc(desc->quantity[i] * typesize);
            
            memcpy(pmem, (void *)addr, desc->quantity[i] * typesize);
            
            desc->localarg[i] = (INT64)pmem;
        }
        else
        {
            //! pointer (C: PTR, VAL)
            desc->localarg[i] = va_arg(ap, VIRT_ADDR);
        }
    }

    {
        int const queue = torc_worker_id();
        if (queue == -1)
        {
            torc_to_rq_end(desc);
        }
        else
        {
            torc_to_lrq_end(queue, desc);
        }
    }
}

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
int F77_FUNC_(torc_num_workers, TORC_NUM_WORKERS)(void)
{
    return torc_num_workers();
}
#endif

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
int F77_FUNC_(torc_worker_id, TORC_WORKER_ID)(void)
{
    return torc_worker_id();
}
#endif

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
int F77_FUNC_(torc_node_id, TORC_NODE_ID)(void)
{
    return torc_node_id();
}
#endif

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
int F77_FUNC_(torc_num_nodes, TORC_NUM_NODES)(void)
{
    return torc_num_nodes();
}
#endif

void F77_FUNC_(torc_broadcastf, TORC_BROADCASTF)(void *a, long *count, MPI_Fint *datatype)
{

    MPI_Datatype dt;

    dt = MPI_Type_f2c(*datatype);
    torc_broadcast(a, *count, dt);
}

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
void F77_FUNC_(torc_enable_stealing, TORC_ENABLE_STEALING)()
{
    torc_enable_stealing();
}
#endif

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
void F77_FUNC_(torc_disable_stealing, TORC_DISABLE_STEALING)()
{
    torc_disable_stealing();
}
#endif

int torc_sched_nextcpu(int cpu, int stride)
{
    int res;
    int ncpus = torc_num_workers();

    if (cpu == -1)
        cpu = torc_worker_id();
    else
        cpu = (cpu + stride) % ncpus;

    res = cpu;
    return res;
}

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
int F77_FUNC_(torc_sched_nextcpu, TORC_SCHED_NEXTCPU)(int *cpu, int *stride)
{
    return torc_sched_nextcpu(*cpu, *stride);
}
#endif

void F77_FUNC_(torc_initf, TORC_INITF)()
{
    torc_init(0, NULL);
}

#if F77_FUNC_(f77fun, F77FUN) == f77fun
#else
void F77_FUNC_(torc_finalize, TORC_FINALIZE)()
{
    torc_finalize();
}
#endif

void F77_FUNC_(fff, FFF)()
{
    fflush(0);
}

#undef f77fun
