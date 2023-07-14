#include <stdio.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h> 
#include <pthread.h>
#include <string.h>

struct stat sb[100];
unsigned char *addr[100];
int args = 0;
pthread_mutex_t mutex; 
pthread_cond_t cond; 
char** results; 
pthread_mutex_t results_mutex;
pthread_cond_t results_cond; 
int chunk_size[250000];
int completed[250000] = {0};


int all_tasks_created = 0;


typedef struct task_node {
    int chunk_index; 
    int chunk_start; 
    int chunk_end; 
    unsigned char* chunk_data;
    struct task_node* next; 
} TaskNode; 

typedef struct task_queue{
    TaskNode* head; 
    TaskNode* tail; 
} TaskQueue; 


TaskQueue queue; 


void enqueue_task(TaskQueue* queue, int chunk_index, unsigned char* chunk_data, int chunk_start, int chunk_end) {
    TaskNode* new_node = malloc(sizeof(TaskNode));
    new_node->chunk_index = chunk_index;
    new_node->chunk_data = chunk_data; 
    new_node->chunk_start = chunk_start;
    new_node->chunk_end = chunk_end;
    new_node->next = NULL; 


    if (queue->head->next == NULL) {
        queue->head->next = new_node; 
        new_node->next = queue->tail; 
    }
    else{
        new_node->next = queue->head->next;
        queue->head->next = new_node; 
    } 

}

int isEmpty(TaskQueue* queue) {

    int t_f = (queue->head->next == queue->tail); 

    return t_f; 
}

TaskNode* dequeue_task(TaskQueue* queue) {


    TaskNode* node = queue->head->next; 
    queue->head->next = queue->head->next->next; 
    

    return node; 
}


void* worker_encode() {
    TaskNode* node; 
    for(;;) {
    
    pthread_mutex_lock(&mutex); 

        while(isEmpty(&queue)) {

        if(all_tasks_created) {
          pthread_mutex_unlock(&mutex);    
          return NULL;
        }
        pthread_cond_wait(&cond, &mutex);
        }

        node = dequeue_task(&queue);
        int string_index = node->chunk_index;
        results[string_index] = (char*)malloc((4096 * 2) * sizeof(char));
        pthread_mutex_unlock(&mutex);         

        unsigned char count = 1;
        int start = node->chunk_start;
        int end = node->chunk_end;
        int size = end - start; 

        
        unsigned char * addr = node->chunk_data;

    
        char* buffer = results[string_index];
        
        int index = 0;

        if(size == 1) {
            buffer[index++] = addr[0];
            buffer[index++] = count;
        }

        for(int i = start; i < end; i++) {

            if(addr[i] == addr[i + 1]) {
                    count++;
            }
            else {
                buffer[index++] = addr[i];
                buffer[index++] = count; 
                count = 1;
            }    
            if(i + 2 == end) {
                if(addr[i] == addr[i + 1]) {
                    buffer[index++] = addr[i];
                    buffer[index++] = count; 
                }
                else {
                    buffer[index++] = addr[i + 1];
                    buffer[index++] = count; 
                }
                break;
            }
        }
    pthread_mutex_lock(&results_mutex);
    chunk_size[string_index] = index;
    completed[string_index] = 1;
    pthread_cond_signal(&results_cond); 
    free(node); 
    pthread_mutex_unlock(&results_mutex);
    }
    
}


void encode() {
    //declare chars to hold the last char and count of that char of a file 
    unsigned char lastchar = '1'; 
    unsigned int lastcount; 

    //begins to process files 
    for(int i = 0; i < args; i++) {
        unsigned char count = 1;
        
        if(sb[i].st_size == 1) {
         if(lastchar != '1') {
            if(addr[i][0] == lastchar) {
                count = lastcount + 1; 
            }
        }
        lastchar = addr[i][0]; 
        lastcount = 1; 

         if(i == args - 1 ) {
            fwrite(&addr[i][0], 1, 1, stdout);
            fwrite(&count, 1, 1, stdout);
        }
        continue;
    }

     //checks last char value 
        if(lastchar != '1') {

            //if they are lastchar == first char of new file, update count
            if(addr[i][0] == lastchar) {
                count = lastcount + 1; 
            }
            else {
                //if not print it out 
                fwrite(&lastchar, 1,1, stdout);
                fwrite(&lastcount, 1,1, stdout);
            }
        }

    for(int m = 0; m < sb[i].st_size; m++) {

        //checks if the current char is equal to the next one 
        if(addr[i][m] == addr[i][m + 1]) {
                count++;
        }
        else {
            //if they aren't print out current count 
            fwrite(&addr[i][m], 1, 1, stdout);
            fwrite(&count, 1, 1, stdout);
            count = 1;
        }    
        
        //checks to see if its 2 chars before the end of stream 
         if(m + 2 == sb[i].st_size) {
            //this block of code is just to make sure you don't go beyond the number of chars

                //this means you're at the last 2 chars, so if they're equal just print them out 
            if(addr[i][m] == addr[i][m + 1]) {
                if(i == args - 1 ) {
                    fwrite(&addr[i][m], 1, 1, stdout);
                    fwrite(&count, 1, 1, stdout);
                }
                lastchar = addr[i][m];
            }
            else {
                //if they aren't equal then print out the last char
                if(i == args - 1 ) {
                    fwrite(&addr[i][m + 1], 1, 1, stdout);
                    fwrite(&count, 1, 1, stdout);
                }
                lastchar = addr[i][m + 1];
            }
            lastcount = count; 
            break;
        }
    }
    }
}


int main (int argc, char *argv[]) {

    int opt, thread_num = 0;
    char ** filenames = NULL;  

    extern char *optarg;
    extern int optind;

    while ((opt = getopt(argc, argv, "j:")) != -1) {
        if (opt == 'j') {
            thread_num = atoi(optarg);
            break;
        }
    }
    args = argc - optind;
    filenames = &argv[optind];

    int fd[args]; 

    //opens file descriptors
    for(int i = 0; i < args; i++) {
        fd[i] = open(filenames[i], O_RDONLY);
    }

    //gets the size of the file descriptors 
    for(int i = 0; i < args; i ++) {
         if (fstat(fd[i], &sb[i]) == -1)
             printf("error");
    }

    //maps these files to memory 
   
    for(int i = 0; i < args; i++) {
        addr[i] = mmap(NULL, sb[i].st_size, PROT_READ, MAP_PRIVATE, fd[i], 0);
    }
    pthread_t threads[thread_num];

    //start working 
     if(thread_num == 0) {
        encode();
    }
    else {
        //multi-threaded 
        results = (char**)malloc(250000 * sizeof(char*));
        pthread_mutex_init(&mutex, NULL); 
        pthread_cond_init(&cond, NULL);
        pthread_cond_init(&results_cond, NULL);
        pthread_mutex_init(&results_mutex, NULL);

        queue.head = malloc(sizeof(TaskNode));
        queue.tail = malloc(sizeof(TaskNode));
        queue.head->next = queue.tail;
        queue.tail->next = NULL;

        for(int i = 0; i < thread_num; i++) {
            pthread_create(&threads[i], NULL, worker_encode, NULL);
        }
        //enqueue_task(TaskQueue* queue, int chunk_index, char* chunk_data, int chunk_start, int chunk_end)
        
        //create tasks 
        int chunk_index = 0;
        int total_chunks = 0;
        for(int i = 0; i < args; i++) {
            int file_size = sb[i].st_size; 
            int chunks = file_size / 4096; 
            int remainder = file_size - (chunks * 4096);
            if(remainder != 0) {
                chunks++;
            }

            for(int m = 0; m < chunks; m++) {
                pthread_mutex_lock(&mutex);
                if(m == chunks - 1) {
                    enqueue_task(&queue, chunk_index++, addr[i], m * 4096, file_size);
                } 
                else {
                    enqueue_task(&queue, chunk_index++, addr[i], m * 4096, (m + 1) * 4096);
                }
                pthread_cond_broadcast(&cond);
                pthread_mutex_unlock(&mutex);
               
            }
            total_chunks += chunks; 
        }
        pthread_mutex_lock(&mutex);
        all_tasks_created = 1; 
        pthread_cond_broadcast(&cond); 
        pthread_mutex_unlock(&mutex);
      

        //chunk_size[i] = number chunk_size[i] - 1 = letter
        int i = 0;
        pthread_mutex_lock(&results_mutex);
        for(;;) {
            
            if(!completed[i]) {
                pthread_cond_wait(&results_cond, &results_mutex);
            }
            else {
                pthread_mutex_unlock(&results_mutex);
                if(i != 0) {
                    if(results[i][0] == results[i - 1][chunk_size[i - 1] - 2]) {
                        unsigned char count = results[i][1] + results[i - 1][chunk_size[i - 1] - 1];
                        results[i][1] = count; 
                    } else {
                        fwrite(&results[i - 1][chunk_size[i - 1] - 2], 1, 1, stdout);
                        fwrite(&results[i - 1][chunk_size[i - 1] - 1], 1, 1, stdout);
                    }
                }
                fwrite(results[i], 1, chunk_size[i] - 2, stdout);
                i++; 
                pthread_mutex_lock(&results_mutex);
            }  

            if(total_chunks == i) {
                fwrite(&results[i - 1][chunk_size[i - 1] - 2], 1, 1, stdout);
                fwrite(&results[i - 1][chunk_size[i - 1] - 1], 1, 1, stdout);
                break; 
            } 
        }
        
        pthread_mutex_unlock(&results_mutex);

       

        for(int i = 0; i < thread_num; i++) {
            pthread_join(threads[i], NULL);
    }

    free(queue.head);
    free(queue.tail);
    pthread_mutex_destroy(&mutex); 
    pthread_cond_destroy(&cond);
    pthread_cond_destroy(&results_cond);
    pthread_mutex_destroy(&results_mutex);


    }   

     for(int i = 0; i < args; i++) {
       munmap(addr[i], sb[i].st_size);
       close(fd[i]);
    }

    return 0; 
}