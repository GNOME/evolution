/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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

#include <config.h>

#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-unit.h>
#include <libgnomeprint/gnome-print-config.h>
#include <libgnomeprint/gnome-font.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
#include <libedataserver/e-xml-utils.h>

#include "e-util/e-print.h"
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

#include "e-contact-print.h"
	
#define SCALE 5
#define HYPHEN_PIXELS 20
#define HYPHEN_PENALTY ( (SCALE) * (SCALE) * (HYPHEN_PIXELS) * (HYPHEN_PIXELS) )

typedef struct _EContactPrintContext EContactPrintContext;
typedef struct _ContactPrintItem ContactPrintItem;

struct _EContactPrintContext
{
	GtkPrintContext *pc;
	GnomePrintJob     *master;
	PangoLayout *pl;
	gdouble x;
	gdouble y;
	gint column;
	EContactPrintStyle *style;
	gboolean first_section;

	PangoFontDescription *letter_heading_font;
	char *character;
	gboolean first_contact;

	gboolean uses_book;
	int type;
	EBook *book;
	EBookQuery *query;

	GList *contacts;
};

struct _ContactPrintItem
{
	EContactPrintContext *ctxt;
	EContactPrintStyle *style;
	EContact *contact;
	GList *contact_list;
	EBook *book;
	GtkPrintSettings *settings;
	gboolean uses_book, uses_list;
};

static void 
contact_draw_page (GtkPrintOperation *print, GtkPrintContext *context, gint page_nr, ContactPrintItem *cpi);

static double
get_font_height (PangoFontDescription *font)
{
	return (double)pango_font_description_get_size (font)/(double)PANGO_SCALE;
}

static double
get_font_width (EContactPrintContext *context, PangoFontDescription *font, const char *text)
{
	int width;
	int height;

	g_return_val_if_fail (font, 0.0);
	g_return_val_if_fail (text, 0.0);

	g_assert (context->pl);
	pango_layout_set_font_description (context->pl, font);
	pango_layout_set_text (context->pl, text, -1);
	pango_layout_set_width (context->pl, -1);
	pango_layout_set_indent (context->pl, 0);

	pango_layout_get_size (context->pl,
			       &width,
			       &height);

	return (double)width/(double)PANGO_SCALE;
}

static PangoFontDescription*
find_font (const char *name, double height)
{
	PangoFontDescription *desc = pango_font_description_new ();
	pango_font_description_set_family (desc, name);	
	pango_font_description_set_size (desc, height * PANGO_SCALE);	

	return desc;
}

static PangoFontDescription*
find_closest_font_from_weight_slant (const guchar *family, GnomeFontWeight weight, gboolean italic, gdouble size)
{
	PangoFontDescription *desc = pango_font_description_new ();
	pango_font_description_set_family (desc, family);	

	/* GnomePrintWeight and PangoWeight values should be interchangeable: */
	pango_font_description_set_weight (desc, (PangoWeight)weight);

	if (italic) {
		pango_font_description_set_style (desc, PANGO_STYLE_ITALIC);
	}
	pango_font_description_set_size (desc, size * PANGO_SCALE);

	return desc;
}

static void
e_contact_output(EContactPrintContext *ctxt, PangoFontDescription *font, double x, double y, double width, const gchar *text)
{
	double indent;
	cairo_t *cr = gtk_print_context_get_cairo_context (ctxt->pc);
	cairo_save(cr);

	if ( width == -1 || get_font_width(ctxt, font, text) <= width ) {
		indent = 0.0;
	} else {
		indent = get_font_width (ctxt, font, "     ");	
	} 	

	g_assert (ctxt->pl);
	pango_layout_set_font_description (ctxt->pl, font);
	pango_layout_set_text (ctxt->pl, text, -1);
	pango_layout_set_width (ctxt->pl, width*PANGO_SCALE);
	pango_layout_set_indent (ctxt->pl, indent*PANGO_SCALE);

	cairo_move_to(cr, x, y);
	pango_cairo_show_layout (cr, ctxt->pl);
	cairo_restore (cr);

}

static gdouble
e_contact_text_height(EContactPrintContext *ctxt, PangoFontDescription *font, double width, const gchar *text)
{
	gint w, h;

	g_assert (ctxt->pl);
	pango_layout_set_font_description (ctxt->pl, font);
	pango_layout_set_text (ctxt->pl, text, -1);
	pango_layout_set_width (ctxt->pl, 1); /* fix me width hard coded */
	pango_layout_set_indent (ctxt->pl, 0);
	pango_layout_get_size (ctxt->pl, &w, &h);

	return (double)h/(double)PANGO_SCALE;
}

#if 0
static void
e_contact_output_and_advance(EContactPrintContext *ctxt, PangoFontDescription *font, double x, double width, gchar *text)
{
	ctxt->y -= .1 * get_font_height (font);
	e_contact_output(ctxt->pc, font, x, ctxt->y, width, text);
	ctxt->y -= e_contact_text_height(ctxt->pc, font, width, text);
	ctxt->y -= .1 * get_font_height (font);
}
#endif

static void
e_contact_rectangle(GtkPrintContext *pc, 
		   gdouble x0,
		   gdouble y0,
		   gdouble x1,
		   gdouble y1,
		   gdouble r,
		   gdouble g,
		   gdouble b)
{
	cairo_t *cr;
	cr = gtk_print_context_get_cairo_context (pc);
	cairo_save(cr);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_rectangle (cr,x0, y0, x1, y1);
	cairo_fill (cr);
	cairo_restore (cr);
}

static double
e_contact_get_letter_heading_height (EContactPrintContext *ctxt)
{
	return get_font_height (ctxt->letter_heading_font);
}

static void
e_contact_print_letter_heading (EContactPrintContext *ctxt, gchar *character)
{
	gdouble height;
	gdouble width;
	cairo_t *cr;

	width = get_font_width(ctxt, ctxt->letter_heading_font, "m") * 1.7;
	height = get_font_height (ctxt->letter_heading_font);
	
	cr = gtk_print_context_get_cairo_context (ctxt->pc);
	cairo_save(cr);

	e_contact_rectangle( ctxt->pc, ctxt->x, ctxt->y, width, height + 6, 0, 0, 0); 
	cairo_set_source_rgb(cr, 1, 1, 1);
	ctxt->y += 4;
	e_contact_output(ctxt, ctxt->letter_heading_font, 
			ctxt->x + (width - get_font_width(ctxt, ctxt->letter_heading_font, character))/ 2 - 5,
			ctxt->y - 5, 
			-1,
			character);
	ctxt->y += height;
	ctxt->y += 2;
	ctxt->y += 3;

	cairo_restore(cr);
}

static void
e_contact_start_new_page(EContactPrintContext *ctxt)
{
	cairo_t *cr;
	cr = gtk_print_context_get_cairo_context (ctxt->pc);
       	ctxt->x = ctxt->style->left_margin;
	ctxt->y = ctxt->style->top_margin;
	cairo_show_page (cr);
	ctxt->column = 0;
}

static double
e_contact_get_contact_size(EContact *contact, EContactPrintContext *ctxt)
{
	const char *file_as;
	gdouble height = 0;
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	gint field;
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	height += get_font_height (ctxt->style->headings_font) * .2;

	height += get_font_height (ctxt->style->headings_font) * .2;

	file_as = e_contact_get_const (contact, E_CONTACT_FILE_AS);

	height += e_contact_text_height(ctxt, ctxt->style->headings_font, column_width - 4, file_as);

	height += get_font_height (ctxt->style->headings_font) * .2;

	height += get_font_height (ctxt->style->headings_font) * .2;
	
	for(field = E_CONTACT_FILE_AS; field != E_CONTACT_LAST_SIMPLE_STRING; field++) {
		char *string;
		string = e_contact_get(contact, field);
		if (string && *string) {
			double xoff = 0;
			xoff += get_font_width(ctxt, ctxt->style->body_font, e_contact_pretty_name (field));
			xoff += get_font_width(ctxt, ctxt->style->body_font, ":  ");
			height += e_contact_text_height(ctxt, ctxt->style->body_font, column_width - xoff, string);
			height += .2 * get_font_height (ctxt->style->body_font);
		}
		g_free(string);
	}
	height += get_font_height (ctxt->style->headings_font) * .4;

	/* g_message ("%s %g", e_card_simple_get (simple, E_CARD_SIMPLE_FIELD_FILE_AS), height); */
	return height;
}


static void
e_contact_print_contact (EContact *contact, EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	char *file_as;
	cairo_t *cr;
	int field;

	column_width = (page_width + 18) / ctxt->style->num_columns - 18;
	cr = gtk_print_context_get_cairo_context (ctxt->pc);
	cairo_save(cr);
	ctxt->y += get_font_height (ctxt->style->headings_font) * .2; 
	ctxt->y += get_font_height (ctxt->style->headings_font) * .2;

	file_as = e_contact_get (contact, E_CONTACT_FILE_AS);
                                                             
	if (ctxt->style->print_using_grey)
			e_contact_rectangle(ctxt->pc, 
			    ctxt->x,ctxt->y + get_font_height (ctxt->style->headings_font) * .3, 
			    column_width, 
			    e_contact_text_height(ctxt, 
						    ctxt->style->headings_font, 
					            column_width - 4, file_as) 
						    + get_font_height (ctxt->style->headings_font) * .3,
					            .85, .85, .85); 
	  
		
	e_contact_output(ctxt, ctxt->style->headings_font, ctxt->x + 2, ctxt->y + 5, column_width + 4, file_as);
	ctxt->y += e_contact_text_height(ctxt, ctxt->style->headings_font, column_width + 4, file_as);
	g_free (file_as);

	ctxt->y += get_font_height (ctxt->style->headings_font) * .2;
	ctxt->y += get_font_height (ctxt->style->headings_font) * .2;
	
	for(field = E_CONTACT_FILE_AS; field != E_CONTACT_LAST_SIMPLE_STRING;field++) 
	{
		char *string;
		string = e_contact_get(contact, field);
		
		if (string && *string) {
			double xoff = 0;

		e_contact_output(ctxt, 
				 ctxt->style->body_font, 
				 ctxt->x + xoff, 
				 ctxt->y + 5, 
				 -1, 
				 e_contact_pretty_name (field));

			xoff += get_font_width(ctxt, ctxt->style->body_font, e_contact_pretty_name (field));
			e_contact_output(ctxt, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, ":  ");
			xoff += get_font_width(ctxt, ctxt->style->body_font, ":  ");

		e_contact_output(ctxt, 
				 ctxt->style->body_font, 
				 ctxt->x + xoff, 
				 ctxt->y + 5, 
				 column_width - xoff, 
				 string);

			ctxt->y += e_contact_text_height(ctxt, ctxt->style->body_font, column_width - xoff, string);
			ctxt->y += .2 * get_font_height (ctxt->style->body_font);

		}
		g_free(string);
	} 
	ctxt->y += get_font_height (ctxt->style->headings_font) * .4 + 8;
	cairo_restore(cr);
}

static void
e_contact_start_new_column (EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin); 
	gdouble column_offset;

	column_offset = (page_width + 18) / ctxt->style->num_columns;
	ctxt->column ++;

	if (ctxt->column >= ctxt->style->num_columns) {
		e_contact_start_new_page(ctxt);
		ctxt->column = 0;
	}
	ctxt->x = ctxt->style->left_margin + column_offset * ctxt->column;
    	ctxt->y = ctxt->style->top_margin + 12;
}

static void
complete_sequence (EBookView *book_view, EBookViewStatus status, EContactPrintContext *ctxt)
{
	GList *contacts = ctxt->contacts;
	cairo_t *cr;
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);

	ctxt->first_contact = TRUE;
	ctxt->character = NULL;
	ctxt->y = ctxt->style->page_height + ctxt->style->top_margin;
	ctxt->x = (ctxt->style->left_margin);

	for(; contacts; contacts = contacts->next) {
		EContact *contact = contacts->data;
		guchar *file_as;
		gchar *letter_str = NULL;

		file_as = e_contact_get (contact, E_CONTACT_FILE_AS);
		
	        if (file_as != NULL) {
		        letter_str = g_strndup (file_as, g_utf8_next_char (file_as) - (gchar *) file_as);
	        }
	        if ( file_as && (!ctxt->character || g_utf8_collate (ctxt->character, letter_str) != 0) ) {
			g_free (ctxt->character);
			ctxt->character = g_strdup (letter_str);
			if (ctxt->style->sections_start_new_page && ! ctxt->first_contact) {
				e_contact_start_new_page(ctxt);
		        }
			else if ((!ctxt->first_contact) && (ctxt->y  > ctxt->style->page_height * 60 )) 
		        	e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
			ctxt->first_section = FALSE;
		} 
			
	        else if ( (!ctxt->first_contact) && (ctxt->y  > ctxt->style->page_height * 60)) { 
			e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
		}
	
		g_free (letter_str);
		e_contact_print_contact(contact, ctxt);
		ctxt->first_contact = FALSE;
	}

	if (book_view)
		g_object_unref(book_view);

        g_object_unref(ctxt->pc);
	g_object_unref(ctxt->pl);
	if (ctxt->book)
		g_object_unref(ctxt->book);

	g_free(ctxt->character);
	if (ctxt->query)
		e_book_query_unref (ctxt->query);
	g_list_foreach(ctxt->contacts, (GFunc) g_object_unref, NULL);
	g_list_free(ctxt->contacts);
	pango_font_description_free(ctxt->style->headings_font);
	pango_font_description_free(ctxt->style->body_font);
	pango_font_description_free(ctxt->style->header_font);
	pango_font_description_free(ctxt->style->footer_font);
	pango_font_description_free(ctxt->letter_heading_font);
	g_free(ctxt->style);
	g_free(ctxt);
}

static int
contact_compare (EContact *contact1, EContact *contact2)
{
	if (contact1 && contact2) {
		const char *file_as1, *file_as2;
		file_as1 = e_contact_get_const (contact1, E_CONTACT_FILE_AS);
		file_as2 = e_contact_get_const (contact2, E_CONTACT_FILE_AS);

		if (file_as1 && file_as2)
			return g_utf8_collate(file_as1, file_as2);
		if (file_as1)
			return -1;
		if (file_as2)
			return 1;
		return strcmp(e_contact_get_const(contact1, E_CONTACT_UID), e_contact_get_const(contact2, E_CONTACT_UID));
	} else {
		return 0;
	}
}

static void
create_contact(EBookView *book_view, const GList *contacts, EContactPrintContext *ctxt)
{
	for(; contacts; contacts = contacts->next) {
		EContact *contact = contacts->data;
		g_object_ref(contact);
		ctxt->contacts = g_list_insert_sorted(ctxt->contacts, contact, (GCompareFunc) contact_compare);
	}
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, EContactPrintContext *ctxt)
{
	g_object_ref(book_view);

	g_signal_connect(book_view,
			 "contacts_added",
			 G_CALLBACK(create_contact),
			 ctxt);

	e_book_view_start (book_view);
}

static void
e_contact_do_print_contacts (EBook *book, EBookQuery *query, EContactPrintContext *ctxt)
{
	EBookView *bookview;
	gboolean status;

	status = e_book_get_book_view (book, query, NULL, -1, &bookview, NULL);
	book_view_loaded (book, 1, bookview, ctxt);
}
	
static  void
e_contact_do_print (EBook *book, EBookQuery *query, EContactPrintContext *ctxt)
{
	switch ( ctxt->style->type ) {
	case E_CONTACT_PRINT_TYPE_CARDS:
		e_contact_do_print_contacts( book, query, ctxt);
		break;
	default:
		break;
	}
}

static void lowify( char *data )
{
	for ( ; *data; data++ )
		*data = tolower((unsigned char) *data);
}

static gboolean get_bool( char *data )
{
	if ( data ) {
		lowify ( data );
		return ! strcmp(data, "true");
	} else
		return FALSE;
}

static void get_string( char *data, char **variable )
{
	g_free ( *variable );
	if ( data )
		*variable = g_strdup( data );
	else
		*variable = g_strdup( "" );
}

static int get_integer( char *data )
{
	if ( data )
		return atoi(data);
	else 
		return 0;
}

static double get_float( char *data )
{
	if ( data )
		return atof(data);
	else 
		return 0;
}

static void get_font( char *data, PangoFontDescription **variable )
{
	if ( data ) {
		PangoFontDescription *font = pango_font_description_from_string ( data );
		if ( font ) {
			pango_font_description_free(*variable);
			*variable = font;
		}
	}
}


static void
e_contact_build_style(EContactPrintStyle *style, GtkPrintSettings *config)
{
	xmlDocPtr styledoc;
	gchar *filename;
	gdouble page_height, page_width;

	style->title = g_strdup("");
	style->type = E_CONTACT_PRINT_TYPE_CARDS;
	style->sections_start_new_page = TRUE;
	style->num_columns = 2;
	style->blank_forms = 2;
	style->letter_headings = FALSE;

	style->headings_font = find_closest_font_from_weight_slant ("Sans", GNOME_FONT_BOLD, FALSE, 8);
	style->body_font = find_closest_font_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

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

	style->header_font = find_closest_font_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

	style->left_header = g_strdup("");
	style->center_header = g_strdup("");
	style->right_header = g_strdup("");

	style->footer_font = find_closest_font_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

	style->left_footer = g_strdup("");
	style->center_footer = g_strdup("");
	style->right_footer = g_strdup("");
	style->reverse_on_even_pages = FALSE;
	filename = g_concat_dir_and_file(EVOLUTION_ECPSDIR, "medbook.ecps");
	styledoc = e_xml_parse_file (filename);
	g_free(filename);
	if (styledoc) {
		xmlNodePtr stylenode = xmlDocGetRootElement(styledoc);
		xmlNodePtr node;
		for (node = stylenode->children; node; node = node->next) {
			char *data = xmlNodeGetContent ( node );
			if ( !strcmp( node->name, "title" ) ) {
				get_string(data, &(style->title));
			} else if ( !strcmp( node->name, "type" ) ) {
				lowify( data );
				if ( !strcmp( data, "cards" ) )
					style->type = E_CONTACT_PRINT_TYPE_CARDS;
				else if ( !strcmp( data, "memo_style" ) )
					style->type = E_CONTACT_PRINT_TYPE_MEMO_STYLE;
				else if ( !strcmp( data, "phone_list" ) )
					style->type = E_CONTACT_PRINT_TYPE_PHONE_LIST;
			} else if ( !strcmp( node->name, "sections_start_new_page" ) ) {
				style->sections_start_new_page = get_bool(data);
			} else if ( !strcmp( node->name, "num_columns" ) ) {
				style->num_columns = get_integer(data);
			} else if ( !strcmp( node->name, "blank_forms" ) ) {
				style->blank_forms = get_integer(data);
			} else if ( !strcmp( node->name, "letter_headings" ) ) {
				style->letter_headings = get_bool(data);
			} else if ( !strcmp( node->name, "headings_font" ) ) {
				get_font( data, &(style->headings_font) );
			} else if ( !strcmp( node->name, "body_font" ) ) {
				get_font( data, &(style->body_font) );
			} else if ( !strcmp( node->name, "print_using_grey" ) ) {
				style->print_using_grey = get_bool(data);
			} else if ( !strcmp( node->name, "paper_width" ) ) {
				style->paper_width = get_float(data);
			} else if ( !strcmp( node->name, "paper_height" ) ) {
				style->paper_height = get_float(data);
			} else if ( !strcmp( node->name, "top_margin" ) ) {
				style->top_margin = get_float(data);
			} else if ( !strcmp( node->name, "left_margin" ) ) {
				style->left_margin = get_float(data);
			} else if ( !strcmp( node->name, "bottom_margin" ) ) {
				style->bottom_margin = get_float(data);
			} else if ( !strcmp( node->name, "right_margin" ) ) {
				style->right_margin = get_float(data);
			} else if ( !strcmp( node->name, "page_width" ) ) {
				style->page_width = get_float(data);
			} else if ( !strcmp( node->name, "page_height" ) ) {
				style->page_height = get_float(data);
			} else if ( !strcmp( node->name, "orientation" ) ) {
				if ( data ) {
					lowify(data);
					style->orientation_portrait = strcmp(data, "landscape");
				} else {
					style->orientation_portrait = TRUE;
				}
			} else if ( !strcmp( node->name, "header_font" ) ) {
				get_font( data, &(style->header_font) );
			} else if ( !strcmp( node->name, "left_header" ) ) {
				get_string(data, &(style->left_header));
			} else if ( !strcmp( node->name, "center_header" ) ) {
				get_string(data, &(style->center_header));
			} else if ( !strcmp( node->name, "right_header" ) ) {
				get_string(data, &(style->right_header));
			} else if ( !strcmp( node->name, "footer_font" ) ) {
				get_font( data, &(style->footer_font) );
			} else if ( !strcmp( node->name, "left_footer" ) ) {
				get_string(data, &(style->left_footer));
			} else if ( !strcmp( node->name, "center_footer" ) ) {
				get_string(data, &(style->center_footer));
			} else if ( !strcmp( node->name, "right_footer" ) ) {
				get_string(data, &(style->right_footer));
			} else if ( !strcmp( node->name, "reverse_on_even_pages" ) ) {
				style->reverse_on_even_pages = get_bool(data);
			}
			if ( data )
				xmlFree (data);
		}
		xmlFreeDoc(styledoc);
	}

}

static gint
e_contact_print_close(GnomeDialog *dialog, gpointer data)
{
	return FALSE;
}

void
e_contact_print_response(GtkWidget *dialog, gint response_id, gpointer data)
{
	GtkPrintSettings *settings;
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GtkPaperSize *paper_size;
	GtkPageSetup *page_setup;
	GtkPrintPages print_pages;
	GList *contact_list = NULL;
	EBook *book = NULL;
	EBookQuery *query = NULL;
	EContact *contact = NULL;
	gdouble font_size;
	gboolean uses_book = FALSE, uses_list = FALSE, uses_range = FALSE;

	EContactPrintContext *ctxt = g_new0 (EContactPrintContext, 1);
	EContactPrintStyle *style = g_new0 (EContactPrintStyle, 1);
	ContactPrintItem *cpi = g_new0 (ContactPrintItem, 1);

	settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (dialog));
	uses_range = GPOINTER_TO_INT (g_object_get_data(G_OBJECT (dialog),"uses_range"));

	if (uses_range) {
		if (gtk_print_settings_get_print_pages (settings) == GTK_PRINT_PAGES_ALL) {
			uses_book = TRUE;
		 }
		if (gtk_print_settings_get_print_pages (settings) == GTK_PRINT_PAGES_CURRENT)
			uses_list = TRUE;
	}
	else {
		uses_book = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "uses_book"));
		uses_list = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "uses_list"));
	} 
	if (uses_book) {
		book = g_object_get_data(G_OBJECT(dialog), "book");
		query = g_object_get_data(G_OBJECT(dialog), "query");
		e_book_query_ref (query);
	}
	else if (uses_list) {
		contact_list = g_object_get_data(G_OBJECT(dialog), "contact_list");
	}
	else {
		contact = g_object_get_data(G_OBJECT(dialog), "contact");
	}   

	page_setup = gtk_page_setup_new ();
	paper_size = gtk_paper_size_new ("iso_a4"); /* FIXME paper size hardcoded */
	gtk_page_setup_set_paper_size (page_setup, paper_size);	
	print = gtk_print_operation_new ();
	gtk_print_operation_set_default_page_setup (print, page_setup);
	gtk_print_operation_set_n_pages (print, 1);
	gtk_print_settings_set_print_pages (settings, GTK_PRINT_PAGES_ALL);

	/* style information */
	e_contact_build_style(style, settings);
	style->page_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_INCH);
	style->page_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_INCH);
	ctxt->x = 0;
	ctxt->y = 0;
	ctxt->column = 0;
	ctxt->style = style;
	ctxt->first_section = TRUE;
	ctxt->type = GTK_RESPONSE_OK;
	font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
	ctxt->letter_heading_font = find_font (pango_font_description_get_family (ctxt->style->headings_font), 
					       get_font_height (ctxt->style->headings_font)*1.5);
	ctxt->book = book;
	ctxt->query = query;
	cpi->uses_book = uses_book;
	cpi->uses_list = uses_list;
	cpi->settings = settings;
	cpi->ctxt = ctxt;
	cpi->contact= contact;
	cpi->ctxt->contacts = NULL;
	cpi->contact_list= contact_list;
	cpi->book = book; 
	e_contact_do_print_contacts (book, query, ctxt);

        /* runs the print dialog , emitting signals */
	g_signal_connect (print, "draw_page",G_CALLBACK (contact_draw_page), cpi);
	if (response_id == GTK_RESPONSE_APPLY) { 
		res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PREVIEW, NULL, NULL);
	}
	else
		res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, NULL, NULL); 
	g_object_unref (settings);
	g_object_unref (print);
	g_object_unref (paper_size);
	g_object_unref (page_setup); 
	g_object_unref (cpi);
	gtk_widget_destroy (dialog); 
}


GtkWidget *
e_contact_print_dialog_new(EBook *book, char *query, GList *list)
{
	GtkWidget *dialog;
	GList *copied_list = NULL;
	GList *l;
	
	dialog = e_print_get_dialog(_("Print contacts"), GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES); 

	if (list != NULL) {
		copied_list = g_list_copy (list);
		for (l = copied_list; l; l = l->next)
			l->data = e_contact_duplicate (E_CONTACT (l->data));
	}
	g_object_ref(book);
	g_object_set_data(G_OBJECT(dialog), "contact_list", copied_list);
	g_object_set_data(G_OBJECT(dialog), "book", book);
	g_object_set_data(G_OBJECT(dialog), "query", e_book_query_from_string  (query));
	g_object_set_data(G_OBJECT(dialog), "uses_range", GINT_TO_POINTER (TRUE)); 

	g_signal_connect(dialog,
			 "response", G_CALLBACK(e_contact_print_response), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_close), NULL);
	return dialog;

}

GtkWidget *
e_contact_print_contact_dialog_new(EContact *contact)
{
	GtkWidget *dialog;

	dialog = e_print_get_dialog(_("Print contact"), GNOME_PRINT_DIALOG_COPIES);
        contact = e_contact_duplicate(contact);
	g_object_set_data(G_OBJECT(dialog), "contact", contact);
	g_object_set_data(G_OBJECT(dialog), "uses_list", GINT_TO_POINTER (FALSE));
	g_object_set_data(G_OBJECT(dialog), "uses_book", GINT_TO_POINTER (FALSE));
	g_object_set_data(G_OBJECT(dialog), "uses_range", GINT_TO_POINTER (FALSE));
	g_signal_connect(dialog,
			 "response", G_CALLBACK(e_contact_print_response), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_close), NULL);
	return dialog;
}

GtkWidget *
e_contact_print_contact_list_dialog_new(GList *list)
{
	GtkWidget *dialog;
	GList *copied_list;
	GList *l;

	if (list == NULL)
		return NULL;

	copied_list = g_list_copy (list);
	for (l = copied_list; l; l = l->next)
		l->data = e_contact_duplicate (E_CONTACT (l->data));

	dialog = e_print_get_dialog(_("Print contact"), GNOME_PRINT_DIALOG_COPIES);

	g_object_set_data(G_OBJECT(dialog), "contact_list", copied_list);
	g_object_set_data(G_OBJECT(dialog), "uses_list", GINT_TO_POINTER (TRUE));
	g_object_set_data(G_OBJECT(dialog), "uses_book", GINT_TO_POINTER (FALSE));
	g_object_set_data(G_OBJECT(dialog), "uses_range", GINT_TO_POINTER (FALSE));
	g_signal_connect(dialog,
			 "response", G_CALLBACK(e_contact_print_response), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_close), NULL);
	return dialog;
}

static void
contact_draw_page (GtkPrintOperation *print, GtkPrintContext *context, gint page_nr, ContactPrintItem *cpi)
{
 cairo_t *cr;    
 EBookView *view;
 EBookViewStatus status;
 
		cpi->ctxt->pc = context;
		g_object_ref (cpi->ctxt->pc);
		cpi->ctxt->pl =gtk_print_context_create_pango_layout (context);
		
			if (cpi->uses_book) {
				complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, cpi->ctxt);
			} 
			
			else if (cpi->uses_list) {
				complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, cpi->ctxt);
			}
			else {
				cpi->ctxt->contacts = g_list_append(NULL,cpi->contact);
				complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, cpi->ctxt);
			} 
}
