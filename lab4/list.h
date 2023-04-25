//
// Created by vital on 06-Apr-23.
//

#pragma once

#include <stdio.h>

typedef enum client_state
{
    READ_REQUEST,
    WRITE_RESPONSE
} client_state_t;

typedef struct client_desc
{
    int socket_fd;
    client_state_t state;

    char *request;
    ssize_t request_size;
    ssize_t request_alloc_size;

    char *response;
    ssize_t response_size;
    ssize_t response_bytes_written;

    struct client_desc *prev;
    struct client_desc *next;
} client_desc_t;

typedef struct client_list
{
    client_desc_t *head;
    size_t size;
} client_list_t;


client_list_t *list_create();

client_desc_t *list_add(client_list_t *list, int socket_fd);

void list_remove(client_list_t *list, client_desc_t *entry);

void list_destroy(client_list_t *list);
