# include common defs
torclibdir = $(libdir)
torclib_LIBRARIES = libtorc.a

AM_CFLAGS = @DEBUG_FLAG@ -DMAX_NVPS=@NVPS@ -DMAX_NODES=@NNODES@ -DMAX_TORC_TASKS=@NTASKS@

libtorc_a_SOURCES = torc_runtime.c torc_queue.c torc_thread.c torc_comm.c torc_server.c torc.c

torcincdir = $(includedir)
torcinc_HEADERS = ../include/torc.h ../include/torcf.h  
