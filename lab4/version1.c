//
// Created by vital on 06-Apr-23.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include "netutils.h"

#define BUF_SIZE 1024
#define TIMEOUT_S 10

char *recv_request(int client_socket_fd, ssize_t *out_size)
{
    char buf[BUF_SIZE];
    char *request = NULL;
    ssize_t request_size = 0, request_alloc_size = 0;

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client_socket_fd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_S;
        timeout.tv_usec = 0;

        int num_fds_ready = select(client_socket_fd + 1, &readfds, NULL, NULL, &timeout);
        if (num_fds_ready < 0)
        {
            perror("\nrecv_request: select error");
            break;
        }
        if (num_fds_ready == 0)
        {
            break;
        }

        ssize_t bytes_read = recv(client_socket_fd, buf, BUF_SIZE, 0);
        if (bytes_read < 0)
        {
            perror("\nrecv_request: recv error");
            break;
        }
        if (bytes_read == 0)
        {
            break;
        }

        if (request_size + bytes_read > request_alloc_size)
        {
            request_alloc_size += BUF_SIZE;
            char *check = (char *)realloc(request, request_alloc_size);
            if (check == NULL)
            {
                perror("\nrecv_request: realloc error");
                break;
            }
            request = check;
        }
        memcpy(request + request_size, buf, bytes_read);
        request_size += bytes_read;

        if (
                (request_size >= 4 && strings_equal(request + request_size - 4, "\r\n\r\n", 4) )||
                (request_size >= 2 && strings_equal(request + request_size - 2, "\n\n", 2))
            )
        {
            break;
        }
    }

    shutdown(client_socket_fd, SHUT_RDWR);

    *out_size = request_size;
    return request;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port = atoi(argv[1]);
    if (!IS_PORT_VALID(port))
    {
        fprintf(stderr, "Port must be in range [0, %d], got: %d\n", 0xFFFF, port);
        return EXIT_FAILURE;
    }

    int listen_socket_fd = open_listen_socket(port);
    if (listen_socket_fd < 0)
    {
        fprintf(stderr, "Unable to open listen socket\n");
        return EXIT_FAILURE;
    }

    int client_socket_fd = accept(listen_socket_fd, NULL, NULL);
    if (client_socket_fd < 0)
    {
        perror("accept error");
        close(listen_socket_fd);
        return EXIT_FAILURE;
    }

    ssize_t request_size = 0;
    char *request = recv_request(client_socket_fd, &request_size);
    if (request == NULL)
    {
        fprintf(stderr, "Didn't receive any request\n");
        close(client_socket_fd);
        close(listen_socket_fd);
        return EXIT_FAILURE;
    }

    write_to_fd(STDOUT_FILENO, request, request_size, TIMEOUT_S);

    free(request);
    close(client_socket_fd);
    close(listen_socket_fd);

    return EXIT_SUCCESS;
}
