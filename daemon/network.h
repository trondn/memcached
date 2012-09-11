/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef NETWORK_H
#define NETWORK_H

SOCKET accept_socket(conn *c, void (*disable_listen)(void));
int listen_socket(SOCKET s, int backlog);
int bind_socket(SOCKET s, const struct sockaddr *addr,
                socklen_t addrlen, int *inuse);
ssize_t sendmsg_socket(conn *c, const struct msghdr *message,
                       int *blocking);
ssize_t recv_socket(SOCKET sfd, void *buffer, size_t length, int *blocking);
ssize_t send_socket(SOCKET sfd, const void *buffer, size_t length, int *blocking);
int close_socket(SOCKET s);
#endif
