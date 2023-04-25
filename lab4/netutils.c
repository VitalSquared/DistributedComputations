//
// Created by vital on 06-Apr-23.
//

#include "netutils.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int open_listen_socket(int port)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("open_listen_socket: socket error");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("open_listen_socket: bind error");
        close(sock_fd);
        return -1;
    }

    if (listen(sock_fd, SOMAXCONN) < 0)
    {
        perror("open_listen_socket: listen error");
        close(sock_fd);
        return -1;
    }

    int true = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int)) < 0) {
        perror("open_listen_socket: setsockopt error");
    }

    return sock_fd;
}

void write_to_fd(int fd, const char *buf, ssize_t size, int timeout_s)
{
    ssize_t total_bytes_written = 0;

    while (total_bytes_written < size)
    {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd, &writefds);

        struct timeval timeout;
        timeout.tv_sec = timeout_s;
        timeout.tv_usec = 0;

        int num_fds_ready = select(fd + 1, NULL, &writefds, NULL, &timeout);
        if (num_fds_ready < 0)
        {
            perror("\nwrite_to_fd: select error");
            break;
        }
        if (num_fds_ready == 0)
        {
            break;
        }

        ssize_t bytes_written = write(fd, buf + total_bytes_written, size - total_bytes_written);
        if (bytes_written < 0)
        {
            perror("\nwrite_to_fd: write error");
            break;
        }

        total_bytes_written += bytes_written;
    }
}

int strings_equal(const char *str1, const char *str2, size_t size)
{
    if (str1 == NULL || str2 == NULL)
    {
        return size == 0;
    }

    size_t idx = 0;
    while (idx < size && str1[idx] != '\0' && str2[idx] != '\0')
    {
        if (str1[idx] != str2[idx])
        {
            return 0;
        }
        idx++;
    }

    return idx == size;
}
