/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print-types.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
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

#ifndef E_CONTACT_PRINT_TYPES_H
#define E_CONTACT_PRINT_TYPES_H

#include <gnome.h>
#include <libgnomeprint/gnome-font.h>

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
	gboolean letter_tabs;
	gboolean letter_headings;
	GnomeFont *headings_font;
	GnomeFont *body_font;
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
	GnomeFont *header_font;
	gchar *left_header;
	gchar *center_header;
	gchar *right_header;
	GnomeFont *footer_font;
	gchar *left_footer;
	gchar *center_footer;
	gchar *right_footer;
	gboolean reverse_on_even_pages;
};

#endif /* E_CONTACT_PRINT_TYPES_H */

