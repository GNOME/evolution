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

#include <libebook/e-contact.h>

#include "eab-menu.h"

static void eabm_standard_menu_factory(EMenu *emp, void *data);

static GObjectClass *eabm_parent;

static void
eabm_init(GObject *o)
{
	/*EABMenu *emp = (EABMenu *)o; */
}

static void
eabm_finalise(GObject *o)
{
	((GObjectClass *)eabm_parent)->finalize(o);
}

static void
eabm_target_free(EMenu *ep, EMenuTarget *t)
{
	switch (t->type) {
	case EAB_MENU_TARGET_SELECT: {
		EABMenuTargetSelect *s = (EABMenuTargetSelect *)t;
		int i;

		for (i=0;i<s->cards->len;i++)
			g_object_unref(s->cards->pdata[i]);
		g_ptr_array_free(s->cards, TRUE);
		g_object_unref(s->book);
		break; }
	}

	((EMenuClass *)eabm_parent)->target_free(ep, t);
}

static void
eabm_class_init(GObjectClass *klass)
{
	klass->finalize = eabm_finalise;
	((EMenuClass *)klass)->target_free = eabm_target_free;

	e_menu_class_add_factory((EMenuClass *)klass, NULL, (EMenuFactoryFunc)eabm_standard_menu_factory, NULL);
}

GType
eab_menu_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EABMenuClass),
			NULL, NULL,
			(GClassInitFunc)eabm_class_init,
			NULL, NULL,
			sizeof(EABMenu), 0,
			(GInstanceInitFunc)eabm_init
		};
		eabm_parent = g_type_class_ref(e_menu_get_type());
		type = g_type_register_static(e_menu_get_type(), "EABMenu", &info, 0);
	}

	return type;
}

EABMenu *eab_menu_new(const char *menuid)
{
	EABMenu *emp = g_object_new(eab_menu_get_type(), 0);

	e_menu_construct(&emp->menu, menuid);

	return emp;
}

/**
 * eab_menu_target_new_select - create a menu target of the current selection.
 * @eabp: Address book menu.
 * @book: Book the cards belong to.  May be NULL in which case cards must be an empty GPtrArray.
 * @readonly: Book is read-only mode.  FIXME: Why can't we just get this off the book?
 * @cards: Cards selected.  This will be freed on completion and the array indices unreferenced.
 *
 * Create a new selection menu target.
 * 
 * Return value: 
 **/
EABMenuTargetSelect *
eab_menu_target_new_select(EABMenu *eabp, struct _EBook *book, int readonly, GPtrArray *cards)
{
	EABMenuTargetSelect *t = e_menu_target_new(&eabp->menu, EAB_MENU_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	int has_email = FALSE, i;

	/* FIXME: duplicated in eab-popup.c */

	t->book = book;
	if (book)
		g_object_ref(book);
	t->cards = cards;

	for (i=0;i<cards->len && !has_email;i++) {
		EContact *contact = cards->pdata[i];
		GList *email;

		email = e_contact_get(E_CONTACT(contact), E_CONTACT_EMAIL);
		if (email) {
			has_email = TRUE;
			
			g_list_foreach(email, (GFunc)g_free, NULL);
			g_list_free(email);
		}
	}

	if (has_email)
		mask &= ~EAB_MENU_SELECT_EMAIL;

	if (!readonly)
		mask &= ~EAB_MENU_SELECT_EDITABLE;

	if (cards->len == 1)
		mask &= ~EAB_MENU_SELECT_ONE;

	if (cards->len > 1)
		mask &= ~EAB_MENU_SELECT_MANY;

	if (cards->len >= 1)
		mask &= ~EAB_MENU_SELECT_ANY;

	t->target.mask = mask;

	return t;
}

static void
eabm_standard_menu_factory(EMenu *emp, void *data)
{
	/* noop */
}

/* ********************************************************************** */

/* menu plugin handler */

/*
<e-plugin
  class="com.ximian.mail.plugin.popup:1.0"
  id="com.ximian.mail.plugin.popup.item:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="com.ximian.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <item
    type="item|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="eabm_view_emacs"/>
  </menu>
  </extension>

*/

static void *eabmph_parent_class;
#define eabmph ((EABMenuHook *)eph)

static const EMenuHookTargetMask eabmph_select_masks[] = {
	{ "one", EAB_MENU_SELECT_ONE },
	{ "many", EAB_MENU_SELECT_MANY },
	{ "any", EAB_MENU_SELECT_ANY },
	{ "editable", EAB_MENU_SELECT_EDITABLE },
	{ "email", EAB_MENU_SELECT_EMAIL },
	{ 0 }
};

static const EMenuHookTargetMap eabmph_targets[] = {
	{ "select", EAB_MENU_TARGET_SELECT, eabmph_select_masks },
	{ 0 }
};

static void
eabmph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)eabmph_parent_class)->finalize(o);
}

static void
eabmph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = eabmph_finalise;
	((EPluginHookClass *)klass)->id = "com.novell.evolution.addressbook.bonobomenu:1.0";

	for (i=0;eabmph_targets[i].type;i++)
		e_menu_hook_class_add_target_map((EMenuHookClass *)klass, &eabmph_targets[i]);

	/* FIXME: leaks parent set class? */
	((EMenuHookClass *)klass)->menu_class = g_type_class_ref(eab_menu_get_type());
}

GType
eab_menu_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EABMenuHookClass), NULL, NULL, (GClassInitFunc) eabmph_class_init, NULL, NULL,
			sizeof(EABMenuHook), 0, (GInstanceInitFunc) NULL,
		};

		eabmph_parent_class = g_type_class_ref(e_menu_hook_get_type());
		type = g_type_register_static(e_menu_hook_get_type(), "EABMenuHook", &info, 0);
	}
	
	return type;
}
