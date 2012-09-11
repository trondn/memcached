/* win32.h
 *
 */

#ifndef WIN32_H
#define WIN32_H

#undef  _WIN32_WINNT
#define _WIN32_WINNT    0x0501        /* Needed to resolve getaddrinfo et al. */

#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <io.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <process.h>

#define EAI_SYSTEM         -11

#ifndef SIGHUP
#define SIGHUP -1
#endif

typedef char *caddr_t;

#define F_GETFL 3
#define F_SETFL 4

#define RUSAGE_SELF    0

#define IOV_MAX 1024

struct msghdr {
    void         *msg_name;         /* Socket name            */
    int          msg_namelen;       /* Length of name        */
    struct iovec *msg_iov;          /* Data blocks            */
    int          msg_iovlen;        /* Number of blocks        */
    void         *msg_accrights;    /* Per protocol magic (eg BSD file descriptor passing) */
    int          msg_accrightslen;  /* Length of rights list */
};

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

/* Structure which says how much of each resource has been used.  */
struct rusage {
    /* Total amount of user time used.  */
    struct timeval ru_utime;
    /* Total amount of system time used.  */
    struct timeval ru_stime;
    /* Maximum resident set size (in kilobytes).  */
    long int ru_maxrss;
    /* Amount of sharing of text segment memory
       with other processes (kilobyte-seconds).  */
    long int ru_ixrss;
    /* Amount of data segment memory used (kilobyte-seconds).  */
    long int ru_idrss;
    /* Amount of stack memory used (kilobyte-seconds).  */
    long int ru_isrss;
    /* Number of soft page faults (i.e. those serviced by reclaiming
       a page from the list of pages awaiting reallocation.  */
    long int ru_minflt;
    /* Number of hard page faults (i.e. those that required I/O).  */
    long int ru_majflt;
    /* Number of times a process was swapped out of physical memory.  */
    long int ru_nswap;
    /* Number of input operations via the file system.  Note: This
       and `ru_oublock' do not include operations with the cache.  */
    long int ru_inblock;
    /* Number of output operations via the file system.  */
    long int ru_oublock;
    /* Number of IPC messages sent.  */
    long int ru_msgsnd;
    /* Number of IPC messages received.  */
    long int ru_msgrcv;
    /* Number of signals delivered.  */
    long int ru_nsignals;
    /* Number of voluntary context switches, i.e. because the process
       gave up the process before it had to (usually to wait for some
       resource to be available).  */
    long int ru_nvcsw;
    /* Number of involuntary context switches, i.e. a higher priority process
       became runnable or the current process used up its time slice.  */
    long int ru_nivcsw;
};

int inet_aton(register const char *cp, struct in_addr *addr);

#if !defined(strtok_r)
#define strtok_r( _s, _sep, _lasts ) \
        ( *(_lasts) = strtok( (_s), (_sep) ) )
#endif /* strtok_r */

#if !defined(AI_ALL)
#define AI_ALL 0X0
#endif

int getrusage(int who, struct rusage *usage);
int kill(int pid, int sig);
int sleep(int seconds);

struct sigaction {
    void (*sa_handler)(int);
    int sa_mask;
    int sa_flags;
};

#define sigemptyset(a) 0
extern int sigaction(int sig, struct sigaction *act, struct sigaction *oact);
#define daemonize(a,b) spawn_memcached(argc, argv)
extern int spawn_memcached(int argc, char **argv);
#endif
