AM_CFLAGS = @DEBUG_FLAG@ -I. -I../include 
LIBS = -L../src -ltorc $(MPILIB) -lpthread -lm 

bin_PROGRAMS= masterslave mbench1 fibo broadcast struct pipe async zerolength

masterslave_SOURCES = masterslave.c
mbench1_SOURCES = mbench1.c
fibo_SOURCES = fibo.c
broadcast_SOURCES = broadcast.c
struct_SOURCES = struct.c
pipe_SOURCES = pipe.c
async_SOURCES = async.c
zerolength_SOURCES = zerolength.c

.c.o:
	$(CC) $(AM_CFLAGS) $(CFLAGS) -c $<
