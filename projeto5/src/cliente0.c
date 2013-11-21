#include "barrier.h"
#include <string.h>

static zhandle_t *zh; /* representa a conexão com o servidor */
static char *client_id;

//static int barrierflag = 1;

void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *context);

int main(int argc, char *argv[])
{
    struct Stat stat;
    char server_address[256];
    char ephemeral_path[256];
    int i;

    /* verificação básica dos argumentos */
    if (argc != 2) {
        fprintf(stderr, "Uso: ./program <endereço do servidor>\n");
        exit(EXIT_FAILURE);
    }
    /* ultra migué pra retirar o ./ do comando */
    client_id = argv[0] +2;
    fprintf(stderr, "\n\n#### client_id: %s\n\n", client_id);

    /* Primeiro argumento é o endereço do servidor */
    strcpy(server_address, argv[1]);

    /* Inicializa a conexão com o servidor */
    if ((zh = zookeeper_init(server_address, watcher, 10000, NULL, NULL, 0)) == NULL) {
        fprintf(stderr, "Não foi possível conectar-se ao servidor %s\n", server_address);
        exit(EXIT_FAILURE);
    } else {
        fprintf(stdout, "Conectado à %s\n", server_address);
    }

    /* Inicializa a barreira, se ainda não existir */
    if (zoo_exists(zh, BARRIER_NODE, 1, &stat) != ZOK) {
        init_barrier(zh);
        fprintf(stderr, "\n\n##### Inicializou barreira #####\n\n");
    }
    /* O 3 (último argumento) é o tamanho da barreira */
    for (i=0; i<5; i++){
	enter_barrier(zh, client_id, 2);	
	//while (barrierflag == 1){
	/* Processa algo */
	//}
	if (i == 3) printf("~~~~~~Essa eh a minha terceira vez computando algo.\n");
	else if (i == 4) {
	    printf("~~~~~~Na quarta, eu durmo por 5 segundos.\n");
	    sleep(5);
	    printf("acabou o sleep\n");
	}
	leave_barrier(zh, client_id);
	//barrierflag = 1;
    }
    //printf("\n\n##### Saiu da barreira #####\n\n");

    zookeeper_close(zh);
    return EXIT_SUCCESS;

}

void watcher(zhandle_t *zzh, int type, int state, const char *path,
             void *context)
{
    if (type == ZOO_CREATED_EVENT && !strcmp(path, BARRIER_READY_NODE)) {
        fprintf(stderr, "\n\n#### watcher ###\n\n");
        //leave_barrier(zzh, client_id);
	pthread_mutex_lock(&barrier_mutex);
        pthread_cond_signal(&barrier_varcond);
	pthread_mutex_unlock(&barrier_mutex);
	/* Abaixa a flag */
	//barrierflag = 0;
    }
	else if (type == ZOO_DELETED_EVENT && !strcmp(path, BARRIER_READY_NODE)) {
	/* Abaixa a flag */
	printf("/ready foi deletado\n");
	pthread_mutex_lock(&crowded_mutex);
        pthread_cond_signal(&crowded_varcond);
	pthread_mutex_unlock(&crowded_mutex);
	crowdedflag = 0;
    }
    /* reseta o watch da barreira */
    zoo_exists(zzh, BARRIER_READY_NODE, 1, NULL);
    return;
}
