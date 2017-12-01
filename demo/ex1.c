#include <stdio.h>
#include <math.h>
#include <torc.h>

void callback(int a, double *res) {
    printf("sqrt(%d)=%lf\n", a, *res);
}

void taskf(int a, double *res) {
    *res = sqrt(a);
//     torc_create_detached(torc_node_id(), callback, 2,
//                          1, MPI_INT,    CALL_BY_VAD,
//                          1, MPI_DOUBLE, CALL_BY_RES,
//                          a, res);
}

int main(int argc, char *argv[]) {
    int i;
    double y[10];

    torc_register_task(callback);
    torc_register_task(taskf);

    torc_init(argc, argv, MODE_MS);

    for (i=0; i<10; i++) {
        torc_create(-1, taskf, 2,
                     1, MPI_INT,    CALL_BY_VAD,
                     1, MPI_DOUBLE, CALL_BY_RES,
                     i, &y[i]);
    }
    torc_waitall();

    for (i=0; i<10; i++) printf("sqrt(%d)=%lf\n", i, y[i]);
    torc_finalize();
    return 0;
}
