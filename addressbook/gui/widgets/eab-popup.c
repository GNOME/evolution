/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include "eab-popup.h"
#include <libedataserverui/e-source-selector.h>
#include <libebook/e-contact.h>

static GObjectClass *eabp_parent;

static void
eabp_init(GObject *o)
{
	/*EABPopup *eabp = (EABPopup *)o; */
}

static void
eabp_finalise(GObject *o)
{
	((GObjectClass *)eabp_parent)->finalize(o);
}

static void
eabp_target_free(EPopup *ep, EPopupTarget *t)
{
	switch (t->type) {
	case EAB_POPUP_TARGET_SELECT: {
		EABPopupTargetSelect *s = (EABPopupTargetSelect *)t;
		int i;

		for (i=0;i<s->cards->len;i++)
			g_object_unref(s->cards->pdata[i]);
		g_ptr_array_free(s->cards, TRUE);
		g_object_unref(s->book);

		break; }
	case EAB_POPUP_TARGET_SOURCE: {
		EABPopupTargetSource *s = (EABPopupTargetSource *)t;

		g_object_unref(s->selector);
		break; }
	case EAB_POPUP_TARGET_SELECT_NAMES: {
		EABPopupTargetSelectNames *s = (EABPopupTargetSelectNames *)t;

		g_object_unref(s->model);
		break; }
	}

	((EPopupClass *)eabp_parent)->target_free(ep, t);
}

static void
eabp_class_init(GObjectClass *klass)
{
	klass->finalize = eabp_finalise;
	((EPopupClass *)klass)->target_free = eabp_target_free;
}

GType
eab_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EABPopupClass),
			NULL, NULL,
			(GClassInitFunc)eabp_class_init,
			NULL, NULL,
			sizeof(EABPopup), 0,
			(GInstanceInitFunc)eabp_init
		};
		eabp_parent = g_type_class_ref(e_popup_get_type());
		type = g_type_register_static(e_popup_get_type(), "EABPopup", &info, 0);
	}

	return type;
}

EABPopup *eab_popup_new(const char *menuid)
{
	EABPopup *eabp = g_object_new(eab_popup_get_type(), 0);

	e_popup_construct(&eabp->popup, menuid);

	return eabp;
}

/**
 * eab_popup_target_new_select:
 * @eabp: Address book popup.
 * @book: Book the cards belong to.
 * @readonly: Book is read-only mode.  FIXME: Why can't we just get this off the book?
 * @cards: Cards selected.  This will be freed on completion.
 *
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
EABPopupTargetSelect *
eab_popup_target_new_select(EABPopup *eabp, struct _EBook *book, int readonly, GPtrArray *cards)
{
	EABPopupTargetSelect *t = e_popup_target_new(&eabp->popup, EAB_POPUP_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	int has_email = FALSE, i;

	/* FIXME: duplicated in eab-menu.c */

	t->book = book;
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
		mask &= ~EAB_POPUP_SELECT_EMAIL;

	if (!readonly)
		mask &= ~EAB_POPUP_SELECT_EDITABLE;

	if (cards->len == 1)
		mask &= ~EAB_POPUP_SELECT_ONE;

	if (cards->len > 1)
		mask &= ~EAB_POPUP_SELECT_MANY;

	if (cards->len >= 1)
		mask &= ~EAB_POPUP_SELECT_ANY;

	t->target.mask = mask;

	return t;
}

EABPopupTargetSource *
eab_popup_target_new_source(EABPopup *eabp, ESourceSelector *selector)
{
	EABPopupTargetSource *t = e_popup_target_new(&eabp->popup, EAB_POPUP_TARGET_SOURCE, sizeof(*t));
	guint32 mask = ~0;
	const char *source_uri;
	ESource *source;

	/* TODO: this is duplicated for calendar and tasks too */

	t->selector = selector;
	g_object_ref(selector);

	/* TODO: perhaps we need to copy this so it doesn't change during the lifecycle */
	source = e_source_selector_peek_primary_selection(selector);
	if (source)
		mask &= ~EAB_POPUP_SOURCE_PRIMARY;

	/* FIXME Gross hack, should have a property or something */
	source_uri = e_source_peek_relative_uri(source);
	if (source_uri && !strcmp("system", source_uri))
		mask &= ~EAB_POPUP_SOURCE_SYSTEM;
	else
		mask &= ~EAB_POPUP_SOURCE_USER;

	t->target.mask = mask;

	return t;
}

EABPopupTargetSelectNames *
eab_popup_target_new_select_names(EABPopup *eabp, struct _ESelectNamesModel *model, int row)
{
	EABPopupTargetSelectNames *t = e_popup_target_new(&eabp->popup, EAB_POPUP_TARGET_SELECT_NAMES, sizeof(*t));

	/* TODO: this is sort of not very useful, maybe the popup which uses it doesn't
	   need to be pluggable */

	t->model = model;
	g_object_ref(model);
	t->row = row;

	return t;
}

/* ********************************************************************** */
/* Popup menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.iteab:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <iteab
    type="iteab|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="eabp_view_eabacs"/>
  </menu>
  </extension>

*/

static void *eabph_parent_class;
#define eabph ((EABPopupHook *)eph)

static const EPopupHookTargetMask eabph_select_masks[] = {
	{ "one", EAB_POPUP_SELECT_ONE },
	{ "many", EAB_POPUP_SELECT_MANY },
	{ "any", EAB_POPUP_SELECT_ANY },
	{ "editable", EAB_POPUP_SELECT_EDITABLE },
	{ "email", EAB_POPUP_SELECT_EMAIL },
	{ 0 }
};

static const EPopupHookTargetMask eabph_source_masks[] = {
	{ "primary", EAB_POPUP_SOURCE_PRIMARY },
	{ "system", EAB_POPUP_SOURCE_SYSTEM },
	{ 0 }
};

static const EPopupHookTargetMask eabph_select_names_masks[] = {
	{ 0 }
};

static const EPopupHookTargetMap eabph_targets[] = {
	{ "select", EAB_POPUP_TARGET_SELECT, eabph_select_masks },
	{ "source", EAB_POPUP_TARGET_SOURCE, eabph_source_masks },
	{ "select-names", EAB_POPUP_TARGET_SELECT_NAMES, eabph_select_names_masks },
	{ 0 }
};

static void
eabph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)eabph_parent_class)->finalize(o);
}

static void
eabph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = eabph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.addressbook.popup:1.0";

	for (i=0;eabph_targets[i].type;i++)
		e_popup_hook_class_add_target_map((EPopupHookClass *)klass, &eabph_targets[i]);

	((EPopupHookClass *)klass)->popup_class = g_type_class_ref(eab_popup_get_type());
}

GType
eab_popup_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(EABPopupHookClass), NULL, NULL, (GClassInitFunc) eabph_class_init, NULL, NULL,
			sizeof(EABPopupHook), 0, (GInstanceInitFunc) NULL,
		};

		eabph_parent_class = g_type_class_ref(e_popup_hook_get_type());
		type = g_type_register_static(e_popup_hook_get_type(), "EABPopupHook", &info, 0);
	}
	
	return type;
}
