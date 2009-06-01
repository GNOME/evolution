/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <libedataserver/e-flag.h>
#include <libedataserver/e-xml-utils.h>

#include "e-util/e-print.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "e-contact-print.h"

typedef struct _EContactPrintContext EContactPrintContext;
typedef struct _ContactPrintItem ContactPrintItem;

struct _EContactPrintContext
{
	GtkPrintContext *context;
	gdouble x;
	gdouble y;
	gint column;
	gdouble column_width;
	gdouble column_spacing;
	EContactPrintStyle *style;
	gboolean first_section;

	gint page_nr, pages;

	PangoFontDescription *letter_heading_font;
	gchar *section;
	gboolean first_contact;

	EBook *book;
	EBookQuery *query;

	GList *contact_list;
};

static gdouble
get_font_height (PangoFontDescription *desc)
{
	return pango_units_to_double (
		pango_font_description_get_size (desc));
}

static gdouble
get_font_width (GtkPrintContext *context,
                PangoFontDescription *desc,
                const gchar *text)
{
	PangoLayout *layout;
	gint width, height;

	g_return_val_if_fail (desc, .0);
	g_return_val_if_fail (text, .0);

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, -1);
	pango_layout_set_indent (layout, 0);

	pango_layout_get_size (layout, &width, &height);

	g_object_unref (layout);

	return pango_units_to_double (width);
}

static void
e_contact_output (GtkPrintContext *context,
                  PangoFontDescription *font,
                  gdouble x, gdouble y, gdouble width,
                  const gchar *text)
{
	PangoLayout *layout;
	gdouble indent;
	cairo_t *cr;

	layout = gtk_print_context_create_pango_layout (context);

	if (width == -1 || get_font_width (context, font, text) <= width)
		indent = .0;
	else
		indent = get_font_width (context, font, "     ");

	pango_layout_set_font_description (layout, font);
	pango_layout_set_text (layout, text, -1);
	pango_layout_set_width (layout, pango_units_from_double (width));
	pango_layout_set_indent (layout, pango_units_from_double (indent));

	cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	g_object_unref (layout);
}

static gdouble
e_contact_text_height (GtkPrintContext *context,
                       PangoFontDescription *desc,
                       const gchar *text)
{
	PangoLayout *layout;
	gint width, height;

	layout = gtk_print_context_create_pango_layout (context);

	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, text, -1);

	pango_layout_get_size (layout, &width, &height);

	g_object_unref (layout);

	return pango_units_to_double (height);
}

static void
e_contact_print_letter_heading (EContactPrintContext *ctxt, gchar *letter)
{
	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoFontMetrics *metrics;
	gint width, height;
	cairo_t *cr;

	desc = ctxt->letter_heading_font;

	layout = gtk_print_context_create_pango_layout (ctxt->context);

	/* Make the rectangle thrice the average character width.
	 * XXX Works well for English, what about other locales? */
	metrics = pango_context_get_metrics (
		pango_layout_get_context (layout),
		desc, pango_language_get_default ());
	width = pango_font_metrics_get_approximate_char_width (metrics) * 3;
	pango_font_metrics_unref (metrics);

	pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, letter, -1);
	pango_layout_set_width (layout, width);
	pango_layout_get_size (layout, NULL, &height);

	if (ctxt->page_nr == -1 || ctxt->pages != ctxt->page_nr) {
		/* only calculating number of pages
		   or on page we do not want to print */
		ctxt->y += pango_units_to_double (height);

		return;
	}

	/* Draw white text centered in a black rectangle. */
	cr = gtk_print_context_get_cairo_context (ctxt->context);

	cairo_save (cr);
	cairo_set_source_rgb (cr, .0, .0, .0);
	cairo_rectangle (
		cr, ctxt->x, ctxt->y,
		pango_units_to_double (width),
		pango_units_to_double (height));
	cairo_fill (cr);
	cairo_restore (cr);

	cairo_save (cr);
	cairo_move_to (cr, ctxt->x, ctxt->y);
	cairo_set_source_rgb (cr, 1., 1., 1.);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);

	ctxt->y += pango_units_to_double (height);
}

static void
e_contact_start_new_page (EContactPrintContext *ctxt)
{
	cairo_t *cr;

	cr = gtk_print_context_get_cairo_context (ctxt->context);

	/*cairo_show_page (cr);*/

	ctxt->x = ctxt->y = .0;
	ctxt->column = 0;
	ctxt->pages++;
}

static void
e_contact_print_contact (EContact *contact, EContactPrintContext *ctxt)
{
	gchar *file_as;
	cairo_t *cr;
	gint field;

	cr = gtk_print_context_get_cairo_context (ctxt->context);
	cairo_save(cr);
	ctxt->y += get_font_height (ctxt->style->headings_font) * .2;

	file_as = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (ctxt->style->print_using_grey && ctxt->pages == ctxt->page_nr) {
		cairo_save (cr);
		cairo_set_source_rgb (cr, .85, .85, .85);
		cairo_rectangle (cr, ctxt->x, ctxt->y, ctxt->column_width,
			e_contact_text_height (ctxt->context,
				ctxt->style->headings_font, file_as));
		cairo_fill (cr);
		cairo_restore (cr);
	}

	if (ctxt->pages == ctxt->page_nr)
		e_contact_output (
			ctxt->context, ctxt->style->headings_font,
			ctxt->x, ctxt->y, ctxt->column_width + 4, file_as);
	ctxt->y += e_contact_text_height (
		ctxt->context, ctxt->style->headings_font, file_as);

	g_free (file_as);

	ctxt->y += get_font_height (ctxt->style->headings_font) * .2;

	for (field = E_CONTACT_FILE_AS; field != E_CONTACT_LAST_SIMPLE_STRING; field++)
	{
		const gchar *value;
		gchar *text;

		value = e_contact_get_const (contact, field);
		if (value == NULL || *value == '\0')
			continue;

		text = g_strdup_printf ("%s:  %s",
			e_contact_pretty_name (field), value);

		if (ctxt->pages == ctxt->page_nr)
			e_contact_output (
				ctxt->context, ctxt->style->body_font,
				ctxt->x, ctxt->y, -1, text);

		ctxt->y += e_contact_text_height (
			ctxt->context, ctxt->style->body_font, text);

		ctxt->y += .2 * get_font_height (ctxt->style->body_font);

		g_free (text);
	}

	ctxt->y += get_font_height (ctxt->style->headings_font) * .4 + 8;

	cairo_restore (cr);
}

static void
e_contact_start_new_column (EContactPrintContext *ctxt)
{
	if (++ctxt->column >= ctxt->style->num_columns)
		e_contact_start_new_page (ctxt);
	else {
		ctxt->x = ctxt->column *
			(ctxt->column_width + ctxt->column_spacing);
		ctxt->y = .0;
	}
}

static gint
contact_compare (EContact *contact1, EContact *contact2)
{
	const gchar *field1, *field2;

	if (contact1 == NULL || contact2 == NULL)
		return 0;

	field1 = e_contact_get_const (contact1, E_CONTACT_FILE_AS);
	field2 = e_contact_get_const (contact2, E_CONTACT_FILE_AS);

	if (field1 != NULL && field2 != NULL)
		return g_utf8_collate (field1, field2);

	if (field1 != NULL || field2 != NULL)
		return (field1 != NULL) ? -1 : 1;

	field1 = e_contact_get_const (contact1, E_CONTACT_UID);
	field2 = e_contact_get_const (contact2, E_CONTACT_UID);

	g_return_val_if_fail (field1 != NULL && field2 != NULL, (field1 != NULL) ? -1 : 1);

	return strcmp (field1, field2);
}

static void
contacts_added (EBookView *book_view, const GList *contact_list,
                EContactPrintContext *ctxt)
{
	while (contact_list != NULL) {
		ctxt->contact_list = g_list_insert_sorted(
			ctxt->contact_list,
			g_object_ref (contact_list->data),
			(GCompareFunc) contact_compare);
		contact_list = contact_list->next;
	}
}

static void
sequence_complete (EBookView *book_view, const GList *contact_list,
                   EFlag *book_view_started)
{
	e_flag_set (book_view_started);
}

static gboolean
get_bool (gchar *data)
{
	if (data)
		return (g_ascii_strcasecmp (data, "true") == 0);
	else
		return FALSE;
}

static void
get_string (gchar *data, gchar **variable)
{
	g_free (*variable);
	*variable = g_strdup ((data != NULL) ? data : "");
}

static gint
get_integer (gchar *data)
{
	return (data != NULL) ? atoi (data) : 0;
}

static gdouble
get_float (gchar *data)
{
	return (data != NULL) ? atof (data) : .0;
}

static void
get_font (gchar *data, PangoFontDescription **variable)
{
	PangoFontDescription *desc = NULL;

	if (data != NULL)
		desc = pango_font_description_from_string (data);

	if (desc != NULL) {
		pango_font_description_free (*variable);
		*variable = desc;
	}
}

static void
e_contact_build_style (EContactPrintStyle *style)
{
	xmlDocPtr styledoc;
	gchar *filename;

	style->title = g_strdup("");
	style->type = E_CONTACT_PRINT_TYPE_CARDS;
	style->sections_start_new_page = TRUE;
	style->num_columns = 2;
	style->blank_forms = 2;
	style->letter_headings = FALSE;

	style->headings_font = pango_font_description_from_string ("Sans Bold 8");
	style->body_font = pango_font_description_from_string ("Sans 6");

	style->print_using_grey = TRUE;
	style->paper_type = 0;
	style->paper_width = 8.5;
	style->paper_height = 11;
	style->paper_source = 0;
	style->top_margin = .5;
	style->left_margin = .5;
	style->bottom_margin = .5;
	style->right_margin = .5;
	style->page_size = 0;
	style->page_width = 2.75;
	style->page_height = 4.25;
#if 0
	style->page_width = 4.25;
	style->page_height = 5.5;
#endif
#if 0
	style->page_width = 5.5;
	style->page_height = 8.5;
#endif
	style->orientation_portrait = FALSE;

	style->header_font = pango_font_description_copy (style->body_font);

	style->left_header = g_strdup("");
	style->center_header = g_strdup("");
	style->right_header = g_strdup("");

	style->footer_font = pango_font_description_copy (style->body_font);

	style->left_footer = g_strdup ("");
	style->center_footer = g_strdup ("");
	style->right_footer = g_strdup ("");
	style->reverse_on_even_pages = FALSE;

	filename = g_build_filename (EVOLUTION_ECPSDIR, "medbook.ecps", NULL);
	styledoc = e_xml_parse_file (filename);
	g_free (filename);

	if (styledoc) {
		xmlNodePtr stylenode = xmlDocGetRootElement(styledoc);
		xmlNodePtr node;
		for (node = stylenode->children; node; node = node->next) {
			gchar *data = (gchar *)xmlNodeGetContent ( node );
			if ( !strcmp( (gchar *)node->name, "title" ) ) {
				get_string(data, &(style->title));
			} else if ( !strcmp( (gchar *)node->name, "type" ) ) {
				if (g_ascii_strcasecmp (data, "cards") == 0)
					style->type = E_CONTACT_PRINT_TYPE_CARDS;
				else if (g_ascii_strcasecmp (data, "memo_style") == 0)
					style->type = E_CONTACT_PRINT_TYPE_MEMO_STYLE;
				else if (g_ascii_strcasecmp (data, "phone_list") == 0)
					style->type = E_CONTACT_PRINT_TYPE_PHONE_LIST;
			} else if ( !strcmp( (gchar *)node->name, "sections_start_new_page" ) ) {
				style->sections_start_new_page = get_bool(data);
			} else if ( !strcmp( (gchar *)node->name, "num_columns" ) ) {
				style->num_columns = get_integer(data);
			} else if ( !strcmp( (gchar *)node->name, "blank_forms" ) ) {
				style->blank_forms = get_integer(data);
			} else if ( !strcmp( (gchar *)node->name, "letter_headings" ) ) {
				style->letter_headings = get_bool(data);
			} else if ( !strcmp( (gchar *)node->name, "headings_font" ) ) {
				get_font( data, &(style->headings_font) );
			} else if ( !strcmp( (gchar *)node->name, "body_font" ) ) {
				get_font( data, &(style->body_font) );
			} else if ( !strcmp( (gchar *)node->name, "print_using_grey" ) ) {
				style->print_using_grey = get_bool(data);
			} else if ( !strcmp( (gchar *)node->name, "paper_width" ) ) {
				style->paper_width = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "paper_height" ) ) {
				style->paper_height = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "top_margin" ) ) {
				style->top_margin = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "left_margin" ) ) {
				style->left_margin = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "bottom_margin" ) ) {
				style->bottom_margin = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "right_margin" ) ) {
				style->right_margin = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "page_width" ) ) {
				style->page_width = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "page_height" ) ) {
				style->page_height = get_float(data);
			} else if ( !strcmp( (gchar *)node->name, "orientation" ) ) {
				if ( data ) {
					style->orientation_portrait =
						(g_ascii_strcasecmp (data, "landscape") != 0);
				} else {
					style->orientation_portrait = TRUE;
				}
			} else if ( !strcmp( (gchar *)node->name, "header_font" ) ) {
				get_font( data, &(style->header_font) );
			} else if ( !strcmp( (gchar *)node->name, "left_header" ) ) {
				get_string(data, &(style->left_header));
			} else if ( !strcmp( (gchar *)node->name, "center_header" ) ) {
				get_string(data, &(style->center_header));
			} else if ( !strcmp( (gchar *)node->name, "right_header" ) ) {
				get_string(data, &(style->right_header));
			} else if ( !strcmp( (gchar *)node->name, "footer_font" ) ) {
				get_font( data, &(style->footer_font) );
			} else if ( !strcmp( (gchar *)node->name, "left_footer" ) ) {
				get_string(data, &(style->left_footer));
			} else if ( !strcmp( (gchar *)node->name, "center_footer" ) ) {
				get_string(data, &(style->center_footer));
			} else if ( !strcmp( (gchar *)node->name, "right_footer" ) ) {
				get_string(data, &(style->right_footer));
			} else if ( !strcmp( (gchar *)node->name, "reverse_on_even_pages" ) ) {
				style->reverse_on_even_pages = get_bool(data);
			}
			if ( data )
				xmlFree (data);
		}
		xmlFreeDoc(styledoc);
	}

}

static void
load_contacts (EContactPrintContext *ctxt)
{
	/* Load contacts from the EBook.  This is an asynchronous operation
	 * but we force it to be synchronous here. */

	EBookView *book_view;
	EFlag *book_view_started;

	book_view_started = e_flag_new ();

	e_book_get_book_view (
		ctxt->book, ctxt->query, NULL, -1, &book_view, NULL);

	g_signal_connect (
		book_view, "contacts_added",
		G_CALLBACK (contacts_added), ctxt);
	g_signal_connect (
		book_view, "sequence_complete",
		G_CALLBACK (sequence_complete), book_view_started);

	e_book_view_start (book_view);

	while (!e_flag_is_set (book_view_started))
		g_main_context_iteration (NULL, TRUE);

	e_flag_free (book_view_started);

	g_signal_handlers_disconnect_by_func (book_view, G_CALLBACK (contacts_added), ctxt);
	g_signal_handlers_disconnect_by_func (book_view, G_CALLBACK (sequence_complete), book_view_started);
}

static void
contact_draw (EContact *contact, EContactPrintContext *ctxt)
{
	GtkPageSetup *setup;
	gdouble page_height;
	gchar *file_as;
	gboolean new_section = FALSE;

	setup = gtk_print_context_get_page_setup (ctxt->context);
	page_height = gtk_page_setup_get_page_height (setup, GTK_UNIT_POINTS);

	file_as = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (file_as != NULL) {
		gchar *section;
		gsize width;

		width = g_utf8_next_char (file_as) - file_as;
		section = g_utf8_strup (file_as, width);

		new_section = (ctxt->section == NULL ||
			g_utf8_collate (ctxt->section, section) != 0);

		if (new_section) {
			g_free (ctxt->section);
			ctxt->section = section;
		} else
			g_free (section);
	}

	if (new_section) {
		if (!ctxt->first_contact) {
			if (ctxt->style->sections_start_new_page)
				e_contact_start_new_page (ctxt);
			else if (ctxt->y > page_height)
				e_contact_start_new_column (ctxt);
		}
		if (ctxt->style->letter_headings)
			e_contact_print_letter_heading (ctxt, ctxt->section);
		ctxt->first_section = FALSE;
	}

	else if (!ctxt->first_contact && (ctxt->y > page_height)) {
		e_contact_start_new_column (ctxt);
		if (ctxt->style->letter_headings)
			e_contact_print_letter_heading (ctxt, ctxt->section);
	}

	e_contact_print_contact (contact, ctxt);

	ctxt->first_contact = FALSE;
}

static void
free_contacts (EContactPrintContext *ctxt)
{
	g_list_foreach (ctxt->contact_list, (GFunc) g_object_unref, NULL);
	g_list_free (ctxt->contact_list);
}

static void
contact_begin_print (GtkPrintOperation *operation,
                     GtkPrintContext *context,
                     EContactPrintContext *ctxt)
{
	GtkPageSetup *setup;
	gdouble page_width;

	e_contact_build_style (ctxt->style);

	setup = gtk_print_context_get_page_setup (context);
	page_width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);

	ctxt->context = context;
	ctxt->x = ctxt->y = .0;
	ctxt->column = 0;
	ctxt->first_contact = TRUE;
	ctxt->first_section = TRUE;
	ctxt->section = NULL;

	ctxt->column_spacing = gtk_print_context_get_dpi_x (context) / 4;
	ctxt->column_width = (page_width + ctxt->column_spacing) /
		ctxt->style->num_columns - ctxt->column_spacing;

	ctxt->letter_heading_font = pango_font_description_new ();
	pango_font_description_set_family (
		ctxt->letter_heading_font,
		pango_font_description_get_family (
			ctxt->style->headings_font));
	pango_font_description_set_size (
		ctxt->letter_heading_font,
		pango_font_description_get_size (
			ctxt->style->headings_font) * 1.5);

	if (ctxt->book != NULL) {
		load_contacts (ctxt);
		ctxt->page_nr = -1;
		ctxt->pages = 1;
		g_list_foreach (ctxt->contact_list, (GFunc) contact_draw, ctxt);
		gtk_print_operation_set_n_pages (operation, ctxt->pages);
	}
}

static void
contact_draw_page (GtkPrintOperation *operation,
                   GtkPrintContext *context,
                   gint page_nr,
                   EContactPrintContext *ctxt)
{
	/* only text on page_nr == pages will be drawn, the pages is recalculated */
	ctxt->page_nr = page_nr;
	ctxt->pages = 0;

	ctxt->x = ctxt->y = .0;
	ctxt->column = 0;
	ctxt->first_contact = TRUE;
	ctxt->first_section = TRUE;
	ctxt->section = NULL;

	g_list_foreach (ctxt->contact_list, (GFunc) contact_draw, ctxt);
}

static void
contact_end_print (GtkPrintOperation *operation,
                   GtkPrintContext *context,
                   EContactPrintContext *ctxt)
{
	pango_font_description_free (ctxt->style->headings_font);
	pango_font_description_free (ctxt->style->body_font);
	pango_font_description_free (ctxt->style->header_font);
	pango_font_description_free (ctxt->style->footer_font);
	pango_font_description_free (ctxt->letter_heading_font);

	g_free (ctxt->section);

	if (ctxt->book != NULL)
		free_contacts (ctxt);
}

void
e_contact_print (EBook *book, EBookQuery *query,
                 GList *contact_list, GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;
	EContactPrintContext ctxt;
	EContactPrintStyle style;

	if (book != NULL) {
		ctxt.book = book;
		ctxt.query = query;
		ctxt.contact_list = NULL;
	} else {
		ctxt.book = NULL;
		ctxt.query = NULL;
		ctxt.contact_list = contact_list;
	}
	ctxt.style = &style;
	ctxt.page_nr = 0;
	ctxt.pages = 0;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (
		operation, "begin-print",
		G_CALLBACK (contact_begin_print), &ctxt);
	g_signal_connect (
		operation, "draw_page",
		G_CALLBACK (contact_draw_page), &ctxt);
	g_signal_connect (
		operation, "end-print",
		G_CALLBACK (contact_end_print), &ctxt);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}
