#ifndef __MENUS_H__
#define __MENUS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>

void get_main_menu (GtkWidget **menubar, GtkAcceleratorTable **table);
void menus_create(GtkMenuEntry *entries, int nmenu_entries);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MENUS_H__ */
