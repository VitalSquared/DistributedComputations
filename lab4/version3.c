//
// Created by vital on 06-Apr-23.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include "netutils.h"
#include "list.h"

#define BUF_SIZE 1024
#define TIMEOUT_S 10
#define MAX_POOL_SIZE 100
#define IS_POOL_SIZE_VALID(S) (1 <= (S) && (S) <= MAX_POOL_SIZE)

typedef struct thread_data
{
    int index;
    int response_num;

    client_list_t *client_list;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

} thread_data_t;


char hello_world_page[] = "HTTP/1.1 200 OK\r\nContent-Length: 71\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Hello</TITLE></HEAD><BODY>Hello World!</BODY></HTML>\r\n";
char format_page[] = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Hello</TITLE></HEAD><BODY>Thread %s\nRequest #%s</BODY></HTML>\r\n";
//                                                       /\
//                                                     77 + ...


int threads_size;
pthread_t threads[MAX_POOL_SIZE];
thread_data_t *threads_data[MAX_POOL_SIZE];
pthread_mutex_t threads_data_mutex = PTHREAD_MUTEX_INITIALIZER;
int is_done_listening;

void free_thread_data(thread_data_t *data, int is_destroy_mutex, int is_destroy_cond)
{
    if (data == NULL)
    {
        return;
    }

    pthread_mutex_lock(&threads_data_mutex);
    threads_data[data->index] = NULL;
    pthread_mutex_unlock(&threads_data_mutex);

    if (is_destroy_mutex)
    {
        pthread_mutex_destroy(&data->mutex);
    }
    if (is_destroy_cond)
    {
        pthread_cond_destroy(&data->cond);
    }

    list_destroy(data->client_list);

    free(data);
}

int init_select_masks(thread_data_t *data, fd_set *readfds, fd_set *writefds)
{
    int max_fd = 0;

    client_desc_t *cur = data->client_list->head;
    while (cur != NULL)
    {
        switch (cur->state)
        {
            case READ_REQUEST:
                FD_SET(cur->socket_fd, readfds);
                break;
            case WRITE_RESPONSE:
                FD_SET(cur->socket_fd, writefds);
                break;
        }

        if (cur->socket_fd > max_fd)
        {
            max_fd = cur->socket_fd;
        }

        cur = cur->next;
    }

    return max_fd;
}

int update_client_read(client_desc_t *client, fd_set *readfds)
{
    if (!FD_ISSET(client->socket_fd, readfds))
    {
        return 0;
    }

    char buf[BUF_SIZE];

    ssize_t bytes_read = recv(client->socket_fd, buf, BUF_SIZE, 0);
    if (bytes_read < 0)
    {
        perror("\nupdate_client_read: recv error");
        return -1;
    }
    if (bytes_read == 0)
    {
        return -1;
    }

    if (client->request_size + bytes_read > client->request_alloc_size)
    {
        client->request_alloc_size += BUF_SIZE;
        char *check = (char *)realloc(client->request, client->request_alloc_size);
        if (check == NULL)
        {
            perror("\nupdate_client_read: realloc error");
            return -1;
        }
        client->request = check;
    }
    memcpy(client->request + client->request_size, buf, bytes_read);
    client->request_size += bytes_read;

    if (client->request_size >= 3 && !strings_equal(client->request, "GET", 3))
    {
        return -1;
    }

    if (
            (client->request_size >= 4 && strings_equal(client->request + client->request_size - 4, "\r\n\r\n", 4) )||
            (client->request_size >= 2 && strings_equal(client->request + client->request_size - 2, "\n\n", 2))
        )
    {
        client->state = WRITE_RESPONSE;
    }

    return 0;
}

int update_client_write(client_desc_t *client, fd_set *writefds)
{
    if (!FD_ISSET(client->socket_fd, writefds))
    {
        return 0;
    }

    ssize_t bytes_written = write(client->socket_fd, client->response + client->response_bytes_written, client->response_size - client->response_bytes_written);
    if (bytes_written < 0)
    {
        perror("\nupdate_client_write: write error");
        return -1;
    }
    client->response_bytes_written += bytes_written;

    return client->response_bytes_written >= client->response_size ? -1 : 0;
}

void update_clients(thread_data_t *data, fd_set *readfds, fd_set *writefds)
{
    client_desc_t *cur = data->client_list->head;
    while (cur != NULL)
    {
        client_state_t old_state = cur->state;

        int err_code = 0;
        switch (cur->state)
        {
            case READ_REQUEST:
                err_code = update_client_read(cur, readfds);
                break;
            case WRITE_RESPONSE:
                err_code = update_client_write(cur, writefds);
                break;
        }

        if (err_code < 0)
        {
            client_desc_t *next = cur->next;
            list_remove(data->client_list, cur);
            cur = next;
            continue;
        }

        if (old_state == READ_REQUEST && cur->state == WRITE_RESPONSE)
        {
            char thread_num[4];
            int thread_num_length = sprintf(thread_num, "%d", data->index);

            char response_num[15];
            int response_num_length = sprintf(response_num, "%d", data->response_num);

            cur->response = calloc(BUF_SIZE, sizeof(char));
            if (cur->response == NULL)
            {
                cur->response = hello_world_page;
                cur->response_size = sizeof(hello_world_page);
            }
            else
            {
                int length = sprintf(cur->response, format_page, 77 + thread_num_length + response_num_length, thread_num, response_num);
                cur->response_size = length;
            }

            data->response_num++;
        }

        cur = cur->next;
    }
}

void *worker(void *param)
{
    if (param == NULL)
    {
        fprintf(stderr, "worker: param was NULL");
        return NULL;
    }

    thread_data_t *data = (thread_data_t *)param;

    while (1)
    {
        pthread_mutex_lock(&data->mutex);

        while (data->client_list->head == NULL && !is_done_listening)
        {
            pthread_cond_wait(&data->cond, &data->mutex);
        }

        if (is_done_listening && data->client_list->head == NULL)
        {
            break;
        }

        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        int max_fd = init_select_masks(data, &readfds, &writefds);

        pthread_mutex_unlock(&data->mutex);

        int num_fds_ready = select(max_fd + 1, &readfds, &writefds, NULL, NULL);
        if (num_fds_ready < 0)
        {
            perror("worker: select error");
            break;
        }
        if (num_fds_ready == 0)
        {
            continue;
        }

        pthread_mutex_lock(&data->mutex);

        update_clients(data, &readfds, &writefds);

        pthread_mutex_unlock(&data->mutex);
    }

    free_thread_data(data, 1, 1);
    return NULL;
}

int create_threads(int pool_size)
{
    int real_pool_size = 0;

    for (int i = 0; i < pool_size; i++)
    {
        thread_data_t *data = malloc(sizeof(thread_data_t));
        if (data == NULL)
        {
            perror("create_threads: malloc error");
            break;
        }

        data->index = i;
        data->response_num = 0;

        data->client_list = list_create();
        if (data->client_list == NULL)
        {
            fprintf(stderr, "create_threads: Unable to create list\n");
            free_thread_data(data, 0, 0);
            break;
        }

        int err_code = pthread_mutex_init(&data->mutex, NULL);
        if (err_code != 0)
        {
            fprintf(stderr, "create_threads: pthread_mutex_init: %s\n", strerror(err_code));
            free_thread_data(data, 0, 0);
            break;
        }

        err_code = pthread_cond_init(&data->cond, NULL);
        if (err_code != 0)
        {
            fprintf(stderr, "create_threads: pthread_cond_init: %s\n", strerror(err_code));
            free_thread_data(data, 1, 0);
            break;
        }

        pthread_t thread_id;
        err_code = pthread_create(&thread_id, NULL, worker, data);
        if (err_code != 0)
        {
            fprintf(stderr, "create_threads: pthread_create: %s\n", strerror(err_code));
            free_thread_data(data, 1, 1);
            break;
        }
        pthread_detach(thread_id);

        threads[i] = thread_id;
        threads_data[i] = data;

        real_pool_size++;
    }

    return real_pool_size;
}

void accept_clients(int listen_socket_fd)
{
    int cur_idx = 0;
    while (1)
    {
        int client_socket_fd = accept(listen_socket_fd, NULL, NULL);
        if (client_socket_fd < 0)
        {
            perror("accept error");

            pthread_mutex_lock(&threads_data_mutex);

            is_done_listening = 1;
            for (int i = 0; i < threads_size; i++)
            {
                if (threads_data[i] == NULL)
                {
                    continue;
                }
                pthread_cond_signal(&threads_data[i]->cond);
            }

            pthread_mutex_unlock(&threads_data_mutex);

            break;
        }

        //distribute clients using round-robin

        pthread_mutex_lock(&threads_data_mutex);

        int start_idx = cur_idx, found = 0;
        while (!found)
        {
            if (threads_data[cur_idx] != NULL)
            {
                pthread_mutex_lock(&threads_data[cur_idx]->mutex);

                list_add(threads_data[cur_idx]->client_list, client_socket_fd);
                pthread_cond_signal(&threads_data[cur_idx]->cond);

                pthread_mutex_unlock(&threads_data[cur_idx]->mutex);

                fprintf(stderr, "Client %d was assigned to thread %d\n", client_socket_fd, cur_idx);

                found = 1;
            }

            cur_idx++;
            if (cur_idx >= threads_size)
            {
                cur_idx = 0;
            }

            if (found == 0 && cur_idx == start_idx)
            {
                fprintf(stderr, "Can't access any threads data. Stopping.\n");
                break;
            }
        }

        if (found == 0)
        {
            shutdown(client_socket_fd, SHUT_RDWR);
            close(client_socket_fd);

            is_done_listening = 1;

            pthread_mutex_unlock(&threads_data_mutex);
            break;
        }

        pthread_mutex_unlock(&threads_data_mutex);
    }
}

int main(int argc, char **argv)
{
    if (argc <= 2)
    {
        fprintf(stderr, "Usage: %s port pool_size\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (!IS_PORT_VALID(port))
    {
        fprintf(stderr, "Port must be in range [0, %d], got: %d\n", 0xFFFF, port);
        return EXIT_FAILURE;
    }

    int pool_size = atoi(argv[2]);
    if (!IS_POOL_SIZE_VALID(pool_size))
    {
        fprintf(stderr, "Pool size must be in range [1, %d], got: %d\n", MAX_POOL_SIZE, pool_size);
        return EXIT_FAILURE;
    }

    int listen_socket_fd = open_listen_socket(port);
    if (listen_socket_fd < 0)
    {
        fprintf(stderr, "Unable to open listen socket\n");
        return EXIT_FAILURE;
    }

    is_done_listening = 0;

    threads_size = create_threads(pool_size);
    if (threads_size == 0)
    {
        fprintf(stderr, "Unable to create at least 1 thread\n");
        close(listen_socket_fd);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Created %d of %d threads\n", threads_size, pool_size);
    accept_clients(listen_socket_fd);
    fprintf(stderr, "Done listening. Waiting for all connections to finish.\n");

    close(listen_socket_fd);
    return EXIT_SUCCESS;
}
