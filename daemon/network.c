#include "config.h"
#include "memcached.h"
#include "network.h"

#include <errno.h>
#include <strings.h>

#ifdef __WIN32__
static char *win_strerror(DWORD err) {
    char *msg = NULL;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err, 0, (LPTSTR)&msg, 0, NULL) != 0) {
        return msg;
    }
    return NULL;
}

static void win_log_error(const char *prefix, const void *c, DWORD err) {
    char *msg = win_strerror(err);
    if (msg) {
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                        "%s: %s", prefix, msg);
        LocalFree(msg);
    } else {
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                        "%s: %d", prefix, (int)err);
    }
}
#endif

SOCKET accept_socket(conn *c, void (*disable_listen)(void))
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    SOCKET sfd = accept(c->sfd, (struct sockaddr *)&addr, &addrlen);

    if (sfd == INVALID_SOCKET) {
#ifdef __WIN32__
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            win_log_error("Failed to accept new client", c, error);
        }
#else
        if (errno == EMFILE) {
            if (settings.verbose > 0) {
                settings.extensions.logger->log(EXTENSION_LOG_INFO, c,
                                                "Too many open connections");
            }
            disable_listen();
            return -1;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                            "Failed to accept new client: %s",
                                            strerror(errno));
        }
#endif

        return -1;
    }

    return sfd;
}

int listen_socket(SOCKET s, int backlog) {
    if (listen(s, backlog) == SOCKET_ERROR) {
#ifdef __WIN32__
        int error = WSAGetLastError();
        win_log_error("listen() failed", NULL, error);
#else
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                        "listen() failed",
                                        strerror(errno));
#endif
        return SOCKET_ERROR;
    }

    return 0;
}

int bind_socket(SOCKET s,
                const struct sockaddr *addr,
                socklen_t addrlen,
                int *inuse) {
    if (bind(s, addr, addrlen) == SOCKET_ERROR) {
#ifdef __WIN32__
        int error = WSAGetLastError();
        *inuse = (error == WSAEADDRINUSE);
        win_log_error("bind() failed", NULL, error);
#else
        *inuse = (errno == EADDRINUSE);
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                        "bind() failed",
                                        strerror(errno));
#endif
    }

    return 0;
}

ssize_t sendmsg_socket(conn *c,
                       const struct msghdr *msg,
                       int *blocking)
{
#ifdef __WIN32__
    DWORD dwBufferCount;

    if (WSASendTo(c->sfd, (LPWSABUF)msg->msg_iov,
                  (DWORD)msg->msg_iovlen, &dwBufferCount,
                  0, msg->msg_name, msg->msg_namelen, NULL, NULL) == 0) {
        return dwBufferCount;
    }

    int error = WSAGetLastError();
    if (error == WSAECONNRESET) {
        return 0;
    }

    if (error == WSAEWOULDBLOCK) {
        *blocking = 1;
    } else {
        *blocking = 0;
        win_log_error("Failed to write, and not due to blocking", c, error);
    }

    return -1;
#else
    ssize_t ret = sendmsg(c->sfd, msg, 0);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *blocking = 1;
        } else {
            *blocking = 0;
            settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                            "Failed to write, and not due to blocking: %s",
                                            strerror(errno));
        }
    } else if (ret == 0) {
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                        "%d - sendmsg returned 0\n",
                                        c->sfd);
        for (int ii = 0; ii < msg->msg_iovlen; ++ii) {
            settings.extensions.logger->log(EXTENSION_LOG_WARNING, c,
                                            "\t%d - %zu\n",
                                            c->sfd,
                                            msg->msg_iov[ii].iov_len);
        }
    }

    return ret;
#endif
}

ssize_t recv_socket(SOCKET sfd,
                    void *buffer,
                    size_t length,
                    int *blocking)
{
    ssize_t ret = recv(sfd, buffer, length, 0);
    if (ret == SOCKET_ERROR) {
#ifdef __WIN32__
        int error = WSAGetLastError();
        if (error == WSAECONNRESET) {
            return 0;
        }
        if (error == WSAEWOULDBLOCK) {
            *blocking = 1;
        } else {
            *blocking = 0;
            win_log_error("Closing connection due to read error", NULL, error);
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *blocking = 1;
        } else {
            if (errno == ENOTCONN && errno == ECONNRESET) {
                settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                                "%d The client is no longer there",
                                                sfd);

            } else {
                settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                                "%d Closing connection due to read error: %s",
                                                sfd, strerror(errno));
            }
        }
#endif
    }

    return ret;
}

ssize_t send_socket(SOCKET sfd,
                    const void *buffer,
                    size_t length,
                    int *blocking)
{
    ssize_t ret = send(sfd, buffer, length, 0);
    if (ret == SOCKET_ERROR) {
#ifdef __WIN32__
        int error = WSAGetLastError();
        if (error == WSAECONNRESET) {
            return 0;
        }

        if (error == WSAEWOULDBLOCK) {
            *blocking = 1;
        } else {
            *blocking = 0;
            win_log_error("Closing connection due to send error", NULL, error);
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *blocking = 1;
        } else {
            if (errno == ENOTCONN && errno == ECONNRESET) {
                settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                                "%d The client is no longer there",
                                                sfd);

            } else {
                settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                                "%d Closing connection due to send error: %s",
                                                sfd, strerror(errno));
            }
        }
#endif
    }

    return ret;
}

int close_socket(SOCKET s)
{
    int rval = 0;
#ifdef __WIN32__
    if (closesocket(s) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        win_log_error("Failed to close socket", NULL, error);
        rval = -1;
    }
#else
    while ((rval = close(s)) == -1 && (errno == EINTR || errno == EAGAIN)) {
        /* go ahead and retry */
    }

    if (rval == -1) {
        settings.extensions.logger->log(EXTENSION_LOG_WARNING, NULL,
                                        "Failed to close socket %d (%s)!!\n",
                                        (int)s, strerror(errno));
    }
#endif
    return rval;
}
