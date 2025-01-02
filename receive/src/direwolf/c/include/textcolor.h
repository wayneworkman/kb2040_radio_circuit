// File: receive/src/direwolf/c/include/textcolor.h

#ifndef TEXTCOLOR_H
#define TEXTCOLOR_H 1

enum dw_color_e {
    DW_COLOR_INFO,
    DW_COLOR_ERROR,
    DW_COLOR_DEBUG
};

typedef enum dw_color_e dw_color_t;

static inline void text_color_init(int enable_color) { (void)enable_color; }
static inline void text_color_set(dw_color_t c){ (void)c; }
static inline void text_color_term(void){}

int dw_printf(const char *fmt, ...)
    __attribute__((format(printf,1,2)));

#endif
