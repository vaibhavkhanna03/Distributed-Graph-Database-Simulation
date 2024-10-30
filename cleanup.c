#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<string.h>
#include<sys/msg.h>

#define ARG_LEN 500
#define PERMS 0666
#define MAX_FILE_NAME 256
struct message{
    long mtype;    
    int sequence_no; 
    int operation;
    char file_name[MAX_FILE_NAME];
};

int main(){
        struct message msg;
        msg.mtype=1;
        msg.operation=5;
        key_t key;
        int msgqid;
        if((key = ftok("load_balancer.c",'A'))==-1){
                perror("Error in ftok");
                exit(1);
        }
        if((msgqid = msgget(key,PERMS))==-1){
                perror("Error in msgget");
                exit(1);
        }
	while (1) {
		char input;
     	   	printf("Do you want the server to terminate? Press Y for Yes and N for No: ");
		scanf("%c",&input);
		getchar();

        	if (input == 'Y') {
        	    printf("Cleanup process will now terminate.\n");
        	    break;
        	}
		else
			continue;
       }
	// Notifying the server by sending message through message queue

                if (msgsnd(msgqid, &msg,  sizeof(msg)- sizeof(long) , 0) == -1) {
                    perror("msgsnd");
                    exit(1);
                }


	return 0;
}
