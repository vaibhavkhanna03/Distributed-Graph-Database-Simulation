#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // For shared memory and IPC
#include <sys/ipc.h> // For shared memory and IPC
#include <sys/shm.h> // For shared memory
#include <sys/msg.h> // For message queue
#include <string.h>  // For string operations
#include<semaphore.h>
#include<fcntl.h>

#define MAX_NODES 100
#define MAX_FILE_NAME 255

// Define structures for message queues and shared memory data
struct RequestMessage {
    long mtype;
    int sequence_no;
    int op;
    char file_name[MAX_FILE_NAME];
};


char names[3][20][2]={{"aa", "ab","ac","ad","ae","af","ag","ah","ai","aj","ak","al","am","an","ao","ap","aq","ar","as","at"},
                    {"ba", "bb","bc","bd","be","bf","bg","bh","bi","bj","bk","bl","bm","bn","bo","bp","bq","br","bs","bt"},
                    {"ca", "cb","cc","cd","ce","cf","cg","ch","ci","cj","ck","cl","cm","cn","co","cp","cq","cr","cs","ct"}};
sem_t*mut[20];
sem_t*in_mut[20];
sem_t*rw_mut[20];
int counter[20];

struct ResponseMessage {
    long mtype;
    char response[100];
    int vertices[MAX_NODES];
};

struct ThreadArgs {
    struct RequestMessage request;
    int msgid;
    int shmid;
    int* results;
    pthread_mutex_t results_mutex;
    int** graph;
    int num_nodes;
    int starting_node;
    int current_node;
    
};

pthread_mutex_t visit_mutex;
int visited[MAX_NODES];
int queue[MAX_NODES], front = 0, rear = 0;
pthread_mutex_t queue_mutex;

void* dfs(void* arg);

int graphIdx(const char* filename){
    char temp[3] = {'\0', '\0', '\0'};
    int i = 1;
    while(filename[i] != '.'){
        temp[i-1] = filename[i];
        i++;
    }
    return atoi(temp) - 1;
}

void readGraphFromFile(const char* filename, int*** graph, int* num_nodes) {
    int gIdx = graphIdx(filename);

    
    fflush(stdout);

    sem_wait(in_mut[gIdx]);
    sem_wait(mut[gIdx]);
    counter[gIdx]++;
    if(counter[gIdx] == 1) {
        sem_wait(rw_mut[gIdx]);
    }

    sem_post(mut[gIdx]);
    sem_post(in_mut[gIdx]);


    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Unable to open file");
        return;
    }

    fscanf(file, "%d", num_nodes);

    *graph = (int**)malloc((*num_nodes) * sizeof(int*));
    for (int i = 0; i < *num_nodes; ++i) {
        (*graph)[i] = (int*)malloc((*num_nodes) * sizeof(int));
        for (int j = 0; j < *num_nodes; ++j) {
            fscanf(file, "%d", &((*graph)[i][j]));
            
        }
    }
    
    fflush(stdout);

    
    fclose(file);

    sem_wait(mut[gIdx]);
    counter[gIdx]--;
    if(counter[gIdx] == 0){
        sem_post(rw_mut[gIdx]);
    }
    sem_post(mut[gIdx]);


}

void processDFS(int startVertex, void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    args->starting_node = startVertex;
    readGraphFromFile(args->request.file_name, &(args->graph), &(args->num_nodes));
    memset(visited, 0, sizeof(visited));  // Reset visited array
    args->results = malloc(sizeof(int) * args->num_nodes);  // num_nodes should be the total number of nodes in the graph
    if (args->results == NULL) {
        // Handle memory allocation failure
        perror("Failed to allocate memory for results");
        exit(EXIT_FAILURE);
    }
    memset(args->results, 0, sizeof(int) * args->num_nodes); 

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, dfs, arg);
    pthread_join(thread_id, NULL);

    struct ResponseMessage response;
    response.mtype = 2;
    memcpy(response.vertices, args->results, args->num_nodes * sizeof(int)); 
    strcpy(response.response, "Leaf nodes:");
    msgsnd(args->msgid, &response, sizeof(response) - sizeof(long), 0);
    free(args->results); 
}

void* dfs(void* arg) {
    
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    int node = args->starting_node;

    pthread_mutex_lock(&visit_mutex);
    if (visited[node]) {
        pthread_mutex_unlock(&visit_mutex);
        return NULL;
    }
    visited[node] = 1;
    pthread_mutex_unlock(&visit_mutex);

    
    int child_count = 0;
    for (int neighbor = 0; neighbor < args->num_nodes; neighbor++) {
        if (args->graph[node][neighbor] == 1 && !visited[neighbor]) {
            child_count=1;
            pthread_t childThread;
            struct ThreadArgs child_arg = *args;
            child_arg.starting_node = neighbor;
            pthread_create(&childThread, NULL, dfs, &child_arg);
            pthread_join(childThread,NULL);
        }
    }

    
    if (!child_count) {
    pthread_mutex_lock(&args->results_mutex);
    args->results[node] = 1;  // Mark this node as visited in the results array
    pthread_mutex_unlock(&args->results_mutex);
    }

    return NULL;
}

void* processNodeBFS(void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    int node = args->current_node;
    int** graph = args->graph;

    // Add unvisited adjacent nodes to the queue
    for (int i = 0; i < args->num_nodes; ++i) {
        if (graph[node][i] && !visited[i]) {
            pthread_mutex_lock(&queue_mutex);
            if (!visited[i]) {
                visited[i] = 1;
                queue[rear++] = i;
            }
            pthread_mutex_unlock(&queue_mutex);
        }
    }

    free(arg);  // Free the dynamically allocated ThreadArgs
    return NULL;
}

void processBFS(int startVertex, void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    args->starting_node = startVertex;
    readGraphFromFile(args->request.file_name, &(args->graph), &(args->num_nodes));

    // Initialize BFS-related data
    memset(visited, 0, sizeof(visited));  // Reset visited array
    
    int bfsTraversal[MAX_NODES], bfsIndex = 0;  // Store BFS traversal order
    // memset(bfsTraversal,-1, sizeof(bfsTraversal));
    for (int i = 0; i < MAX_NODES; i++) {
        bfsTraversal[i] = -1;
    }
    // Add starting vertex to the queue
    queue[rear++] = startVertex;
    visited[startVertex] = 1;

    pthread_mutex_t queue_mutex;
    pthread_mutex_init(&queue_mutex, NULL);

    while (front < rear) {
        int levelSize = rear - front;
        pthread_t threads[levelSize];

        for (int i = 0; i < levelSize; ++i) {
            struct ThreadArgs* threadArgs = malloc(sizeof(struct ThreadArgs));
            *threadArgs = *args;  // Copy the contents of parent args
            threadArgs->current_node = queue[front];
            bfsTraversal[bfsIndex++] = queue[front++];  // Store node in BFS order
            pthread_create(&threads[i], NULL, processNodeBFS, threadArgs);
        }

        // Wait for all threads in this level to complete
        for (int i = 0; i < levelSize; ++i) {
            pthread_join(threads[i], NULL);
        }

        // Next level nodes are added to the queue in processNodeBFS
    }

    pthread_mutex_destroy(&queue_mutex);

    // Send the BFS traversal result back to the client
    struct ResponseMessage response;
    response.mtype = 2;  // Set appropriate message type
    strcpy(response.response, "BFS Traversal:");
    memcpy(response.vertices, bfsTraversal, MAX_NODES * sizeof(int)); 
    
    msgsnd(args->msgid, &response, sizeof(response) - sizeof(long), 0);
}

void* handle_request(void* arg) {
    struct ThreadArgs* args = (struct ThreadArgs*)arg;
    struct RequestMessage* request = &(args->request);
    int msg_id = args->msgid;
    int shm_id = args->shmid;

    int* shared_memory = (int*)shmat(shm_id, NULL, 0);

    if (request->op == 3) {
        processDFS(shared_memory[0], arg);
    }else if(request->op == 4){
        processBFS(shared_memory[0], arg);
    } 
    else  {
        perror("Invalid operation type");
        exit(EXIT_FAILURE);
    }
    return NULL;
}

int main(int argc, char *argv[]) { 

    for(int i = 0 ; i < 20 ; i++){
        mut[i] = sem_open(names[0][i], O_EXCL, 0644, 1);
        in_mut[i] = sem_open(names[1][i], O_EXCL, 0644, 1);
        rw_mut[i] = sem_open(names[2][i], O_EXCL, 0644, 1);
    }
 
    key_t msq_key = ftok("load_balancer.c", 'A');
    
    int msg_id = msgget(msq_key, 0666 );
    

    struct RequestMessage request;
    int serverno= atoi(argv[1]);
    printf("This is Secondary Server %d\n",serverno);
    fflush(stdout);

    while (1) {
       
       if(serverno==1){
        
            msgrcv(msg_id, &request, sizeof(struct RequestMessage) - sizeof(long),222,0);
       }
       else{
            msgrcv(msg_id, &request, sizeof(struct RequestMessage) - sizeof(long),333,0);
       }

       if(request.op==5){
            break;
        }
        

        key_t shared_key = ftok("client.c", request.sequence_no);
        int shm_id = shmget(shared_key, (MAX_NODES * MAX_NODES + 1) * sizeof(int), 0666 );
        struct ThreadArgs* args = malloc(sizeof(struct ThreadArgs));
        args->shmid = shm_id;
        args->msgid = msg_id;
        args->request = request;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_request, (void*)args);
        pthread_detach(tid);

        printf("Sequence number %d handled by secondary server %d\n", request.sequence_no, serverno);
        fflush(stdout);
    }

    for(int i = 0 ; i < 20 ; i++){
        sem_close(mut[i]);
        sem_close(in_mut[i]);
        sem_close(rw_mut[i]);
    }
    // Cleanup code here (if server is ever supposed to stop)
    // shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}
