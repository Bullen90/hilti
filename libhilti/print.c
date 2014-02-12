///
/// Hilti::print() implementation.
//
//
#define _POSIX_SOURCE
#define _POSIX_C_SOURCE 199309

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "types.h"
#include "rtti.h"
#include "memory_.h"
#include "string_.h"
#include "hutil.h"

/*
 * Hilti::print(obj, newline = True)
 *
 * Prints a textual representation of an object to stdout.
 *
 * obj: instance of any HILTI type - The object to print.
 * newline: bool - If true, a newline is added automatically.
 *
 */
void hilti_print(const hlt_type_info* type, void* obj, int8_t newline, hlt_exception** excpt, hlt_execution_context* ctx)
{
    // To prevent race conditions with multiple threads, we have to lock
    // stdout here and then unlock it at each possible exit to this function.

    // We must not terminate while in here.
    int old_state;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);

    flockfile(stdout);

    if ( type->to_string ) {
        __hlt_pointer_stack* seen = __hlt_pointer_stack_new();
        hlt_string s = hlt_object_to_string(type, obj, 0, seen, excpt, ctx);
        __hlt_pointer_stack_delete(seen);

        if ( *excpt )
            goto unlock;

        hlt_string_print(stdout, s, 0, excpt, ctx);
        GC_DTOR(s, hlt_string);

        if ( *excpt )
            goto unlock;
    }

    else {
        /* No fmt function, just print the tag. */
        fprintf(stdout, "<%s>", type->tag);
    }

    if ( newline )
        fputc('\n', stdout);

    fflush(stdout);

unlock:
    funlockfile(stdout);
    pthread_setcancelstate(old_state, NULL);
}

