#include <stdio.h>
#include <mpi.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

#define REQUEST 400
#define RE_REQUEST 500
#define REPLY 600
#define RELEASE 700

#define DEBUG (FALSE)

#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct entry
{
    int rank;
    long timestamp;
    int num_procs_replied;
    int needs_new_request;
} entry_t;

typedef struct queue
{
    struct entry **array;
    int size;
} queue_t;

struct entry **all_entries;

int mpi_rank, mpi_size;
long timestamp = 0, max_timestamp = 0;
entry_t *my_request = NULL;
queue_t *request_queue = NULL;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

queue_t *queue_create(int capacity)
{
    queue_t *queue = malloc(sizeof(queue_t));
    if (queue == NULL)
    {
        fprintf(stderr, "queue_init: Unable to allocate memory for queue\n");
        return NULL;
    }

    queue->size = 0;
    queue->array = calloc(capacity, sizeof(entry_t *));
    if (queue->array == NULL)
    {
        fprintf(stderr, "queue_init: Unable to allocate memory for queue array\n");
        free(queue);
        return NULL;
    }

    return queue;
}

void queue_destroy(queue_t *queue)
{
    free(queue->array);
    free(queue);
}

void queue_add(queue_t *queue, entry_t *entry)
{
    int index_insert = queue->size;
    for (int i = 0; i < queue->size; i++)
    {
        if (queue->array[i]->timestamp > entry->timestamp)
        {
            index_insert = i;
            break;
        }
    }

    if (queue->size > index_insert)
        memmove(queue->array + index_insert + 1, queue->array + index_insert, (queue->size - index_insert) * sizeof(entry_t *));

    queue->array[index_insert] = entry;
    queue->size++;

    entry->num_procs_replied = 0;
}

void queue_remove(queue_t *queue, entry_t *entry)
{
    int idx = -1;
    for (int i = 0; i < queue->size; i++)
    {
        if (queue->array[i] == entry)
        {
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        fprintf(stderr, "queue_remove: %d - didn't find entry for %d\n", mpi_rank, entry->rank);
        return;
    }

    memmove(queue->array + idx, queue->array + idx + 1, (queue->size - idx - 1) * sizeof(entry_t *));
    queue->size--;
}

void send_message_to_other_procs(int tag, long msg)
{
    MPI_Request requests[mpi_size];
    for (int i = 0; i < mpi_size; i++)
    {
        if (i == mpi_rank)
            continue;

        MPI_Isend(&msg, 1, MPI_LONG, i, tag, MPI_COMM_WORLD, &requests[i]);
    }
}

void *receiver(void *param)
{
    while (TRUE)
    {
        long recv_timestamp;
        MPI_Status status;
        MPI_Recv(&recv_timestamp, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        int source = status.MPI_SOURCE, tag = status.MPI_TAG;

        if (DEBUG)
            fprintf(stderr, "%d - recv %d from %d\n", mpi_rank, tag, source);

        if (source == mpi_rank)
            continue;

        if (tag == RELEASE)
        {
            pthread_mutex_lock(&mutex);

            max_timestamp = MAX(max_timestamp, recv_timestamp);

            queue_remove(request_queue, all_entries[source]);

            if (my_request->needs_new_request)
            {
                long msg = timestamp;
                send_message_to_other_procs(RE_REQUEST, msg);
                my_request->needs_new_request = FALSE;
                my_request->num_procs_replied = 0;
            }

            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&cond);
        }
        else if (tag == REQUEST || tag == RE_REQUEST)
        {
            entry_t *entry = all_entries[source];
            entry->timestamp = recv_timestamp;

            pthread_mutex_lock(&mutex);

            max_timestamp = MAX(max_timestamp, recv_timestamp);
            if (tag == REQUEST)
            {
                queue_add(request_queue, entry);
            }

            pthread_mutex_unlock(&mutex);

            clock_t msg = timestamp;
            MPI_Request request;
            MPI_Isend(&msg, 1, MPI_LONG, source, REPLY, MPI_COMM_WORLD, &request);
        }
        else if (tag == REPLY)
        {
            int should_signal = FALSE;

            pthread_mutex_lock(&mutex);

            max_timestamp = MAX(max_timestamp, recv_timestamp);

            if (recv_timestamp > timestamp || (recv_timestamp == timestamp && source < mpi_rank))
                my_request->num_procs_replied++;
            else
                my_request->needs_new_request = TRUE;

            if (my_request->num_procs_replied >= mpi_size - 1)
                should_signal = TRUE;

            pthread_mutex_unlock(&mutex);

            if (should_signal)
                pthread_cond_signal(&cond);
        }
        else
            fprintf(stderr, "receiver: received invalid tag\n");
    }

    return NULL;
}

int init()
{
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    all_entries = calloc(mpi_size, sizeof(entry_t *));
    if (all_entries == NULL)
    {
        fprintf(stderr, "init: Unable to allocate memory for all_entries\n");
        return -1;
    }

    for (int i = 0; i < mpi_size; i++)
    {
        entry_t *entry = calloc(1, sizeof(entry_t));
        if (entry == NULL)
        {
            fprintf(stderr, "init: Unable to allocate memory for entry\n");
            return -1;
        }

        entry->rank = i;
        all_entries[i] = entry;
    }

    request_queue = queue_create(mpi_size * 2);
    if (request_queue == NULL)
        return -1;

    my_request = all_entries[mpi_rank];
    my_request->needs_new_request = TRUE;
    my_request->num_procs_replied = 0;

    //timestamp = mpi_rank;
    return 0;
}

void cleanup()
{
    queue_destroy(request_queue);
    for (int i = 0; i < mpi_size; i++)
    {
        free(all_entries[i]);
    }
    free(all_entries);
}

void lock_print()
{
    pthread_mutex_lock(&mutex);

    my_request->timestamp = ++timestamp;
    queue_add(request_queue, my_request);

    long msg = timestamp;
    send_message_to_other_procs(REQUEST, msg);
    my_request->num_procs_replied = 0;
    my_request->needs_new_request = FALSE;

    if (DEBUG)
        fprintf(stderr, "%d - lock request\n", mpi_rank);

    while (request_queue->array[0] != my_request || my_request->num_procs_replied < mpi_size - 1)
    {
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex);
}

void unlock_print()
{
    long msg = timestamp;

    pthread_mutex_lock(&mutex);

    queue_remove(request_queue, my_request);
    timestamp = MAX(timestamp, max_timestamp + 1);

    pthread_mutex_unlock(&mutex);

    send_message_to_other_procs(RELEASE, msg);

    if (DEBUG)
        fprintf(stderr, "%d - release request\n", mpi_rank);
}

int main(int argc, char **argv)
{
    int desired = MPI_THREAD_MULTIPLE, provided;
    MPI_Init_thread(&argc, &argv, desired, &provided);
    if (desired != provided)
        fprintf(stderr, "MPI_Init_thread: requested %d, got %d\n", desired, provided);

    if (init() == -1)
    {
        cleanup();
        MPI_Finalize();
        return 0;
    }

    pthread_t recv_thread;
    int errCode = pthread_create(&recv_thread, NULL, receiver, NULL);
    if (errCode != 0)
    {
        fprintf(stderr, "Unable to create thread: %s\n", strerror(errCode));
        cleanup();
        MPI_Finalize();
        return 0;
    }

    while (TRUE)
    {
        lock_print();
        printf("Message from %d\n", mpi_rank);
        //usleep(10000);
        unlock_print();
    }

    cleanup();
    MPI_Finalize();
    return 0;
}
