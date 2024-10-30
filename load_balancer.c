#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include<semaphore.h>
#include<fcntl.h>

#define PRIMARY_SERVER 111
#define SECONDARY_SERVER_1 222
#define SECONDARY_SERVER_2 333

#define MAX_FILE_NAME 256

char names[3][20][2]={{"aa", "ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am","an","ao","ap","aq","ar","as","at"},
                    {"ba", "bb","bc","bd","be","bf","bg","bh","bi","bj","bk","bl","bm","bn","bo","bp","bq","br","bs","bt"},
                    {"ca", "cb","cc","cd","ce","cf","cg","ch","ci","cj","ck","cl","cm","cn","co","cp","cq","cr","cs","ct"}};

sem_t*mut[20];
sem_t*in_mut[20];
sem_t*rw_mut[20];

struct message{
    long mtype;    
    int sequence_no; 
    int operation;
    char file_name[MAX_FILE_NAME];
};


int main() {
    key_t key;
    int msgid;

    for(int i = 0 ; i < 20 ; i++){
        mut[i] = sem_open(names[0][i], O_CREAT | O_EXCL, 0644, 1);
        in_mut[i] = sem_open(names[1][i], O_CREAT | O_EXCL, 0644, 1);
        rw_mut[i] = sem_open(names[2][i], O_CREAT | O_EXCL, 0644, 1);
    }

    if ((key = ftok("load_balancer.c", 'A')) == -1) {
        perror("ftok");
        exit(1);
    }

    if ((msgid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }

    while (1) {
        struct message msg;
        //Catch message of mtype = 1
        if (msgrcv(msgid, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        if(msg.operation==5){
            //Shut down servers
            msg.mtype=111;
            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) ;
            msg.mtype=222;
            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) ;
            msg.mtype=333;
            msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) ;

            sleep(5); //Wait for all the servers to shut down
            break;
        }
        
        long destination_server;
        if (msg.operation == 1 || msg.operation == 2) {
            destination_server = PRIMARY_SERVER;
        } else {
            if (msg.sequence_no % 2 == 1) {
                destination_server = SECONDARY_SERVER_1;
            } else {
                destination_server = SECONDARY_SERVER_2;
            }
        }

        msg.mtype = destination_server;
        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }

    }

    for(int i = 0 ; i < 20 ; i++){
        sem_close(mut[i]);
        sem_close(in_mut[i]);
        sem_close(rw_mut[i]);
    }

    for(int i = 0 ; i < 20 ; i++){
        sem_unlink(names[0][i]);
        sem_unlink(names[1][i]);
        sem_unlink(names[2][i]);
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
    

    return 0;
}
