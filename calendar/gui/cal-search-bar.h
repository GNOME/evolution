/* Evolution calendar - Search bar widget for calendar views
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CAL_SEARCH_BAR_H
#define CAL_SEARCH_BAR_H

#include "widgets/misc/e-search-bar.h"
#include "widgets/misc/e-filter-bar.h"

G_BEGIN_DECLS



#define TYPE_CAL_SEARCH_BAR            (cal_search_bar_get_type ())
#define CAL_SEARCH_BAR(obj)            (GTK_CHECK_CAST ((obj), TYPE_CAL_SEARCH_BAR, CalSearchBar))
#define CAL_SEARCH_BAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_CAL_SEARCH_BAR,	\
					CalSearchBarClass))
#define IS_CAL_SEARCH_BAR(obj)         (GTK_CHECK_TYPE ((obj), TYPE_CAL_SEARCH_BAR))
#define IS_CAL_SEARCH_BAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TYPE_CAL_SEARCH_BAR))

typedef struct CalSearchBarPrivate CalSearchBarPrivate;

enum {
	CAL_SEARCH_ANY_FIELD_CONTAINS    = (1 << 0),
	CAL_SEARCH_SUMMARY_CONTAINS      = (1 << 1),
	CAL_SEARCH_DESCRIPTION_CONTAINS  = (1 << 2),
	CAL_SEARCH_COMMENT_CONTAINS      = (1 << 3),
	CAL_SEARCH_LOCATION_CONTAINS     = (1 << 4),
	CAL_SEARCH_CATEGORY_IS           = (1 << 5)
};

#define CAL_SEARCH_ALL (0xff)
#define CAL_SEARCH_CALENDAR_DEFAULT (0x37)
#define CAL_SEARCH_TASKS_DEFAULT    (0x27)

typedef struct {
	ESearchBar search_bar;

	/* Private data */
	CalSearchBarPrivate *priv;
} CalSearchBar;

typedef struct {
	ESearchBarClass parent_class;

	/* Notification signals */

	void (* sexp_changed) (CalSearchBar *cal_search, const char *sexp);
	void (* category_changed) (CalSearchBar *cal_search, const char *category);
} CalSearchBarClass;

GtkType cal_search_bar_get_type (void);

CalSearchBar *cal_search_bar_construct (CalSearchBar *cal_search, guint32 flags);

GtkWidget *cal_search_bar_new (guint32 flags);

void cal_search_bar_set_categories (CalSearchBar *cal_search, GPtrArray *categories);

const char *cal_search_bar_get_category (CalSearchBar *cal_search);



G_END_DECLS

#endif
