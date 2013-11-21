#ifndef ZOOKEEPER_BARRIER
	#define ZOOKEEPER_BARRIER

	#define BARRIER_NODE "/barrier"
	#define BARRIER_MAX_NODES 5
	#define BARRIER_READY_NODE "/ready"

	#include <zookeeper.h>
	#include <pthread.h>

	/* Cria a barreira, se já não existir */
	int init_barrier(zhandle_t *connection);

	/* API de entrada e saída de uma barreira, segundo o algoritmo do recipes */
	int enter_barrier(zhandle_t *connection, char *client_id, int threshold);
	int leave_barrier(zhandle_t *connection, char *client_id);

extern int crowdedflag;
extern pthread_mutex_t barrier_mutex;
extern pthread_cond_t barrier_varcond;
extern pthread_mutex_t crowded_mutex;
extern pthread_cond_t crowded_varcond;

#endif
