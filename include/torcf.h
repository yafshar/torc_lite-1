!
!     Predefined datatypes
!

      INCLUDE 'mpif.h'

!
!
!

      INTEGER, PARMETER :: MODE_MS = 0
      INTEGER, PAREMETR :: MODE_SPMD = 1

      INTEGER, PAREMETR :: CALL_BY_COP = x'0001'
      INTEGER, PAREMETR :: CALL_BY_REF = x'0002'
      INTEGER, PAREMETR :: CALL_BY_RES = x'0003'
      INTEGER, PAREMETR :: CALL_BY_PTR = x'0004'
      INTEGER, PAREMETR :: CALL_BY_VAL = x'0001'
      INTEGER, PAREMETR :: CALL_BY_COP2= x'0005'
      INTEGER, PAREMETR :: CALL_BY_VAD = x'0006'

!
!     INTERFACE
!

      INTEGER*4 :: torc_worker_id
      INTEGER*4 :: torc_num_workers
      INTEGER*4 :: torc_i_worker_id
      INTEGER*4 :: torc_i_num_workers
      INTEGER*4 :: torc_node_id
      INTEGER*4 :: torc_num_nodes
      INTEGER*4 :: torc_sched_nextcpu

      DOUBLE PRECISION :: torc_gettime

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
