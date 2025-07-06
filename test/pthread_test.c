#include <stdio.h>
#include <pthread.h>

void *myThreadFun(void *vargp) {
    printf("Hello from thread!\n");
    return NULL;
}

int main() {
    pthread_t thread_id;
    printf("Before Thread\n");
    pthread_create(&thread_id, NULL, myThreadFun, NULL);
    pthread_join(thread_id, NULL);
    printf("After Thread\n");
    return 0;
}

