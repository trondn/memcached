/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef CONFIG_H
#define CONFIG_H 1

#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS 1

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

struct msghdr {
    void *msg_name;         /* Socket name */
    int msg_namelen;       /* Length of name */
    struct iovec *msg_iov; /* Data blocks */
    int msg_iovlen;        /* Number of blocks */
};

#define PATH_MAX 1024

/* @todo investigate this one.. */

#define SOCKETPAIR_AF AF_INET
#define get_socket_error() WSAGetLastError()
__declspec(dllimport)
extern void initialize_sockets(void);
__declspec(dllimport)
int sendmsg(SOCKET sock, const struct msghdr *msg, int flags);

typedef int in_port_t;
typedef HANDLE pid_t;

#define snprintf _snprintf
#define strtoull(a, b, c) _strtoui64(a, b, c)
#define strtoll(a, b, c) _strtoi64(a, b, c)
/* to stop the stupid compiler to whine about this.. */
#ifndef __cplusplus
#define strdup(a) _strdup(a)
#define putenv(a) _putenv(a)
#endif

#if !defined(__cplusplus) && !defined(PRIu64)
#define PRIu64 "I64u"
#endif

#if !defined(__cplusplus) && !defined(PRI64)
#define PRI64 "I64d"
#endif

#if !defined(__cplusplus) && !defined(PRIu32)
#define PRIu32 "u"
#endif

#else

#ifndef __cplusplus
typedef unsigned long long uint64_t;
typedef long long int64_t;
#endif

#define closesocket(a) close(a)
#define SOCKET int
#define SOCKETPAIR_AF AF_UNIX
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define get_socket_error() errno
#define initialize_sockets()

/* some POSIX systems need the following definition
 * to get mlockall flags out of sys/mman.h.  */
#ifndef _P1003_1B_VISIBLE
#define _P1003_1B_VISIBLE
#endif
/* need this to get IOV_MAX on some platforms. */
#ifndef __need_IOV_MAX
#define __need_IOV_MAX
#endif

#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS
#endif

#ifdef __sun
#include <priv.h>
#define HAVE_DROP_PRIVILEGES 1
#define HAVE_SIGIGNORE 1
#endif

#ifdef __APPLE__
#define HAVE_SIGIGNORE 1
#endif


#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <pwd.h>
#include <sys/time.h>
#include <signal.h>

/* @todo trondn fixme!! */
#if !defined(__cplusplus) && !defined(PRIu64)
#define PRIu64 "lu"
#endif

#if !defined(__cplusplus) && !defined(PRIu32)
#define PRIu32 "u"
#endif


#endif

/* Common section */
#include <dlfcn.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sysexits.h>

#ifndef __cplusplus
#ifndef bool
#define bool char
#define false 0
#define true 1
#endif

#endif

#ifndef IOV_MAX
# define IOV_MAX 1024
#endif
#define SASL_ENABLED

#define MEMCACHED_VERSION "1.4.5"

#endif
