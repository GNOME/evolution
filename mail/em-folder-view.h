/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FOLDER_VIEW_H
#define EM_FOLDER_VIEW_H

#include <gtk/gtk.h>
#include <bonobo/bonobo-ui-component.h>

#include "mail/em-format-html-display.h"
#include "mail/em-menu.h"
#include "mail/em-popup.h"
#include "mail/mail-mt.h"
#include "mail/message-list.h"

/* Standard GObject macros */
#define EM_TYPE_FOLDER_VIEW \
	(em_folder_view_get_type ())
#define EM_FOLDER_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FOLDER_VIEW, EMFolderView))
#define EM_FOLDER_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FOLDER_VIEW, EMFolderViewClass))
#define EM_IS_FOLDER_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FOLDER_VIEW))
#define EM_IS_FOLDER_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FOLDER_VIEW))
#define EM_FOLDER_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FOLDER_VIEW, EMFolderViewClass))

G_BEGIN_DECLS

typedef struct _EMFolderView EMFolderView;
typedef struct _EMFolderViewClass EMFolderViewClass;

typedef struct _EMFolderViewEnable EMFolderViewEnable;

enum {
	EM_FOLDER_VIEW_SELECT_THREADED = EM_POPUP_SELECT_LAST,
	EM_FOLDER_VIEW_SELECT_HIDDEN = EM_POPUP_SELECT_LAST<<1,
	EM_FOLDER_VIEW_SELECT_NEXT_MSG = EM_POPUP_SELECT_LAST<<2,
	EM_FOLDER_VIEW_SELECT_PREV_MSG = EM_POPUP_SELECT_LAST<<3,
	EM_FOLDER_VIEW_SELECT_LISTONLY = EM_POPUP_SELECT_LAST<<4,
	EM_FOLDER_VIEW_SELECT_DISPLAY = EM_POPUP_SELECT_LAST<<5,
	EM_FOLDER_VIEW_SELECT_SELECTION = EM_POPUP_SELECT_LAST<<6,
	EM_FOLDER_VIEW_SELECT_NOSELECTION = EM_POPUP_SELECT_LAST<<7,
	EM_FOLDER_VIEW_PREVIEW_PRESENT = EM_POPUP_SELECT_LAST<<8,
	EM_FOLDER_VIEW_SELECT_LAST = EM_POPUP_SELECT_LAST<<9
};

struct _EMFolderViewEnable {
	const gchar *name;	/* bonobo name, relative to /commands/ */
	guint32 mask;		/* disable mask, see EM_FOLDER_VIEW_CAN* flags */
};

struct _EMFolderView {
	GtkVBox parent;

	struct _EMFolderViewPrivate *priv;

	MessageList *list;

	EMFormatHTMLDisplay *preview;

	CamelFolder *folder;
	gchar *folder_uri;

	gchar *displayed_uid;	/* only used to stop re-loads, don't use it to represent any selection state */

	/* used to load ui from base activate implementation */
	GSList *ui_files;	/* const gchar * list, TODO: should this be on class? */
	const gchar *ui_app_name;

	/* used to manage some menus, particularly plugins */
	EMMenu *menu;

	/* for proxying jobs to main or other threads */
	MailAsyncEvent *async;

	BonoboUIComponent *uic;	/* if we're active, this will be set */
	GSList *enable_map;	/* bonobo menu enable map, entries are 0-terminated EMFolderViewEnable arryas
				   TODO: should this be on class? */

	gint mark_seen_timeout;	/* local copy of gconf stuff */
	guint mark_seen:1;
	guint preview_active:1;	/* is preview being used */
	guint statusbar_active:1; /* should we manage the statusbar messages ourselves? */
	guint hide_deleted:1;
	guint list_active:1;	/* we actually showing the list? */
};

struct _EMFolderViewClass {
	GtkVBoxClass parent_class;

	/* behaviour definition */
	guint update_message_style:1;

	/* if used as a control, used to activate/deactivate custom menu's */
	void (*activate)(EMFolderView *, BonoboUIComponent *uic, gint state);

	void (*set_folder_uri)(EMFolderView *emfv, const gchar *uri);
	void (*set_folder)(EMFolderView *emfv, CamelFolder *folder, const gchar *uri);
	void (*set_message)(EMFolderView *emfv, const gchar *uid, gint nomarkseen);

	void (*show_search_bar)(EMFolderView *emfv);

	/* Signals */
	void (*on_url)(EMFolderView *emfv, const gchar *uri, const gchar *nice_uri);

	void (*loaded)(EMFolderView *emfv);
	void (*changed)(EMFolderView *emfv);
};

GType em_folder_view_get_type(void);

GtkWidget *em_folder_view_new(void);

#define em_folder_view_activate(emfv, uic, state) EM_FOLDER_VIEW_GET_CLASS (emfv)->activate((emfv), (uic), (state))
#define em_folder_view_set_folder(emfv, folder, uri) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_folder((emfv), (folder), (uri))
#define em_folder_view_set_folder_uri(emfv, uri) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_folder_uri((emfv), (uri))
#define em_folder_view_set_message(emfv, uid, nomarkseen) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_message((emfv), (uid), (nomarkseen))

EMPopupTargetSelect *em_folder_view_get_popup_target(EMFolderView *emfv, EMPopup *emp, gint on_display);

gint em_folder_view_mark_selected(EMFolderView *emfv, guint32 mask, guint32 set);
gint em_folder_view_open_selected(EMFolderView *emfv);

gint em_folder_view_print(EMFolderView *emfv, GtkPrintOperationAction action);

/* this could be on message-list */
guint32 em_folder_view_disable_mask(EMFolderView *emfv);

void em_folder_view_set_statusbar(EMFolderView *emfv, gboolean statusbar);
void em_folder_view_set_hide_deleted(EMFolderView *emfv, gboolean status);
void em_folder_view_setup_view_instance (EMFolderView *emfv);
void em_folder_view_show_search_bar (EMFolderView *emfv);

G_END_DECLS

#endif /* EM_FOLDER_VIEW_H */
