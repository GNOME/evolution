#ifndef __E_BEVEL_BUTTON_UTIL_H__ 
#define __E_BEVEL_BUTTON_UTIL_H__

#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7

void
e_bevel_button_util_shade (GdkColor *a,
			   GdkColor *b,
			   gdouble   k);

#endif /* __E_BEVEL_BUTTON_UTIL_H__ */
