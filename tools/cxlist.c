/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: MIT
 *
 * cxlist - the commodities registered with commodities.library, in the
 * order its input handler hands them an event: a commodity earlier in
 * the list sees a mouse press first, and can take it away from the
 * ones after it. Written for a system where EdgeSnap saw every button
 * release and not one press (2026-10-05).
 *
 * CopyBrokerList and FreeBrokerList are the calls Exchange uses: in
 * the library since V36, left out of the public headers, hence the
 * stubs here.
 */
#include <stdio.h>

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/tasks.h>
#include <libraries/commodities.h>
#include <proto/exec.h>
#include <inline/macros.h>

struct Library *CxBase;

struct BrokerCopy {
    struct Node bc_Node;
    char bc_Name[CBD_NAMELEN];
    char bc_Title[CBD_TITLELEN];
    char bc_Descr[CBD_DESCRLEN];
    struct Task *bc_Task;
    struct MsgPort *bc_Port;
    UWORD bc_Dummy;
    ULONG bc_Flags;
};

#define CopyBrokerList(blist) \
    LP1(0xba, LONG, CopyBrokerList, struct List *, blist, a0, , CxBase)
#define FreeBrokerList(blist) \
    LP1NR(0xc0, FreeBrokerList, struct List *, blist, a0, , CxBase)

int main(void)
{
    struct List list;
    struct Node *n;
    LONG count;
    int i = 0;

    CxBase = OpenLibrary((CONST_STRPTR)"commodities.library", 37);
    if (CxBase == NULL) {
        printf("cxlist: no commodities.library\n");
        return 20;
    }
    list.lh_Head = (struct Node *)&list.lh_Tail;
    list.lh_Tail = NULL;
    list.lh_TailPred = (struct Node *)&list.lh_Head;
    count = CopyBrokerList(&list);
    printf("cxlist: %ld commodities, in the order they see events\n",
           (long)count);
    for (n = list.lh_Head; n->ln_Succ != NULL; n = n->ln_Succ) {
        struct BrokerCopy *bc = (struct BrokerCopy *)n;

        /* the copy's ln_Pri is not the broker's priority (seen on
         * 3.2.3: letters of a name), so the order is all it tells */
        printf("%2d  flags %08lx  %-24.24s  %s\n", ++i,
               (unsigned long)bc->bc_Flags, bc->bc_Name, bc->bc_Title);
    }
    FreeBrokerList(&list);
    CloseLibrary(CxBase);
    return 0;
}
