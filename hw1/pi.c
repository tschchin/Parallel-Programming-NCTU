#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

unsigned long long rounds_per_thread;
unsigned long long number_in_circle;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* toss(){
    unsigned long long number_in_circle_in_a_thread=0;

    unsigned int seed =  time(NULL);

    double x, y;

    // calculate points in thread
    for(unsigned long long i=0; i<rounds_per_thread; i++){
        x = 2*  (double)rand_r(&seed)/RAND_MAX - 1;
        y = 2*  (double)rand_r(&seed)/RAND_MAX - 1;

	// if in circle, number_in_circle_in_a_thread++
        if (x*x + y*y <= 1)
            number_in_circle_in_a_thread++;
    }

    // add to global var
    pthread_mutex_lock(&mutex);
    number_in_circle += number_in_circle_in_a_thread;
    pthread_mutex_unlock(&mutex);

    return NULL;
}
int main(int argc, char **argv)
{
    double pi_estimate;
    unsigned long long  number_of_cpu, number_of_tosses;
    if ( argc < 2) {
        exit(-1);
    }
    number_of_cpu = atoi(argv[1]);
    number_of_tosses = atoi(argv[2]);
    if (( number_of_cpu < 1) || ( number_of_tosses < 0)) {
        exit(-1);
    }

    pthread_t *threads;
    threads = malloc(number_of_cpu*sizeof(pthread_t));

    rounds_per_thread = number_of_tosses / number_of_cpu;

    pthread_mutex_init(&mutex, NULL);

    // distribute works to threads
    for (int i=0; i < number_of_cpu; i++) {
        pthread_create(&threads[i], NULL, toss, NULL);
    }

    // wait for all threads
    for (int i=0; i < number_of_cpu; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    free(threads);

    pi_estimate = 4*number_in_circle/((double) number_of_tosses);

    printf("%f\n",pi_estimate);

    return 0;
}