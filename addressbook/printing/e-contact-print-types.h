/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CONTACT_PRINT_TYPES_H
#define E_CONTACT_PRINT_TYPES_H

#include <glib.h>

typedef struct _EContactPrintStyle EContactPrintStyle;
typedef enum _EContactPrintType EContactPrintType;

enum _EContactPrintType {
	E_CONTACT_PRINT_TYPE_CARDS,
	E_CONTACT_PRINT_TYPE_MEMO_STYLE,
	E_CONTACT_PRINT_TYPE_PHONE_LIST
};

struct _EContactPrintStyle
{
	gchar *title;
	EContactPrintType type;
	gboolean sections_start_new_page;
	guint num_columns;
	guint blank_forms;
	gboolean letter_headings;
	PangoFontDescription *headings_font;
	PangoFontDescription *body_font;
	gboolean print_using_grey;
	gint paper_type;
	gdouble paper_width;
	gdouble paper_height;
	gint paper_source;
	gdouble top_margin;
	gdouble left_margin;
	gdouble bottom_margin;
	gdouble right_margin;
	gint page_size;
	gdouble page_width;
	gdouble page_height;
	gboolean orientation_portrait;
	PangoFontDescription *header_font;
	gchar *left_header;
	gchar *center_header;
	gchar *right_header;
	PangoFontDescription *footer_font;
	gchar *left_footer;
	gchar *center_footer;
	gchar *right_footer;
	gboolean reverse_on_even_pages;
};

#endif /* E_CONTACT_PRINT_TYPES_H */

