/*
 *  broadcast.c
 *  TORC_Lite
 *
 *  Created by Panagiotis Hadjidoukas on 1/1/14.
 *  Copyright 2014 ETH Zurich. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <torc.h>

int gi;
int gti[16];

double gd;
double gtd[16];

void slave()
{
    int me = torc_worker_id();
    int node = torc_node_id();

    printf("worker[%d] node[%d]: %d %f %d %f\n", me, node, gi, gd, gti[5], gtd[5]);
    sleep(1);
    return;
}

/**
 * @brief A TORC broadcasting example
 * 
 * In this example, at first we create some scalar and vector variables and initilize them 
 * After starting the TORC library we change these variables on the master thread 
 * Printing the values at this stage would show their values on each worker 
 * After broadcasting the values printing them again would show the updated values on each worker
 * 
 */
int main(int argc, char *argv[])
{
    gi = 5;
    gd = 5.0;
    for (int i = 0; i < 16; i++)
    {
        gti[i] = 100;
        gtd[i] = 100.0;
    }
    
    //! First, register the task 
    torc_register_task(slave);

    //! Second, initialize the TORC library
    torc_init(argc, argv);

    // Here, we change the values
    gi = 23;
    gd = 23.0;
    for (int i = 0; i < 16; i++)
    {
        gti[i] = torc_worker_id() + 1000;
        gtd[i] = gti[i] + 1;
    }

    //! Print the values on each worker
    int const ntasks = torc_num_workers();
    for (int i = 0; i < ntasks; i++)
    {
        torc_create(-1, slave, 0);
    }
    torc_waitall();

    //! broadcast the values to every other nodes
    torc_broadcast(&gi, 1, MPI_INT);
    torc_broadcast(&gd, 1, MPI_DOUBLE);
    torc_broadcast(&gti, 16, MPI_INT);
    torc_broadcast(&gtd, 16, MPI_DOUBLE);

    //! Print the values on each worker
    for (int i = 0; i < ntasks; i++)
    {
        torc_create(-1, slave, 0);
    }
    torc_waitall();

    //! Finalize the TORC library
    torc_finalize();
    return 0;
}
