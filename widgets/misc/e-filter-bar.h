/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-filter-bar.h
 * Copyright (C) 2001 Ximian Inc.
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gtk/gtkobject.h>
#include <gtk/gtkwidget.h>
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
 */

#define E_FILTER_BAR_TYPE			(e_filter_bar_get_type ())
#define E_FILTER_BAR(obj)			(GTK_CHECK_CAST ((obj), E_FILTER_BAR_TYPE, EFilterBar))
#define E_FILTER_BAR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_FILTER_BAR_TYPE, EFilterBarClass))
#define E_IS_FILTER_BAR(obj)		(GTK_CHECK_TYPE ((obj), E_FILTER_BAR_TYPE))
#define E_IS_FILTER_BAR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_FILTER_BAR_TYPE))

typedef struct _EFilterBar       EFilterBar;
typedef struct _EFilterBarClass  EFilterBarClass;

typedef void (*EFilterBarConfigRule)(EFilterBar *, FilterRule *rule, int id, const char *query, void *data);

struct _EFilterBar
{
	ESearchBar parent;

	int menu_base, option_base;
	GPtrArray *menu_rules, *option_rules;

	GtkWidget *save_dialogue; /* current save dialogue (so we dont pop up multiple ones) */

	FilterRule *current_query; /* as it says */
	int setquery;		/* true when we're setting a query directly to advanced, so dont popup the dialogue */

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
	E_FILTERBAR_RESET_ID = -2,
	E_FILTERBAR_SAVE_ID = -3,
	E_FILTERBAR_EDIT_ID = -4,

	/* preset option options */
	E_FILTERBAR_ADVANCED_ID = -5,

	E_FILTERBAR_LAST_ID = -6,
};

#define E_FILTERBAR_SAVE { N_("Save As ..."), E_FILTERBAR_SAVE_ID }
#define E_FILTERBAR_RESET { N_("Show All"), E_FILTERBAR_RESET_ID }
#define E_FILTERBAR_EDIT { N_("Edit ..."), E_FILTERBAR_EDIT_ID }
#define E_FILTERBAR_ADVANCED { N_("Advanced ..."), E_FILTERBAR_ADVANCED_ID }


GtkType    e_filter_bar_get_type   (void);
EFilterBar*e_filter_bar_new        (RuleContext *, const char *sys, const char *user, EFilterBarConfigRule config, void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_FILTER_BAR_H__ */
