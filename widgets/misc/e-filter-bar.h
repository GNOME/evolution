/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-filter-bar.h
 * Copyright (C) 2001 Ximian Inc.
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_FILTER_BAR_H__
#define __E_FILTER_BAR_H__

#include <gtk/gtk.h>

#include "e-search-bar.h"

#include "filter/rule-context.h"
#include "filter/filter-rule.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EFilterBar - A filter rule driven search bar.
 *
 * The following arguments are available:
 *
 * name		 type		read/write	description
 * ---------------------------------------------------------------------------------
 * query         string         R               String representing query.
 * state         string         RW              XML string representing the state.
 */

#define E_FILTER_BAR_TYPE			(e_filter_bar_get_type ())
#define E_FILTER_BAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_FILTER_BAR_TYPE, EFilterBar))
#define E_FILTER_BAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_FILTER_BAR_TYPE, EFilterBarClass))
#define E_IS_FILTER_BAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_FILTER_BAR_TYPE))
#define E_IS_FILTER_BAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_FILTER_BAR_TYPE))

typedef struct _EFilterBar       EFilterBar;
typedef struct _EFilterBarClass  EFilterBarClass;

typedef void (*EFilterBarConfigRule)(EFilterBar *, FilterRule *rule, int id, const char *query, void *data);

struct _EFilterBar {
	ESearchBar parent;
	
	int menu_base, option_base;
	GPtrArray *menu_rules, *option_rules;
	
	ESearchBarItem *default_items;
	
	GtkWidget *save_dialog;    /* current save dialogue (so we dont pop up multiple ones) */
	
	FilterRule *current_query; /* as it says */
	int setquery;		   /* true when we're setting a query directly to advanced, so dont popup the dialog */
	
	RuleContext *context;
	char *systemrules;
	char *userrules;
	
	EFilterBarConfigRule config;
	void *config_data;
};

struct _EFilterBarClass
{
	ESearchBarClass parent_class;
};

/* "preset" items */
enum {
	/* preset menu options */
	E_FILTERBAR_SAVE_ID = -3,
	E_FILTERBAR_EDIT_ID = -4,

	/* preset option options */
	E_FILTERBAR_ADVANCED_ID = -5,
};

#define E_FILTERBAR_SAVE      { N_("_Save Search..."), E_FILTERBAR_SAVE_ID, NULL }
#define E_FILTERBAR_EDIT      { N_("_Edit Saved Searches..."), E_FILTERBAR_EDIT_ID, NULL }
#define E_FILTERBAR_ADVANCED  { N_("_Advanced Search..."), E_FILTERBAR_ADVANCED_ID, NULL }
#define E_FILTERBAR_SEPARATOR { NULL, 0, NULL }

#ifdef JUST_FOR_TRANSLATORS
const char * strings[] = {
	N_("_Save Search..."),
	N_("_Edit Saved Searches..."),
	N_("_Advanced Search...")
};
#endif


GType       e_filter_bar_get_type (void);

EFilterBar *e_filter_bar_new      (RuleContext *context,
				   const char *systemrules,
				   const char *userrules,
				   EFilterBarConfigRule config,
				   void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_FILTER_BAR_H__ */
