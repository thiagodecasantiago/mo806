#include <zookeeper.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "barrier.h"

int crowdedflag = 1;
pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t barrier_varcond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t crowded_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t crowded_varcond = PTHREAD_COND_INITIALIZER;

static char *mk_client_node_path(char* client_id)
{
    char *path;

    path = strcpy((char*) malloc(strlen(BARRIER_NODE) + strlen("/") + strlen(client_id)) + 1,
                  BARRIER_NODE);
    strcat(path, "/");
    strcat(path, client_id);

    return path;
}

static int get_children_number(zhandle_t *connection, char *path,
                               struct String_vector *clients)
{
    zoo_get_children(connection, path, 0, clients);
    return (int) clients->count;
}

int init_barrier(zhandle_t *connection)
{
    int rc;

    rc = zoo_create(connection, BARRIER_NODE, NULL, 0,
                    &ZOO_OPEN_ACL_UNSAFE, 0, NULL, 0);
    return rc;
}

int enter_barrier(zhandle_t *connection, char *client_id, int threshold)
{
    char *client_node;
    int barrier_clients;
    struct String_vector clients;
    struct Stat stat;

    client_node = mk_client_node_path(client_id);
    /* seta um watch para o nó ready */
    zoo_exists(connection, BARRIER_READY_NODE, 1, &stat);
    /* pega o número de clientes na barreira */
    barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    /* verifica se é o cliente */
    while (barrier_clients >= threshold) {
        /* Barreira lotada, espera */
        fprintf(stderr, "\n\n##### Esperando barreira esvaziar #####\n\n");
	//zoo_exists(connection, BARRIER_READY_NODE, 1, &stat);
        pthread_mutex_lock(&crowded_mutex);
        pthread_cond_wait(&crowded_varcond, &crowded_mutex);
        pthread_mutex_unlock(&crowded_mutex);
	//while (crowdedflag == 1){}
	//crowdedflag = 1;
        barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    }
    /* cria o nó do cliente */
    zoo_create(connection, client_node, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL,
               NULL, 0);
    fprintf(stderr, "\n\n##### Criou nó efêmero #####\n\n");
    /* pega o número de clientes na barreira */
    barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    /* verifica se é o cliente */
    while (barrier_clients < threshold) {
        /* se não for o último cliente, espera */
        fprintf(stderr, "\n\n##### Esperando threshold #####\n\n");
        pthread_mutex_lock(&barrier_mutex);
        pthread_cond_wait(&barrier_varcond, &barrier_mutex);
        pthread_mutex_unlock(&barrier_mutex);
        barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    }
    /* eu sou o último cliente */
    zoo_create(connection, BARRIER_READY_NODE, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0,
               NULL, 0);

    return EXIT_SUCCESS;
}

int leave_barrier(zhandle_t *connection, char *client_id)
{
    int barrier_clients;
    char *client_node;
    struct String_vector clients;

    client_node = mk_client_node_path(client_id);
    barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    fprintf(stderr, "\n\n### A: barrier_clients %d\n\n", barrier_clients);
    while (barrier_clients != 0) {
        if (barrier_clients == 1 && !strcmp(clients.data[0], client_id)) {
            /* único nó vivo */
            zoo_delete(connection, client_node, -1);
	    zoo_delete(connection, BARRIER_READY_NODE, -1);
            return EXIT_SUCCESS;
        } else if (!strcmp(clients.data[barrier_clients -1], client_id)) {
            /* se é o menor nó na barreira */
            zoo_exists(connection, clients.data[0], 1, NULL);
	    printf("\n\n##### Client.data[0]: %s  #####\n\n", clients.data[0]);
        } else {
            zoo_delete(connection, client_node, -1);
            zoo_exists(connection, clients.data[barrier_clients -1], 1, NULL);
        }
        barrier_clients = get_children_number(connection, BARRIER_NODE, &clients);
    }

    printf("\n\n##### Saiu da barreira #####\n\n");
    return EXIT_SUCCESS;
}
