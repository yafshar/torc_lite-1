# TORC library
TORC is a task-parallel library that provides a programming and runtime environment where parallel programs can be executed unaltered on both shared and distributed memory platforms. 
TORC supports arbitrary nesting of tasks while all data transfers in the library are performed with explicit, transparent to the user, messaging. 
Due to the task stealing mechanism, the programmer has only to decide about the task distribution scheme. 
A task function can either include source code supplied by the user or invoke an external simulation program. (can be sequential or parallel running on multicores and clusters (e.g. using OpenMP and MPI) or GPUs)

The current repository is in the exploring phase and it is not the final version, it is constantly in develpment. It is forked from the original work of [(TORC tasking library)](https://github.com/cselab/torc_lite), and contains changes toward the original library.
- The first major difference is the initialization of the TORC execution environment. In the original library by default it initializes on the MPI_COMM_WORLD communicator, while here there is an option to initilize the TORC execution environment on a user defined communicator.
- The second major difference is the registration of tasks (this is in the experimental phase). In the original TORC library, user is allowed to register task before initilization of the execution environment, while now there is this option to register tasks at any places.
This addition is for C++ convenience, to register some function at construction of classes.
- While still in the experimental phase, but we are updating the MPI functions to take advantage of all the non-blocking communications (MPI_Iallgather, ...)


References:
------------
- [A Runtime Library for Platform-Independent Task Parallelism](https://ieeexplore.ieee.org/document/6169554)
- [Pi4U: A high performance computing framework for Bayesian uncertainty quantification of complex models](https://www.sciencedirect.com/science/article/pii/S0021999114008134)


Basic instructions:
-------------------
1. Make sure you have an MPI installation available on your system, preferably a thread-safe one
2. ./configure CC=mpicc F77=mpif77 
3. make
4. cd demo; make


Installation:
------------
1. ./configure --prefix=<installation_directory> CC=mpicc F77=mpif77 
2. make; make install
3. Add <installation_directory>/bin to your PATH. This will allow direct access to the torc_cflags and torc_libs flags
4. Compile your code with  CFLAGS=`torc_cflags` and LDFLAGS=`torc_libs`, e.g. mpicc `torc_cflags` -o mytest mytest.c `torc_libs`
