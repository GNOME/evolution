/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef _EM_FOLDER_VIEW_H
#define _EM_FOLDER_VIEW_H

#include <gtk/gtkvbox.h>
#include "em-popup.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _MessageList;
struct _EMFormatHTMLDisplay;
struct _CamelFolder;
struct _CamelMedium;

#define EM_FOLDER_VIEW_GET_CLASS(emfv)  ((EMFolderViewClass *) G_OBJECT_GET_CLASS (emfv))

typedef struct _EMFolderView EMFolderView;
typedef struct _EMFolderViewClass EMFolderViewClass;

typedef struct _EMFolderViewEnable EMFolderViewEnable;

enum {
	EM_FOLDER_VIEW_SELECT_THREADED = EM_POPUP_SELECT_LAST,
	EM_FOLDER_VIEW_SELECT_HIDDEN = EM_POPUP_SELECT_LAST<<1,
	EM_FOLDER_VIEW_SELECT_LAST = EM_POPUP_SELECT_LAST<<2,
};

struct _EMFolderViewEnable {
	const char *name;	/* bonobo name, relative to /commands/ */
	guint32 mask;		/* disable mask, see EM_FOLDER_VIEW_CAN* flags */
};

struct _EMFolderView {
	GtkVBox parent;

	struct _EMFolderViewPrivate *priv;

	struct _MessageList *list;

	struct _EMFormatHTMLDisplay *preview;

	struct _CamelFolder *folder;
	char *folder_uri;

	char *displayed_uid;	/* only used to stop re-loads, don't use it to represent any selection state */

	/* used to load ui from base activate implementation */
	GSList *ui_files;	/* const char * list, TODO: should this be on class? */
	const char *ui_app_name;

	/* for proxying jobs to main or other threads */
	struct _MailAsyncEvent *async;

	struct _BonoboUIComponent *uic;	/* if we're active, this will be set */
	GSList *enable_map;	/* bonobo menu enable map, entries are 0-terminated EMFolderViewEnable arryas
				   TODO: should this be on class? */
	
	int mark_seen_timeout;	/* local copy of gconf stuff */
	int mark_seen:1;
	int preview_active:1;	/* is preview being used */
	int statusbar_active:1; /* should we manage the statusbar messages ourselves? */
	int hide_deleted:1;
	int list_active:1;	/* we actually showing the list? */
};

struct _EMFolderViewClass {
	GtkVBoxClass parent_class;
	
	/* behaviour definition */
	int update_message_style:1;
	
	/* if used as a control, used to activate/deactivate custom menu's */
	void (*activate)(EMFolderView *, struct _BonoboUIComponent *uic, int state);

	void (*set_folder_uri)(EMFolderView *emfv, const char *uri);
	void (*set_folder)(EMFolderView *emfv, struct _CamelFolder *folder, const char *uri);
	void (*set_message)(EMFolderView *emfv, const char *uid, int nomarkseen);

	/* Signals */
	void (*on_url)(EMFolderView *emfv, const char *uri, const char *nice_uri);

	void (*loaded)(EMFolderView *emfv);
	void (*changed)(EMFolderView *emfv);
};

GType em_folder_view_get_type(void);

GtkWidget *em_folder_view_new(void);

#define em_folder_view_activate(emfv, uic, state) EM_FOLDER_VIEW_GET_CLASS (emfv)->activate((emfv), (uic), (state))
#define em_folder_view_set_folder(emfv, folder, uri) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_folder((emfv), (folder), (uri))
#define em_folder_view_set_folder_uri(emfv, uri) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_folder_uri((emfv), (uri))
#define em_folder_view_set_message(emfv, uid, nomarkseen) EM_FOLDER_VIEW_GET_CLASS (emfv)->set_message((emfv), (uid), (nomarkseen))

struct _EMPopupTarget *em_folder_view_get_popup_target(EMFolderView *emfv);

int em_folder_view_mark_selected(EMFolderView *emfv, guint32 mask, guint32 set);
int em_folder_view_open_selected(EMFolderView *emfv);

int em_folder_view_print(EMFolderView *emfv, int preview);

/* this could be on message-list */
guint32 em_folder_view_disable_mask(EMFolderView *emfv);

void em_folder_view_set_statusbar(EMFolderView *emfv, gboolean statusbar);
void em_folder_view_set_hide_deleted(EMFolderView *emfv, gboolean status);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _EM_FOLDER_VIEW_H */
