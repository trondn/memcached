/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/**
 * This piece of code is not inteded to be shipped in a released version
 * of memcached, but used for debugging purposes to figure out if what we're
 * seeing on a windows machine is in fact a fragmented memory or a memory
 * leak.
 *
 * I found the code for dumping information about a heap content in an
 * example at http://msdn.microsoft.com/en-us/library/windows/desktop/ee175819(v=vs.85).aspx
 */
#include "config.h"
#include "memcached.h"

#ifdef WIN32

#include <windows.h>
#include <stdio.h>
#include <malloc.h>

static void dump_specific_heap(HANDLE heap, FILE *fp) {
    DWORD LastError;
    PROCESS_HEAP_ENTRY ent;

    ent.lpData = NULL;

    while (HeapWalk(heap, &ent) != FALSE) {
        if ((ent.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
            fprintf(fp, "Allocated block");

            if ((ent.wFlags & PROCESS_HEAP_ENTRY_MOVEABLE) != 0) {
                fprintf(fp, ", movable with HANDLE %#p", ent.Block.hMem);
            }

            if ((ent.wFlags & PROCESS_HEAP_ENTRY_DDESHARE) != 0) {
                fprintf(fp, ", DDESHARE");
            }
        } else if ((ent.wFlags & PROCESS_HEAP_REGION) != 0) {
            fprintf(fp, "Region\n  %d bytes committed\n"
                    "  %d bytes uncommitted\n  First block address: %#p\n"
                    "  Last block address: %#p\n",
                    ent.Region.dwCommittedSize,
                    ent.Region.dwUnCommittedSize,
                    ent.Region.lpFirstBlock,
                    ent.Region.lpLastBlock);
        } else if ((ent.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) != 0) {
            fprintf(fp, "Uncommitted range\n");
        } else {
            fprintf(fp, "Block\n");
        }

        fprintf(fp, "  Data portion begins at: %#p\n  Size: %d bytes\n"
                "  Overhead: %d bytes\n  Region index: %d\n\n",
                ent.lpData,
                ent.cbData,
                ent.cbOverhead,
                ent.iRegionIndex);
    }
    LastError = GetLastError();
    if (LastError != ERROR_NO_MORE_ITEMS) {
        fprintf(stderr, "HeapWalk failed with LastError %d.\n", LastError);
    }
}

static void dump_heaps(FILE *fp) {
    DWORD total;
#define MAXHEAPS 1024
    HANDLE heaps[MAXHEAPS];

    if ((total = GetProcessHeaps(MAXHEAPS, heaps)) == 0) {
        fprintf(fp, "Failed to retrieve heaps with LastError %d.\n",
                GetLastError());
    } else {
        fprintf("Currently using %d heaps.\n", total);
        for (DWORD ii = 0; ii < total; ++ii) {
            fprintf(fp, "*****************************************\n");
            fprintf(fp, "%d at address: %#p.\n\n", ii, heaps[ii]);
            dump_specific_heap(heaps[ii], fp);
        }
    }
}

void dump_memory_info(void)
{
    time_t now = time(NULL);
    char name[1024];
    sprintf(name, "c:\\temp\\memcached-memory-dump-%d.txt", now);
    FILE *fp = fopen(name, "w");
    if (fp != NULL) {
#if 0
        struct mallinfo m = mallinfo();
        fprintf(fp, "mallinfo: {\n"
                "  unsigned long arena = %08ld /* total space in arena */\n"
                "  unsigned long ordblks = %08ld /* number of ordinary blocks */\n"
                "  unsigned long smblks = %08ld /* number of small blocks */\n"
                "  unsigned long hblkhd = %08ld /* space in holding block headers */\n"
                "  unsigned long hblks = %08ld /* number of holding blocks */\n"
                "  unsigned long usmblks = %08ld /* space in small blocks in use */\n"
                "  unsigned long fsmblks = %08ld /* space in free small blocks */\n"
                "  unsigned long uordblks = %08ld /* space in ordinary blocks in use */\n"
                "  unsigned long fordblks = %08ld /* space in free ordinary blocks */\n"
                "  unsigned long keepcost = %08ld /* space penalty if keep option */\n"
                "                                         /* is used */\n",
                (unsigned long)m.arena,
                (unsigned long)m.ordblks,
                (unsigned long)m.smblks,
                (unsigned long)m.hblkhd,
                (unsigned long)m.hblks,
                (unsigned long)m.usmblks,
                (unsigned long)m.fsmblks,
                (unsigned long)m.uordblks,
                (unsigned long)m.fordblks,
                (unsigned long)m.keepcost);
        fprintf(fp, "\n\n");
#endif
        dump_heaps(fp);
        fclose(fp);
    }
}

#else

void dump_memory_info(void)
{
    /* Do nothing */
}
#endif
