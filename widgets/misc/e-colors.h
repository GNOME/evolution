#ifndef GNUMERIC_COLOR_H
#define GNUMERIC_COLOR_H

void     color_init      (void);

/* Return the pixel value for the given red, green and blue */
int      color_alloc      (gushort red, gushort green, gushort blue);
void     color_alloc_name (const char *name, GdkColor *color);
void     color_alloc_gdk  (GdkColor *color);

/* Colors used by any GnumericSheet item */
extern GdkColor gs_white, gs_light_gray, gs_dark_gray, gs_black, gs_red, gs_lavender;

#endif /* GNUMERIC_COLOR_H */
