#ifndef __IO_H__
#define __IO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

/*
 * read a byte value from address.
 */
static inline u8_t __readb(signed int addr)
{
	return( *((volatile u8_t *)(addr)) );
}

/*
 * read a word value from address.
 */
static inline u16_t __readw(signed int addr)
{
	return( *((volatile u16_t *)(addr)) );
}

/*
 * read a long value from address.
 */
static inline u32_t __readl(signed int addr)
{
	return( *((volatile u32_t *)(addr)) );
}

/*
 * write a byte value to address.
 */
static inline void __writeb(signed int addr, u8_t value)
{
	*((volatile u8_t *)(addr)) = value;
}

/*
 * write a word value to address.
 */
static inline void __writew(signed int addr, u16_t value)
{
	*((volatile u16_t *)(addr)) = value;
}

/*
 * write a long value to address.
 */
static inline void __writel(signed int addr, u32_t value)
{
	*((volatile u32_t *)(addr)) = value;
}

#define readb(a)		__readb(a)
#define readw(a)		__readw(a)
#define readl(a)		__readl(a)

#define writeb(a, v)	__writeb(a, v)
#define writew(a, v)	__writew(a, v)
#define writel(a, v)	__writel(a, v)

#ifdef __cplusplus
}
#endif

#endif /* __IO_H__ */
