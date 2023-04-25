//
// Created by vital on 06-Apr-23.
//

#pragma once

#include <stdio.h>

#define IS_PORT_VALID(P) (0 <= (P) && (P) <= 0xFFFF)

int open_listen_socket(int port);
void write_to_fd(int fd, const char *buf, ssize_t size, int timeout_s);
int strings_equal(const char *str1, const char *str2, size_t size);
