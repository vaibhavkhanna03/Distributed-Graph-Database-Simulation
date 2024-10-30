#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>

#define MAX_FILE_NAME 256
#define MAX_NODES 100

struct RequestMessage {
    long mtype;  
    int sequence_no; 
    int op;
    char file_name[MAX_FILE_NAME];
};

struct ResponseMessage {
    long mtype; 
    char data[100]; 
    int vertices[MAX_NODES]; 
};

int main() {
    key_t key;  
    int msgQueueId; 
    if((key = ftok("load_balancer.c", 'A')) == -1){ // Isme A hai, dekhna hai
        perror("Error in ftok");
        exit(1);
    }
    if((msgQueueId = msgget(key, 0666 | IPC_CREAT)) == -1){
        perror("Error in msgget");
        exit(1);
    }     

    while (1) {
        struct RequestMessage request;
        struct ResponseMessage response;
        request.mtype = 1;

        printf("1. Add a new graph to the database\n2.  Modify an existing graph of the database\n3.  Perform DFS on an existing graph of the database\n4.  Perform BFS on an existing graph of the database\n");
        printf("Enter Sequence Number: ");
        scanf("%d", &request.sequence_no);  

        printf("Enter Operation Number: ");
        scanf("%d", &request.op);

        printf("Enter Graph File Name: ");
        scanf("%s", request.file_name);

        int op = request.op;

        key_t shared_key = ftok("client.c", request.sequence_no);
        int shmid= shmget(shared_key, (MAX_NODES * MAX_NODES + 1) * sizeof(int), 0666 | IPC_CREAT);

        if(shmid == -1){
            perror("shmget");
            exit(1);
        }

        int *sharedData = (int *)shmat(shmid, NULL, 0);
        if((intptr_t)sharedData == -1){
            perror("shmat");
            exit(1);
        }

        if(op == 1 || op == 2){
            int n;
            printf("\n\nEnter the number of nodes in the graph: ");
            scanf("%d", &n);
            
            sharedData[0] = n;
            printf("\nEnter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters\n\n");
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    scanf("%d", &sharedData[i * n + j + 1]);
                }
            }
        }
        else if(op == 3 || op == 4){
            int start;
            printf("Enter starting vertex: ");
            scanf("%d", &start);

            sharedData[0] = start ;
        }

        if(shmdt(sharedData) == -1){
                perror("shmdt");
                exit(1);
        }

        if(msgsnd(msgQueueId, &request, sizeof(struct RequestMessage) - sizeof(long), 0) == -1){
            perror("msgsnd");
            exit(1);
        }
        //2 is mtypr to catch that specific type of response only
        if(msgrcv(msgQueueId, &response, sizeof(struct ResponseMessage) - sizeof(long), 2, 0) == -1){
            perror("msgrcv");
            exit(1);
        }

        printf("\n");
        printf("%s ", response.data);

        if(op==3){
    
        for(int i = 0 ; i<100 ;i++){
            if(response.vertices[i]==1)
            printf("%d ",i);
        }
        

        }else if(op==4){
            int i =0;
            while(response.vertices[i]!=-1){
                printf("%d ",response.vertices[i] );
                i++;
            }
        }
        printf("\n\n");

        if (shmctl(shmid, IPC_RMID, NULL) == -1){
            perror("shmctl");
            exit(1);
        }
    }

    return 0;
}

// ipcs -q | awk '$6 ~ /^[0-9]/ {print $2}' | xargs ipcrm msg : Remove all the existing message queues
