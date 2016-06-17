#ifndef __EXYNOS4412_FB_H__
#define __EXYNOS4412_FB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "surface.h"

void exynos4412_fb_initial(char * commandline);
struct surface_t * exynos4412_screen_surface(void);
void exynos4412_screen_swap(void);
void exynos4412_screen_flush(void);
void exynos4412_screen_backlight(u8_t brightness);
void exynos4412_set_progress(int percent);
void exynos4412_set_messge(const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __EXYNOS4412_FB_H__ */
