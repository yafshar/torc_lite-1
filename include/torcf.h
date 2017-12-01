C
C     Predefined datatypes
C

      INCLUDE 'mpif.h'
      INCLUDE 'torcf_config.h'

C
C
C

      INTEGER MODE_MS
      INTEGER MODE_SPMD

      PARAMETER (MODE_MS   = 0)
      PARAMETER (MODE_SPMD = 1)

      INTEGER CALL_BY_COP
      INTEGER CALL_BY_REF
      INTEGER CALL_BY_RES
      INTEGER CALL_BY_PTR
      INTEGER CALL_BY_VAL
      INTEGER CALL_BY_COP2
      INTEGER CALL_BY_VAD

      PARAMETER (CALL_BY_COP  = x'0001')
      PARAMETER (CALL_BY_REF  = x'0002')
      PARAMETER (CALL_BY_RES  = x'0003')
      PARAMETER (CALL_BY_PTR  = x'0004')
      PARAMETER (CALL_BY_VAL  = x'0001')
      PARAMETER (CALL_BY_COP2 = x'0005')
      PARAMETER (CALL_BY_VAD  = x'0006')

C
C     INTERFACE
C

      INTEGER*4 torc_worker_id
      INTEGER*4 torc_num_workers
      INTEGER*4 torc_i_worker_id
      INTEGER*4 torc_i_num_workers
      INTEGER*4 torc_node_id
      INTEGER*4 torc_num_nodes
      INTEGER*4 torc_sched_nextcpu

      DOUBLE PRECISION torc_gettime

#if F77_FUNC_(torc_init, TORC_INIT) == torc_init
      EXTERNAL torc_initf
#else
      EXTERNAL torc_init
#endif
      EXTERNAL torc_finalize
      EXTERNAL torc_taskinit
#if F77_FUNC_(torc_create, TORC_CREATE) == torc_create
      EXTERNAL torc_createf
#else
      EXTERNAL torc_create
#endif
#if F77_FUNC_(torc_task, TORC_TASK) == torc_task
      EXTERNAL torc_taskf
#else
      EXTERNAL torc_task
#endif
      EXTERNAL torc_waitall
      EXTERNAL torc_register_task
      EXTERNAL torc_enable_stealing
      EXTERNAL torc_disable_stealing
#if F77_FUNC_(torc_broadcast, TORC_BROADCAST) == torc_broadcast
      EXTERNAL torc_broadcastf
#else
      EXTERNAL torc_broadcast
#endif
      EXTERNAL torc_sleep
