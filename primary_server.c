#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>
#include<semaphore.h>
#include <unistd.h>
#include<fcntl.h>


#define MAX_NODES 100
#define MAX_FILE_NAME 255

char names[3][20][2]={{"aa", "ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am","an","ao","ap","aq","ar","as","at"},
                    {"ba", "bb","bc","bd","be","bf","bg","bh","bi","bj","bk","bl","bm","bn","bo","bp","bq","br","bs","bt"},
                    {"ca", "cb","cc","cd","ce","cf","cg","ch","ci","cj","ck","cl","cm","cn","co","cp","cq","cr","cs","ct"}};

sem_t*mut[20];
sem_t*in_mut[20];
sem_t*rw_mut[20];

struct RequestMessage {
    long mtype;
    int sequence_no;
    int op;
    char file_name[MAX_FILE_NAME];
};

struct ResponseMessage {
    long mtype;
    char response[100];
    int vertices[MAX_NODES]; 
};

struct ThreadArgs {
    struct RequestMessage request;
    int msgid;
    int shmid;
};

int graphIdx(const char* filename){
    char temp[3] = {'\0', '\0', '\0'};
    int i = 1;
    while(filename[i] != '.'){
        temp[i-1] = filename[i];
        i++;
    }
    return atoi(temp) - 1;
}

void *handle_request(void *arg) {
    
    struct RequestMessage request = ((struct ThreadArgs *)arg)->request;
    int msg_id = ((struct ThreadArgs *)arg)->msgid;
    int shm_id = ((struct ThreadArgs *)arg)->shmid;
    
    int *shared_memory = (int *)shmat(shm_id, NULL, 0);

    struct ResponseMessage response;
    response.mtype = 2;

    int gIdx = graphIdx(request.file_name);
    sem_wait(in_mut[gIdx]);
    sem_wait(rw_mut[gIdx]);



    FILE *file;
    if (request.op == 1 || request.op == 2) {
        file = fopen(request.file_name, "w");
    }else{
        perror("Invalid operation type");
        exit(EXIT_FAILURE);
    }

    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    int nodes = shared_memory[0];
    fprintf(file, "%d\n", nodes);
    for (int i = 0; i < nodes; ++i) {
        for (int j = 0; j < nodes; ++j) {
            fprintf(file, "%d ", (char)shared_memory[1 + i * nodes + j]);
        }
        fprintf(file, "\n");  
    }
    fclose(file);


    sem_post(rw_mut[gIdx]);
    sem_post(in_mut[gIdx]);


    if(request.op == 1) strcpy(response.response, "File added successfully");

    else if(request.op == 2) strcpy(response.response, "File modified successfully");
    
    if (msgsnd(msg_id, &response, sizeof(struct ResponseMessage) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }
    shmdt(shared_memory);
    free(arg);
    pthread_exit(NULL);
}

int main() {
    key_t msq_key = ftok("load_balancer.c", 'A');
    int msg_id = msgget(msq_key, 0666);

    for(int i = 0 ; i < 20 ; i++){
        mut[i] = sem_open(names[0][i], O_EXCL, 0644, 1);
        in_mut[i] = sem_open(names[1][i], O_EXCL, 0644, 1);
        rw_mut[i] = sem_open(names[2][i], O_EXCL, 0644, 1);
    }

    while (1) {
        struct RequestMessage request;
        //mtype = 111
        msgrcv(msg_id, &request, sizeof(struct RequestMessage) - sizeof(long), 111, 0);

        if(request.op==5){ //Shut downm
            break;
        }
        key_t shared_key = ftok("client.c", request.sequence_no);
        int shm_id = shmget(shared_key, (MAX_NODES * MAX_NODES + 1) * sizeof(int), 0666);

        struct ThreadArgs *args = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
        args->shmid = shm_id;
        args->msgid = msg_id;
        args->request = request;
        pthread_t tid;
        
        pthread_create(&tid, NULL, handle_request, (void *)args);
        
        pthread_join(tid, NULL);

        printf("Sequence Number %d handled by primary server",request.sequence_no);
        fflush(stdout);
    }

    for(int i = 0 ; i < 20 ; i++){
        sem_close(mut[i]);
        sem_close(in_mut[i]);
        sem_close(rw_mut[i]);
    }
    
    return 0;
}
