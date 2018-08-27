# torc_liteBasic instructions:
 
1. Make sure you have an MPI installation available on your system, preferably a thread-safe one

2. ./configure CC=mpicc F77=mpif77 

3. make

4. cd demo; make


Installation:

1. ./configure --prefix=<installation_directory> CC=mpicc F77=mpif77 

2. make; make install

3. Add <installation_directory>/bin to your PATH. This will allow direct access to the torc_cflags and torc_libs flags

4. Compile your code with  CFLAGS=`torc_cflags` and LDFLAGS=`torc_libs`, e.g. mpicc `torc_cflags` -o mytest mytest.c `torc_libs`
