// $Id$
//
// Function implementing local- and encoding-specific functionality.
//
// TODO: Currently, these are just hard-coded in local-independent,
// ASCII-only way.

#ifndef JRX_JITTYPE_H
#define JRX_JITTYPE_H

#include <ctype.h>

#include "jrx-intern.h"
#include "ccl.h"

extern jrx_ccl* local_ccl_lower(jrx_ccl_group* set);
extern jrx_ccl* local_ccl_upper(jrx_ccl_group* set);
extern jrx_ccl* local_ccl_word(jrx_ccl_group* set);

static inline int _isword(jrx_char cp) {
    return isalnum(cp) || cp == '_';
}

static inline int local_word_boundary(jrx_char* prev, jrx_char current)
{
    return _isword(current) ? (prev ? ! _isword(*prev) : 1) : 0;
}

#endif
