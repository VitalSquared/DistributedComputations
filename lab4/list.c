//
// Created by vital on 06-Apr-23.
//

#include "list.h"
#include <stdlib.h>
#include <unistd.h>

client_list_t *list_create()
{
    client_list_t *list = malloc(sizeof(client_list_t));
    if (list == NULL)
    {
        perror("list_create: malloc error");
        return NULL;
    }

    list->head = NULL;
    list->size = 0;

    return list;
}

client_desc_t *list_add(client_list_t *list, int socket_fd)
{
    if (list == NULL)
    {
        return NULL;
    }

    client_desc_t *entry = calloc(1, sizeof(client_desc_t));
    if (entry == NULL)
    {
        perror("list_add: calloc error");
        return NULL;
    }

    entry->socket_fd = socket_fd;
    entry->state = READ_REQUEST;

    if (list->head != NULL)
    {
        list->head->prev = entry;
        entry->next = list->head;
    }
    list->head = entry;
    list->size++;

    return entry;
}

void list_remove(client_list_t *list, client_desc_t *entry)
{
    if (list == NULL || list->size == 0 || entry == NULL)
    {
        return;
    }

    if (list->head == entry)
    {
        list->head = list->head->next;
        if (list->head != NULL)
        {
            list->head->prev = NULL;
        }
    }
    else
    {
        entry->prev->next = entry->next;
        if (entry->next != NULL)
        {
            entry->next->prev = entry->prev;
        }
    }
    list->size--;

    close(entry->socket_fd);
    free(entry->request);
    free(entry->response);
    free(entry);
}

void list_destroy(client_list_t *list)
{
    if (list == NULL)
    {
        return;
    }

    while(list->head != NULL)
    {
        list_remove(list, list->head);
    }

    free(list);
}
