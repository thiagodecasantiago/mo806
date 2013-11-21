#include <zookeeper.h>
#include <stdio.h>
#include <string.h>

static zhandle_t *zh; /* representa a conexão com o servidor */
static clientid_t myid; /* identificação do cliente */
static struct Stat stat;

void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *context);

int main(int argc, char *argv[])
{
    char server_address[256];
    char ephemeral_path[256];
    int rc;

    /* verificação básica dos argumentos */
    if (argc != 2) {
        fprintf(stderr, "Uso: ./program <endereço do servidor>\n");
        exit(EXIT_FAILURE);
    }

    /* Primeiro argumento é o endereço do servidor */
    strcpy(server_address, argv[1]);

    /* Inicializa a conexão com o servidor */
    if ((zh = zookeeper_init(server_address, watcher, 10000, &myid, NULL, 0)) == NULL) {
        fprintf(stderr, "Não foi possível conectar-se ao servidor %s\n", server_address);
        exit(EXIT_FAILURE);
    } else {
        fprintf(stdout, "Conectado à %s\n", server_address);
    }
    fprintf(stdout, "Myid: %d\n", myid.client_id);
    sprintf(ephemeral_path, "/fase1/cliente-%d", myid.client_id);

    /* Cria um nó comum */
    zoo_create(zh, "/fase1", NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0,
               NULL, 0);
    /* Cria um nó efêmero */
    rc = zoo_create(zh, ephemeral_path, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL,
                    NULL, 0);
    /* seta um watch em um arquivo criado por um processo diferente */
    zoo_exists(zh, "/fase1/cliente-1", 1, &stat);

    while(1) {
        /* espera até que o watcher dispare */
    }

    /* não deve chegar aqui nunca */
    zookeeper_close(zh);
    return EXIT_SUCCESS;
}

void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *context)
{
    /* quando um nó é deletado, verifica se é o nó cliente-1 (segundo cliente),
     * se for, sai também */
    if (type == ZOO_DELETED_EVENT && !strcmp(path, "/fase1/cliente-1")) {
        fprintf(stdout, "cliente-1 desconectou, saindo...\n");
        zookeeper_close(zh);
        exit(EXIT_SUCCESS);
    } else {
        /* caso o nó deletado não seja o esperado, reativa o watcher */
        zoo_exists(zh, "/fase1/cliente-1", 1, &stat);
    }

    return;
}
