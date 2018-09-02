/*
 *  torc_queue.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */
#include <torc_internal.h>
#include <torc.h>

/**
 * @brief Initilize all the ready queues
 * 
 */
void rq_init()
{
    _queue_init(&reuse_q);

    _queue_init(&private_grq);

    for (int i = 0; i < 10; i++)
    {
        _queue_init(&public_grq[i]);
    }
}

/**
 * @brief Adds the descriptor desc at the tail of the queue reuse_q
 * 
 * @param desc 
 */
static void torc_to_i_reuseq_end(torc_t *desc)
{
    _enqueue_tail(&reuse_q, desc);
}

/**
 * @brief Double-ended queue
 * 
 * @return torc_t* 
 */
static torc_t *torc_i_reuseq_dequeue()
{
    torc_t *desc = NULL;

    _dequeue(&reuse_q, &desc);

    return desc;
}

torc_t *_torc_get_reused_desc()
{
    //! Size of the data structure
    static unsigned long torc_size = sizeof(torc_t);

    torc_t *desc = torc_i_reuseq_dequeue();

    if (desc != NULL)
    {
        static unsigned long offset = sizeof(_lock_t);

        char *ptr = (char *)desc;
        ptr += offset;

        memset(ptr, 0, torc_size - offset);
    }
    else
    {
        desc = calloc(1, torc_size);
    }

    return desc;
}

/**
 * @brief Put the descriptor desc at the tail of the queue reuse_q
 * 
 * @param desc 
 */
void _torc_put_reused_desc(torc_t *desc)
{
    torc_to_i_reuseq_end(desc);
}

/**
 * \defgroup Intra-node Queues
 */
/**@{*/

/**
 * @brief Add the descriptor desc at the head of the private global queue private_grq
 * 
 * @param desc 
 */
void torc_to_i_pq(torc_t *desc)
{
    _enqueue_head(&private_grq, desc);
}

/**
 * @brief Add the descriptor desc at the tail of the private global queue private_grq
 * 
 * @param desc 
 */
void torc_to_i_pq_end(torc_t *desc)
{
    _enqueue_tail(&private_grq, desc);
}

/**
 * @brief Get the descriptor of a double-eneded queue of the private global queue
 * 
 * @return torc_t* 
 */
torc_t *torc_i_pq_dequeue()
{
    torc_t *desc = NULL;

    _dequeue(&private_grq, &desc);

    return desc;
}

/**
 * @brief Add the descriptor desc at the head of the public global queue public_grq
 * 
 * @param desc 
 */
void torc_to_i_rq(torc_t *desc)
{
    int const lvl = (desc->level <= 1) ? 0 : (desc->level >= 11) ? 9 : desc->level - 1;

    _enqueue_head(&public_grq[lvl], desc);
}

/**
 * @brief Add the descriptor desc at the tail of the public global queue public_grq
 * 
 * @param desc 
 */
void torc_to_i_rq_end(torc_t *desc)
{
    int const lvl = (desc->level <= 1) ? 0 : (desc->level >= 11) ? 9 : desc->level - 1;

    _enqueue_tail(&public_grq[lvl], desc);
}

/**
 * @brief Get the descriptor of a double-eneded queue of the public global queue
 * 
 * Based on the way it is called in TORC we do not need to check the lvl bound
 * 
 * @return torc_t* 
 */
torc_t *torc_i_rq_dequeue(int lvl)
{
    torc_t *desc = NULL;

    _dequeue(&public_grq[lvl], &desc);

    return desc;
}

/**@}*/

/**
 * \defgroup Inter-node Queues
 */
/**@{*/

/**
 * @brief Copy local data arguments to the temporary space (Read the arguments) 
 * 
 * @param desc 
 */
static void read_arguments(torc_t *desc)
{
    for (int i = 0; i < desc->narg; i++)
    {
        desc->temparg[i] = desc->localarg[i];
    }
}

void torc_to_nrq(int target_node, torc_t *desc)
{
#if DEBUG
    printf("rte_to_lrq_2: target = %d, target_node = %d, target_queue = %s\n", -1, target_node, "inter-node gq");
    fflush(0);
#endif

    desc->target_queue = -1;
    //! alternatively, it can be set only when it goes outside
    desc->inter_node = 1;
    desc->insert_in_front = 1;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        //! Adds the descriptor desc at the head of the public global queue public_grq
        torc_to_i_rq(desc);
    }
}

void torc_to_nrq_end(int target_node, torc_t *desc)
{
#if DEBUG
    printf("rte_to_lrq_end_2: target = %d, target_node = %d, target_queue = %s\n", -1, target_node, "inter-node gq");
    fflush(0);
#endif

    desc->target_queue = -1;
    //! alternatively, it can be set only when it goes outside
    desc->inter_node = 1;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        //! Adds the descriptor desc at the tail of the public global queue public_grq
        torc_to_i_rq_end(desc);
    }
}

/**
 * @brief Public global queue - general version
 * 
 * @param desc 
 */
void torc_to_rq(torc_t *desc)
{
    static int initialized = 0;
    static int target_node;

    if (!initialized)
    {
        initialized = 1;
        target_node = torc_node_id();
    }

#if DEBUG
    printf("rte_to_rq : target_node = %d\n", target_node);
    fflush(0);
#endif

    //! Global
    desc->target_queue = -1;
    desc->inter_node = 1;
    desc->insert_in_front = 1;
    desc->insert_private = 0;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        torc_to_i_rq(desc);
    }

    int const total_nodes = torc_num_nodes();

    target_node = (target_node + 1) % total_nodes;
}

/**
 * @brief node version
 * 
 * @param desc 
 */
void torc_to_rq_end__(torc_t *desc)
{
    static int initialized = 0;
    static int target_node;

    if (!initialized)
    {
        initialized = 1;
        target_node = torc_node_id();
    }

#if DEBUG
    printf("rte_to_rq_end: target_node = %d\n", target_node);
    fflush(0);
#endif

    //! global
    desc->target_queue = -1;
    desc->inter_node = 1;
    desc->insert_private = 0;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        torc_to_i_rq_end(desc);
    }

    int const total_nodes = torc_num_nodes();

    target_node = (target_node + 1) % total_nodes;
}

/**
 * @brief worker version
 * 
 * @param desc 
 */
void torc_to_rq_end(torc_t *desc)
{
    static int initialized = 0;

    static int target_worker;

    if (!initialized)
    {
        initialized = 1;

        target_worker = torc_worker_id();
    }

    if (torc_num_nodes() == 1)
    {
        torc_to_i_rq_end(desc);

        return;
    }

    int target_node = global_thread_id_to_node_id(target_worker);
    int target_queue = global_thread_id_to_local_thread_id(target_worker);

#if DEBUG
    printf("rte_to_rq_end: target_worker = %d, target_node = %d, target_queue = %d\n", target_worker, target_node, target_queue);
    fflush(0);
#endif

    //! Global
    desc->target_queue = target_queue;
    desc->inter_node = 1;
    desc->insert_private = 0;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        torc_to_i_rq_end(desc);
    }

    int const total_workers = torc_num_workers();

    target_worker = (target_worker + 1) % total_workers;
}

/**
 * @brief Public local (worker) queue
 * 
 * @param target 
 * @param desc 
 */
void torc_to_lrq_end(int target, torc_t *desc)
{
    if (torc_num_nodes() == 1)
    {
        torc_to_i_rq_end(desc);

        return;
    }

    int target_node = global_thread_id_to_node_id(target);
    int target_queue = global_thread_id_to_local_thread_id(target);

#if DEBUG
    printf("rte_to_lrq_end: target = %d, target_node = %d, target_queue = %d\n", target, target_node, target_queue);
    fflush(0);
#endif

    desc->target_queue = target_queue;
    desc->inter_node = 1;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        torc_to_i_rq_end(desc);
    }
}

void torc_to_lrq(int target, torc_t *desc)
{
    if (torc_num_nodes() == 1)
    {
        torc_to_i_rq(desc);

        return;
    }

    int target_node = global_thread_id_to_node_id(target);
    int target_queue = global_thread_id_to_local_thread_id(target);

#if DEBUG
    printf("rte_to_lrq_end: target = %d, target_node = %d, target_queue = %d\n", target, target_node, target_queue);
    fflush(0);
#endif

    desc->target_queue = target_queue;
    desc->inter_node = 1;
    desc->insert_in_front = 1;

    if (torc_node_id() != target_node)
    {
#if DEBUG
        printf("enqueing remotely: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        send_descriptor(target_node, desc, TORC_NORMAL_ENQUEUE);

        _torc_put_reused_desc(desc);
    }
    else
    {
#if DEBUG
        printf("enqueing locally: desc->rte_desc = %p\n", desc);
        fflush(0);
#endif
        //! Read the arguments
        read_arguments(desc);

        torc_to_i_rq(desc);
    }
}

/**@}*/
