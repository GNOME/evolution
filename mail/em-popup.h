/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
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

#ifndef __EM_POPUP_H__
#define __EM_POPUP_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* NB: This is TEMPORARY, to be replaced by EggMenu, if it does what we need? */

typedef struct _EMPopup EMPopup;
typedef struct _EMPopupClass EMPopupClass;

typedef struct _EMPopupItem EMPopupItem;
typedef struct _EMPopupFactory EMPopupFactory; /* anonymous type */
typedef struct _EMPopupTarget EMPopupTarget;

typedef void (*EMPopupFactoryFunc)(EMPopup *emp, EMPopupTarget *target, void *data);

/* Menu item descriptions */
enum _em_popup_t {
	EM_POPUP_ITEM = 0,
	EM_POPUP_TOGGLE,
	EM_POPUP_RADIO,
	EM_POPUP_IMAGE,
	EM_POPUP_SUBMENU,
	EM_POPUP_BAR,
	EM_POPUP_TYPE_MASK = 0xffff,
	EM_POPUP_ACTIVE = 0x10000,
};

struct _EMPopupItem {
	enum _em_popup_t type;
	char *path;		/* absolute path! must sort ascii-lexographically into the right spot */
	char *label;
	GCallback activate;
	void *activate_data;
	void *image;		/* char* for item type, GtkWidget * for image type */
	guint32 mask;
};

/* Current target description */
/* Types of popup tagets */
enum _em_popup_target_t {
	EM_POPUP_TARGET_SELECT,
	EM_POPUP_TARGET_URI,
	EM_POPUP_TARGET_PART,
	EM_POPUP_TARGET_FOLDER,
};

/* Flags that describe a TARGET_SELECT */
enum {
	EM_POPUP_SELECT_ONE		    = 1<<1,
	EM_POPUP_SELECT_MANY		    = 1<<2,
	EM_POPUP_SELECT_MARK_READ              = 1<<3,
	EM_POPUP_SELECT_MARK_UNREAD            = 1<<4,
	EM_POPUP_SELECT_DELETE                 = 1<<5,
	EM_POPUP_SELECT_UNDELETE               = 1<<6,
	EM_POPUP_SELECT_MAILING_LIST            = 1<<7,
	EM_POPUP_SELECT_RESEND                 = 1<<8,
	EM_POPUP_SELECT_MARK_IMPORTANT         = 1<<9,
	EM_POPUP_SELECT_MARK_UNIMPORTANT       = 1<<10,
	EM_POPUP_SELECT_FLAG_FOLLOWUP      = 1<<11,
	EM_POPUP_SELECT_FLAG_COMPLETED         = 1<<12,
	EM_POPUP_SELECT_FLAG_CLEAR             = 1<<13,
	EM_POPUP_SELECT_ADD_SENDER             = 1<<14,
	EM_POPUP_SELECT_MARK_JUNK              = 1<<15,
	EM_POPUP_SELECT_MARK_NOJUNK            = 1<<16,
	EM_POPUP_SELECT_FOLDER		       = 1<<17,	/* do we have any folder at all? */
	EM_POPUP_SELECT_LAST = 1<<18 /* reserve 2 slots */
};

/* Flags that describe a TARGET_URI */
enum {
	EM_POPUP_URI_HTTP = 1<<0,
	EM_POPUP_URI_MAILTO = 1<<1,
	EM_POPUP_URI_NOT_MAILTO = 1<<2,
};

/* Flags that describe TARGET_PART */
enum {
	EM_POPUP_PART_MESSAGE = 1<<0,
	EM_POPUP_PART_IMAGE = 1<<1,
};

/* Flags that describe TARGET_FOLDER */
enum {
	EM_POPUP_FOLDER_FOLDER = 1<<0, /* normal folder */
	EM_POPUP_FOLDER_STORE = 1<<1, /* root/nonselectable folder, i.e. store */
	EM_POPUP_FOLDER_INFERIORS = 1<<2, /* folder can have children */
	EM_POPUP_FOLDER_DELETE = 1<<3, /* folder can be deleted/renamed */
	EM_POPUP_FOLDER_SELECT = 1<<4, /* folder can be selected/opened */
};

struct _EMPopupTarget {
	enum _em_popup_target_t type;
	guint32 mask;		/* depends on type, see above */
	struct _GtkWidget *widget;	/* used if you need a parent toplevel, if available */
	union {
		char *uri;
		struct {
			struct _CamelFolder *folder;
			char *folder_uri;
			GPtrArray *uids;
		} select;
		struct {
			char *mime_type;
			struct _CamelMimePart *part;
		} part;
		struct {
			char *folder_uri;
		} folder;
	} data;
};

/* The object */
struct _EMPopup {
	GObject object;

	struct _EMPopupPrivate *priv;

	char *menuid;
};

struct _EMPopupClass {
	GObjectClass object_class;
};

GType em_popup_get_type(void);

/* Static class methods */
EMPopupFactory *em_popup_static_add_factory(const char *menuid, EMPopupFactoryFunc func, void *data);
void em_popup_static_remove_factory(EMPopupFactory *f);

EMPopup *em_popup_new(const char *menuid);
void em_popup_add_items(EMPopup *, GSList *items, GDestroyNotify freefunc);
void em_popup_add_static_items(EMPopup *emp, EMPopupTarget *target);
struct _GtkMenu *em_popup_create_menu(EMPopup *, guint32 hide_mask, guint32 disable_mask);
struct _GtkMenu *em_popup_create_menu_once(EMPopup *emp, EMPopupTarget *, guint32 hide_mask, guint32 disable_mask);

EMPopupTarget *em_popup_target_new_uri(const char *uri);
EMPopupTarget *em_popup_target_new_select(struct _CamelFolder *folder, const char *folder_uri, GPtrArray *uids);
EMPopupTarget *em_popup_target_new_part(struct _CamelMimePart *part, const char *mime_type);
EMPopupTarget *em_popup_target_new_folder(const char *uri, guint32 info_flags, int isstore);
void em_popup_target_free(EMPopupTarget *target);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_POPUP_H__ */
