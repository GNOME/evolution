/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
 * bonobo-ui-compat.c: Compatibility functions for the old GnomeUI stuff,
 *                    and the old Bonobo UI handler API.
 *
 *  This module acts as an excercise in filthy coding habits
 * take a look around.
 *
 * Author:
 *	Michael Meeks (michael@helixcode.com)
 *
 * Copyright 2000 Helix Code, Inc.
 */
#ifndef _BONOBO_UI_COMPAT_H_
#define _BONOBO_UI_COMPAT_H_

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkaccelgroup.h>
#include <gnome-xml/tree.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-win.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-control-frame.h>
#include <bonobo/bonobo-view.h>
#include <bonobo/bonobo-view-frame.h>

#define BONOBO_UI_HANDLER_TYPE        (bonobo_ui_handler_get_type ())
#define BONOBO_UI_HANDLER(o)          (GTK_CHECK_CAST ((o), BONOBO_UI_HANDLER_TYPE, BonoboUIHandler))
#define BONOBO_UI_HANDLER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), BONOBO_UI_HANDLER_TYPE, BonoboUIHandlerClass))
#define BONOBO_IS_UI_HANDLER(o)       (GTK_CHECK_TYPE ((o), BONOBO_UI_HANDLER_TYPE))
#define BONOBO_IS_UI_HANDLER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), BONOBO_UI_HANDLER_TYPE))

typedef enum {
	BONOBO_UI_COMPAT_LIST,
	BONOBO_UI_COMPAT_ONE,
	BONOBO_UI_COMPAT_TREE
} BonoboUICompatType;

typedef struct {
	BonoboUICompatType type;
	GnomeUIInfo       *uii;
	gpointer           data;
} BonoboUIHandlerMenuItem;

typedef BonoboUIHandlerMenuItem BonoboUIHandlerToolbarItem;
typedef gpointer                BonoboUIHandler;

/*
 * The order of the arguments to this function might seem a bit
 * confusing; the closure (user_data), typically passed as the final
 * argument to a callback, is followed by a path parameter.  This is
 * to provide compatibility with the GnomeUIInfo (gnome-app-helper)
 * callback routines, whose arguments are (GtkWidget *menu_item, void
 * *user_data).
 */
typedef void (*BonoboUIHandlerCallback) (BonoboUIHandler *uih, void *user_data, const char *path);

typedef enum {
	BONOBO_UI_HANDLER_PIXMAP_NONE,
	BONOBO_UI_HANDLER_PIXMAP_STOCK,
	BONOBO_UI_HANDLER_PIXMAP_FILENAME,
	BONOBO_UI_HANDLER_PIXMAP_XPM_DATA,
	BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA
} BonoboUIHandlerPixmapType;

typedef enum {
	BONOBO_UI_HANDLER_MENU_END,
	BONOBO_UI_HANDLER_MENU_ITEM,
	BONOBO_UI_HANDLER_MENU_SUBTREE,
	BONOBO_UI_HANDLER_MENU_RADIOITEM,
	BONOBO_UI_HANDLER_MENU_RADIOGROUP,
	BONOBO_UI_HANDLER_MENU_TOGGLEITEM,
	BONOBO_UI_HANDLER_MENU_SEPARATOR,
	BONOBO_UI_HANDLER_MENU_PLACEHOLDER
} BonoboUIHandlerMenuItemType;

/*
 *  A hack to allow placeholders to be put into a gnome-app-helper
 * structure, so that they can be used trivialy.
 */
#define BONOBO_APP_UI_PLACEHOLDER (GNOME_APP_UI_SUBTREE_STOCK + 0x100)
#define BONOBOUIINFO_PLACEHOLDER(label) \
	{ BONOBO_APP_UI_PLACEHOLDER, label, NULL, NULL, NULL, NULL, \
		GNOME_APP_PIXMAP_NONE, NULL, 0, (GdkModifierType) 0, NULL }

typedef enum {
	BONOBO_UI_HANDLER_TOOLBAR_END,
	BONOBO_UI_HANDLER_TOOLBAR_ITEM,
	BONOBO_UI_HANDLER_TOOLBAR_RADIOITEM,
	BONOBO_UI_HANDLER_TOOLBAR_RADIOGROUP,
	BONOBO_UI_HANDLER_TOOLBAR_TOGGLEITEM,
	BONOBO_UI_HANDLER_TOOLBAR_SEPARATOR,
	BONOBO_UI_HANDLER_TOOLBAR_CONTROL
} BonoboUIHandlerToolbarItemType;

/*
 * New methods you need to add:
 */
/*
 * _new_for_app to create the toplevel UI handler
 * _new to create the normal old style handler.
 */
BonoboUIHandler		*bonobo_ui_handler_new				(void);
BonoboUIHandler		*bonobo_ui_handler_new_from_component           (BonoboUIComponent *component);

BonoboUIComponent       *bonobo_ui_compat_get_component                 (BonoboUIHandler *uih);
BonoboWin               *bonobo_ui_compat_get_app                       (BonoboUIHandler *uih);
Bonobo_UIContainer       bonobo_ui_compat_get_container                 (BonoboUIHandler *uih);

/*
 * Compat functions; no special use needed
 */
GtkType			 bonobo_ui_handler_get_type			(void);
void			 bonobo_ui_handler_set_container                (BonoboUIHandler *uih,
									 Bonobo_Unknown   container);

void			 bonobo_ui_handler_unset_container              (BonoboUIHandler *uih);
char			*bonobo_ui_handler_build_path			(const char *base, ...);
char			*bonobo_ui_handler_build_path_v			(const char *base, va_list path_components);

/* Toplevel menu routines. */
void			 bonobo_ui_handler_set_app			(BonoboUIHandler *uih, BonoboWin *app);
BonoboWin               *bonobo_ui_handler_get_app                      (BonoboUIHandler *uih);

void			 bonobo_ui_handler_set_toolbar			(BonoboUIHandler *uih, const char *name,
									 GtkWidget *toolbar);

void                     bonobo_ui_handler_create_menubar               (BonoboUIHandler *uih);
void			 bonobo_ui_handler_menu_new			(BonoboUIHandler *uih, const char *path,
									 BonoboUIHandlerMenuItemType type,
									 const char *label, const char *hint,
									 int pos, BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void			 bonobo_ui_handler_menu_new_item		(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint,
									 int pos, BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void			 bonobo_ui_handler_menu_new_subtree		(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data, guint accelerator_key,
									 GdkModifierType ac_mods);
void			 bonobo_ui_handler_menu_new_separator		(BonoboUIHandler *uih, const char *path, int pos);
void			 bonobo_ui_handler_menu_new_placeholder		(BonoboUIHandler *uih, const char *path);
void			 bonobo_ui_handler_menu_new_radiogroup		(BonoboUIHandler *uih, const char *path);
void			 bonobo_ui_handler_menu_new_radioitem		(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void			 bonobo_ui_handler_menu_new_toggleitem		(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void			 bonobo_ui_handler_menu_add_one			(BonoboUIHandler *uih, const char *parent_path,
									 BonoboUIHandlerMenuItem *item);
void			 bonobo_ui_handler_menu_add_list       		(BonoboUIHandler *uih, const char *parent_path,
									 BonoboUIHandlerMenuItem *item);
void			 bonobo_ui_handler_menu_add_tree       		(BonoboUIHandler *uih, const char *parent_path,
									 BonoboUIHandlerMenuItem *item);
void			 bonobo_ui_handler_menu_remove			(BonoboUIHandler *uih, const char *path);
gboolean                 bonobo_ui_handler_menu_path_exists             (BonoboUIHandler *uih, const char *path);
void			 bonobo_ui_handler_menu_free_one		(BonoboUIHandlerMenuItem *item);
void			 bonobo_ui_handler_menu_free_list		(BonoboUIHandlerMenuItem *item);
void			 bonobo_ui_handler_menu_free_tree		(BonoboUIHandlerMenuItem *item);
GList			*bonobo_ui_handler_menu_get_child_paths		(BonoboUIHandler *uih, const char *parent_path);

BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_one        (GnomeUIInfo *uii);
BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_list       (GnomeUIInfo *uii);
BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_tree       (GnomeUIInfo *uii);

BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_one_with_data  (GnomeUIInfo *uii, gpointer data);
BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_list_with_data (GnomeUIInfo *uii, gpointer data);
BonoboUIHandlerMenuItem *bonobo_ui_handler_menu_parse_uiinfo_tree_with_data (GnomeUIInfo *uii, gpointer data);

BonoboUIHandlerMenuItem *bonobo_ui_compat_menu_item_new                  (GnomeUIInfo *uii, gpointer data, BonoboUICompatType type);

int			 bonobo_ui_handler_menu_get_pos			(BonoboUIHandler *uih, const char *path);

void			 bonobo_ui_handler_menu_set_sensitivity		(BonoboUIHandler *uih, const char *path,
									 gboolean sensitive);

gchar                   *bonobo_ui_handler_menu_get_label               (BonoboUIHandler *uih, const char *path);
void			 bonobo_ui_handler_menu_set_label		(BonoboUIHandler *uih, const char *path,
									 const gchar *label);

void			 bonobo_ui_handler_menu_set_hint			(BonoboUIHandler *uih, const char *path,
									 const gchar *hint);

void			 bonobo_ui_handler_menu_set_pixmap		(BonoboUIHandler *uih, const char *path,
									 BonoboUIHandlerPixmapType type, gpointer data);

void			 bonobo_ui_handler_menu_set_callback		(BonoboUIHandler *uih, const char *path,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data,
									 GDestroyNotify callback_data_destroy_notify);
void			 bonobo_ui_handler_menu_get_callback		(BonoboUIHandler *uih, const char *path,
									 BonoboUIHandlerCallback *callback,
									 gpointer *callback_data,
									 GDestroyNotify *callback_data_destroy_notify);
void			 bonobo_ui_handler_menu_remove_callback_no_notify (BonoboUIHandler *uih, const char *path);

void			 bonobo_ui_handler_menu_set_toggle_state	(BonoboUIHandler *uih, const char *path,
									 gboolean state);
gboolean		 bonobo_ui_handler_menu_get_toggle_state	(BonoboUIHandler *uih, const char *path);

void			 bonobo_ui_handler_menu_set_radio_state		(BonoboUIHandler *uih, const char *path,
									 gboolean state);
gboolean		 bonobo_ui_handler_menu_get_radio_state		(BonoboUIHandler *uih, const char *path);

char			*bonobo_ui_handler_menu_get_radio_group_selection(BonoboUIHandler *uih, const char *rg_path);

void				 bonobo_ui_handler_create_toolbar	(BonoboUIHandler *uih, const char *name);
void				 bonobo_ui_handler_remove_toolbar	(BonoboUIHandler *uih, const char *name);

void				 bonobo_ui_handler_toolbar_new		(BonoboUIHandler *uih, const char *path,
									 BonoboUIHandlerToolbarItemType type,
									 const char *label, const char *hint,
									 int pos, const Bonobo_Control control,
									 BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data, guint accelerator_key,
									 GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void				 bonobo_ui_handler_toolbar_new_control	(BonoboUIHandler *uih, const char *path,
									 int pos, Bonobo_Control control);
void				 bonobo_ui_handler_toolbar_new_item	(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void				 bonobo_ui_handler_toolbar_item_set_pixmap	(BonoboUIHandler *uih, const char *path,
										 BonoboUIHandlerPixmapType type, gpointer data);
void				 bonobo_ui_handler_toolbar_new_separator	(BonoboUIHandler *uih, const char *path, int pos);
void				 bonobo_ui_handler_toolbar_new_radiogroup(BonoboUIHandler *uih, const char *path);
void				 bonobo_ui_handler_toolbar_new_radioitem	(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void				 bonobo_ui_handler_toolbar_new_toggleitem(BonoboUIHandler *uih, const char *path,
									 const char *label, const char *hint, int pos,
									 BonoboUIHandlerPixmapType pixmap_type,
									 gpointer pixmap_data,
									 guint accelerator_key, GdkModifierType ac_mods,
									 BonoboUIHandlerCallback callback,
									 gpointer callback_data);
void				 bonobo_ui_handler_toolbar_remove	(BonoboUIHandler *uih, const char *path);

BonoboUIHandlerMenuItem         *bonobo_ui_handler_toolbar_parse_uiinfo_list (GnomeUIInfo *uii);
BonoboUIHandlerMenuItem         *bonobo_ui_handler_toolbar_parse_uiinfo_list_with_data (GnomeUIInfo *uii, gpointer data);
void                             bonobo_ui_handler_toolbar_free_list    (BonoboUIHandlerMenuItem *item);
void				 bonobo_ui_handler_toolbar_add_list	(BonoboUIHandler *uih, const char *parent_path,
									 BonoboUIHandlerToolbarItem *item);

void			 bonobo_ui_handler_set_statusbar			(BonoboUIHandler *uih, GtkWidget *statusbar);
GtkWidget		*bonobo_ui_handler_get_statusbar			(BonoboUIHandler *uih);

gboolean  bonobo_ui_handler_dock_add            (BonoboUIHandler       *uih,
						 const char            *name,
						 Bonobo_Control         control,
						 GnomeDockItemBehavior  behavior,
						 GnomeDockPlacement     placement,
						 gint                   band_num,
						 gint                   band_position,
						 gint                   offset);
gboolean  bonobo_ui_handler_dock_remove         (BonoboUIHandler       *uih,
						 const char            *name);
gboolean  bonobo_ui_handler_dock_set_sensitive  (BonoboUIHandler       *uih,
						 const char            *name,
						 gboolean               sensitivity);
gboolean  bonobo_ui_handler_dock_get_sensitive  (BonoboUIHandler       *uih,
						 const char            *name);

/*
 * Other misc. deprecated stuff.
 */
BonoboUIHandler *bonobo_control_get_ui_handler        (BonoboControl *control);
Bonobo_Unknown   bonobo_control_get_remote_ui_handler (BonoboControl *control);

BonoboUIHandler *bonobo_view_get_ui_handler           (BonoboView    *view);
Bonobo_Unknown   bonobo_view_get_remote_ui_handler    (BonoboView    *view);

Bonobo_Unknown   bonobo_view_frame_get_ui_handler    (BonoboViewFrame *view_frame);
Bonobo_Unknown   bonobo_control_frame_get_ui_handler (BonoboControlFrame  *control_frame);

#endif /* _BONOBO_UI_HANDLER_H_ */
