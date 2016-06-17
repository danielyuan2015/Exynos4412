#ifndef __MALLOC_H__
#define __MALLOC_H__

#include "types.h"

void * xmalloc(unsigned int size);
void * xrealloc(void * ptr, unsigned int size);
void xfree(void * ptr);
void * xcalloc(unsigned int nmemb, unsigned int size);

#endif /* __MALLOC_H__ */
