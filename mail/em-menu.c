/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include <libgnome/gnome-url.h>
#include <libgnomevfs/gnome-vfs-mime.h>

#include "em-menu.h"
#include "libedataserver/e-msgport.h"
#include <e-util/e-icon-factory.h>
#include "em-utils.h"
#include "em-composer-utils.h"

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-url.h>

#include <camel/camel-vee-folder.h>
#include <camel/camel-vtrash-folder.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <gal/util/e-util.h>

static void emp_standard_menu_factory(EMenu *emp, void *data);

static GObjectClass *emp_parent;

static void
emp_init(GObject *o)
{
	/*EMMenu *emp = (EMMenu *)o; */
}

static void
emp_finalise(GObject *o)
{
	((GObjectClass *)emp_parent)->finalize(o);
}

static void
emp_target_free(EMenu *ep, EMenuTarget *t)
{
	switch (t->type) {
	case EM_MENU_TARGET_SELECT: {
		EMMenuTargetSelect *s = (EMMenuTargetSelect *)t;

		if (s->folder)
			camel_object_unref(s->folder);
		g_free(s->uri);
		if (s->uids)
			em_utils_uids_free(s->uids);
		break; }
	}

	((EMenuClass *)emp_parent)->target_free(ep, t);
}

static void
emp_class_init(GObjectClass *klass)
{
	printf("em menu class init\n");

	klass->finalize = emp_finalise;
	((EMenuClass *)klass)->target_free = emp_target_free;

	e_menu_class_add_factory((EMenuClass *)klass, NULL, (EMenuFactoryFunc)emp_standard_menu_factory, NULL);
}

GType
em_menu_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMMenuClass),
			NULL, NULL,
			(GClassInitFunc)emp_class_init,
			NULL, NULL,
			sizeof(EMMenu), 0,
			(GInstanceInitFunc)emp_init
		};
		emp_parent = g_type_class_ref(e_menu_get_type());
		type = g_type_register_static(e_menu_get_type(), "EMMenu", &info, 0);
	}

	return type;
}

EMMenu *em_menu_new(const char *menuid)
{
	EMMenu *emp = g_object_new(em_menu_get_type(), 0);

	e_menu_construct(&emp->popup, menuid);

	return emp;
}

/**
 * em_menu_target_new_select:
 * @folder: The selection will ref this for the life of it.
 * @folder_uri: 
 * @uids: The selection will free this when done with it.
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
EMMenuTargetSelect *
em_menu_target_new_select(EMMenu *emp, struct _CamelFolder *folder, const char *folder_uri, GPtrArray *uids)
{
	EMMenuTargetSelect *t = e_menu_target_new(&emp->popup, EM_MENU_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	int i;
	const char *tmp;

	/* NB: This is identical to em-popup-target-new-select function */

	t->uids = uids;
	t->folder = folder;
	t->uri = g_strdup(folder_uri);

	if (folder == NULL) {
		t->target.mask = mask;

		return t;
	}

	camel_object_ref(folder);
	mask &= ~EM_MENU_SELECT_FOLDER;
	
	if (em_utils_folder_is_sent(folder, folder_uri))
		mask &= ~EM_MENU_SELECT_EDIT;
	
	if (!(em_utils_folder_is_drafts(folder, folder_uri)
	      || em_utils_folder_is_outbox(folder, folder_uri))
	    && uids->len == 1)
		mask &= ~EM_MENU_SELECT_ADD_SENDER;

	if (uids->len == 1)
		mask &= ~EM_MENU_SELECT_ONE;

	if (uids->len >= 1)
		mask &= ~EM_MENU_SELECT_MANY;

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info = camel_folder_get_message_info(folder, uids->pdata[i]);
		guint32 flags;

		if (info == NULL)
			continue;

		flags = camel_message_info_flags(info);

		if (flags & CAMEL_MESSAGE_SEEN)
			mask &= ~EM_MENU_SELECT_MARK_UNREAD;
		else
			mask &= ~EM_MENU_SELECT_MARK_READ;
		
		if (flags & CAMEL_MESSAGE_DELETED)
			mask &= ~EM_MENU_SELECT_UNDELETE;
		else
			mask &= ~EM_MENU_SELECT_DELETE;

		if (flags & CAMEL_MESSAGE_FLAGGED)
			mask &= ~EM_MENU_SELECT_MARK_UNIMPORTANT;
		else
			mask &= ~EM_MENU_SELECT_MARK_IMPORTANT;

		if (flags & CAMEL_MESSAGE_JUNK)
			mask &= ~EM_MENU_SELECT_MARK_NOJUNK;
		else
			mask &= ~EM_MENU_SELECT_MARK_JUNK;
			
		tmp = camel_message_info_user_tag(info, "follow-up");
		if (tmp && *tmp) {
			mask &= ~EM_MENU_SELECT_FLAG_CLEAR;
			tmp = camel_message_info_user_tag(info, "completed-on");
			if (tmp == NULL || *tmp == 0)
				mask &= ~EM_MENU_SELECT_FLAG_COMPLETED;
		} else
			mask &= ~EM_MENU_SELECT_FLAG_FOLLOWUP;

		if (i == 0 && uids->len == 1
		    && (tmp = camel_message_info_mlist(info))
		    && tmp[0] != 0)
			mask &= ~EM_MENU_SELECT_MAILING_LIST;

		camel_folder_free_message_info(folder, info);
	}

	t->target.mask = mask;

	return t;
}

static void
emp_standard_menu_factory(EMenu *emp, void *data)
{
	/* noop */
}

/* ********************************************************************** */

/* menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="emp_view_emacs"/>
  </menu>
  </extension>

*/

static void *emph_parent_class;
#define emph ((EMMenuHook *)eph)

static const EMenuHookTargetMask emph_select_masks[] = {
	{ "one", EM_MENU_SELECT_ONE },
	{ "many", EM_MENU_SELECT_MANY },
	{ "mark_read", EM_MENU_SELECT_MARK_READ },
	{ "mark_unread", EM_MENU_SELECT_MARK_UNREAD },
	{ "delete", EM_MENU_SELECT_DELETE },
	{ "undelete", EM_MENU_SELECT_UNDELETE },
	{ "mailing_list", EM_MENU_SELECT_MAILING_LIST },
	{ "resend", EM_MENU_SELECT_EDIT },
	{ "mark_important", EM_MENU_SELECT_MARK_IMPORTANT },
	{ "mark_unimportant", EM_MENU_SELECT_MARK_UNIMPORTANT },
	{ "flag_followup", EM_MENU_SELECT_FLAG_FOLLOWUP },
	{ "flag_completed", EM_MENU_SELECT_FLAG_COMPLETED },
	{ "flag_clear", EM_MENU_SELECT_FLAG_CLEAR },
	{ "add_sender", EM_MENU_SELECT_ADD_SENDER },
	{ "mark_junk", EM_MENU_SELECT_MARK_JUNK },
	{ "mark_nojunk", EM_MENU_SELECT_MARK_NOJUNK },
	{ "folder", EM_MENU_SELECT_FOLDER },
	{ 0 }
};

static const EMenuHookTargetMap emph_targets[] = {
	{ "select", EM_MENU_TARGET_SELECT, emph_select_masks },
	{ 0 }
};

static void
emph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)emph_parent_class)->finalize(o);
}

static void
emph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = emph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.mail.bonobomenu:1.0";

	for (i=0;emph_targets[i].type;i++)
		e_menu_hook_class_add_target_map((EMenuHookClass *)klass, &emph_targets[i]);

	/* FIXME: leaks parent set class? */
	((EMenuHookClass *)klass)->menu_class = g_type_class_ref(em_menu_get_type());
}

GType
em_menu_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMMenuHookClass), NULL, NULL, (GClassInitFunc) emph_class_init, NULL, NULL,
			sizeof(EMMenuHook), 0, (GInstanceInitFunc) NULL,
		};

		emph_parent_class = g_type_class_ref(e_menu_hook_get_type());
		type = g_type_register_static(e_menu_hook_get_type(), "EMMenuHook", &info, 0);
	}
	
	return type;
}
