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
#include "e-contact-print.h"

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
#include <libgnomeprint/gnome-font.h>
#include <libgnomeprint/gnome-print-job.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libebook/e-book-async.h>
#include <libebook/e-contact.h>
#include <gal/util/e-util.h>

#define SCALE 5
#define HYPHEN_PIXELS 20
#define HYPHEN_PENALTY ( (SCALE) * (SCALE) * (HYPHEN_PIXELS) * (HYPHEN_PIXELS) )

typedef struct _EContactPrintContext EContactPrintContext;

struct _EContactPrintContext
{
	GnomePrintContext *pc;
	GnomePrintJob     *master;
	gdouble x;
	gdouble y;
	gint column;
	EContactPrintStyle *style;
	gboolean first_section;
	gchar first_char_on_page;
	gchar last_char_on_page;
	GnomeFont *letter_heading_font;
	GnomeFont *letter_tab_font;
	char *character;
	gboolean first_contact;

	gboolean uses_book;
	int type;
	EBook *book;
	EBookQuery *query;

	GList *contacts;
};

static gint
e_contact_divide_text(GnomePrintContext *pc, GnomeFont *font, double width, const gchar *text, GList **return_val /* Of type char[] */)
{
	if ( width == -1 || gnome_font_get_width_utf8(font, text) <= width ) {
		if ( return_val ) {
			*return_val = g_list_append(*return_val, g_strdup(text));
		}
		return 1;
	} else {
#if 1
		int i, l;
		double x = 0;
		int lastend = 0;
		int linestart = 0;
		int firstword = 1;
		int linecount = 0;
		l = strlen(text);
		for ( i = 0; i < l; i++ ) {
			if ( text[i] == ' ' ) {
				if ( (!firstword) && x + gnome_font_get_width_utf8_sized(font, text + lastend, i - lastend) > width ) {
					if (return_val) {
						*return_val = g_list_append(*return_val, g_strndup(text + linestart, lastend - linestart));
					}
					x = gnome_font_get_width_utf8(font, "    ");
					linestart = lastend + 1;
					x += gnome_font_get_width_utf8_sized(font, text + linestart, i - linestart);
					lastend = i;
					linecount ++;
				} else {
					x += gnome_font_get_width_utf8_sized(font, text + lastend, i - lastend);
					lastend = i;
				}
				firstword = 0;
			} else if ( text[i] == '\n' ) {
				if ( (!firstword) && x + gnome_font_get_width_utf8_sized(font, text + lastend, i - lastend) > width ) {
					if (return_val) {
						*return_val = g_list_append(*return_val, g_strndup(text + linestart, lastend - linestart));
					}
					linestart = lastend + 1;
					lastend = i;
					linecount ++;
				}
				if (return_val) {
					*return_val = g_list_append(*return_val, g_strndup(text + linestart, i - linestart));
				}
				linestart = i + 1;
				lastend = i + 1;
				linecount ++;
				x = gnome_font_get_width_utf8(font, "    ");

				firstword = 1;
			}
		}
		if ( (!firstword) && x + gnome_font_get_width_utf8_sized(font, text + lastend, i - lastend) > width ) {
			if (return_val) {
				*return_val = g_list_append(*return_val, g_strndup(text + linestart, lastend - linestart));
			}
			linestart = lastend + 1;
			lastend = i;
			linecount ++;
		}
		if (return_val) {
			*return_val = g_list_append(*return_val, g_strndup(text + linestart, i - linestart));
		}
		linecount ++;
		return(linecount);
#else
		HnjBreak *breaks;
		gint *result;
		gint *is;
		gint n_breaks = 0, n_actual_breaks = 0;
		gint i;
		gint l;
		gchar *hyphenation;
		double x = - gnome_font_get_width_utf8(font, "    ") * SCALE;
		HnjParams hnjparams;

		hnjparams.set_width = width * SCALE + x;
		hnjparams.max_neg_space = 0;
		hnjparams.tab_width = 0;

		l = strlen(text);
	
		/* find possible line breaks. */
		for (i = 0; i < l; i++) {
			if (text[i] == '-')
				n_breaks++;
			else if (text[i] == ' ')
				n_breaks++;
#if 0
 			else if (hyphenation[i] & 1)
				n_breaks++;
#endif
		}

		breaks = g_new( HnjBreak, n_breaks + 1 );
		result = g_new( gint, n_breaks + 1 );
		is = g_new( gint, n_breaks + 1 );
		n_breaks = 0;
		/* find possible line breaks. */
	
		for (i = 0; i < l; i++) {
			if ( text[i] == '-' ) {
				x += gnome_font_get_width(font, text[i]) * SCALE;
				breaks[n_breaks].x0 = x;
				breaks[n_breaks].x1 = x;
				breaks[n_breaks].penalty = HYPHEN_PENALTY;
				breaks[n_breaks].flags = HNJ_JUST_FLAG_ISHYPHEN;
				is[n_breaks] = i + 1;
				n_breaks++;
			} else if ( text[i] == ' ' ) {
				breaks[ n_breaks ].x0 = x;
				x += gnome_font_get_width(font, text[i]) * SCALE;
				breaks[ n_breaks ].x1 = x;
				breaks[ n_breaks ].penalty = 0;
				breaks[ n_breaks ].flags = HNJ_JUST_FLAG_ISSPACE;
				is[ n_breaks ] = i + 1;
				n_breaks++;
#if 0
			} else if (word->hyphenation[i] & 1) {
				breaks[n_breaks].x0 = x + gnome_font_get_width(font, '-') * SCALE;
				breaks[n_breaks].x1 = x;
				breaks[n_breaks].penalty = HYPHEN_PENALTY;
				breaks[n_breaks].flags = HNJ_JUST_FLAG_ISHYPHEN;
				is[n_breaks] = i + 1;
				n_breaks++;
#endif
			} else
				x += gnome_font_get_width(font, text[i]) * SCALE;

		}
		is[n_breaks] = i;
		breaks[n_breaks].flags = 0;
		n_breaks++;

		/* Calculate optimal line breaks. */
		n_actual_breaks = hnj_hs_just (breaks, n_breaks,
					       &hnjparams, result);

		if ( return_val ) {
			gchar *next_val;
			if ( breaks[result[0]].flags == HNJ_JUST_FLAG_ISHYPHEN && text[is[result[0]]] != '-' ) {
				next_val = g_new(gchar, is[result[0]] + 2);
				strncpy(next_val, text, is[result[0]]);
				next_val[is[result[0]]] = 0;
				strcat(next_val, "-");
			} else {
				next_val = g_new(gchar, is[result[0]] + 1);
				strncpy(next_val, text, is[result[0]]);
				next_val[is[result[0]]] = 0;
			}
			*return_val = g_list_append(*return_val, next_val);
			
			for ( i = 1; i < n_actual_breaks; i++ ) {
				if ( (breaks[result[i]].flags & HNJ_JUST_FLAG_ISHYPHEN) && (text[is[result[i]]] != '-') ) {
					next_val = g_new(gchar, is[result[i]] - is[result[i - 1]] + 2);
					strncpy(next_val, text + is[result[i - 1]], is[result[i]] - is[result[i - 1]]);
					next_val[is[result[i]] - is[result[i - 1]]] = 0;
					strcat(next_val, "-");
				} else {
					next_val = g_new(gchar, is[result[i]] - is[result[i - 1]] + 1);
					strncpy(next_val, text + is[result[i - 1]], is[result[i]] - is[result[i - 1]]);
					next_val[is[result[i]] - is[result[i - 1]]] = 0;
				}
				*return_val = g_list_append(*return_val, next_val);
			}
		}
		
		g_free (breaks);
		g_free (result);
		g_free (is);
		return n_actual_breaks;
#endif
	}
}

static void
e_contact_output(GnomePrintContext *pc, GnomeFont *font, double x, double y, double width, const gchar *text)
{
	GList *list = NULL, *list_start;
	int first_line = 1;
	gnome_print_gsave(pc);
	gnome_print_setfont(pc, font);
	e_contact_divide_text(pc, font, width, text, &list);
	for ( list_start = list; list; list = g_list_next(list)) {
		y -= gnome_font_get_ascender(font);
		gnome_print_moveto(pc, x, y);
		gnome_print_show(pc, (char *)list->data);
		y -= gnome_font_get_descender(font);
		y -= .2 * gnome_font_get_size (font);
		if ( first_line ) {
			x += gnome_font_get_width_utf8(font, "    ");
			first_line = 0;
		}
	}
	g_list_foreach( list_start, (GFunc) g_free, NULL );
	g_list_free( list_start );
	gnome_print_grestore(pc);
}

static gdouble
e_contact_text_height(GnomePrintContext *pc, GnomeFont *font, double width, const gchar *text)
{
	int line_count = e_contact_divide_text(pc, font, width, text, NULL);
	return line_count * (gnome_font_get_ascender(font) + gnome_font_get_descender(font)) +
		(line_count - 1) * .2 * gnome_font_get_size (font);
}

#if 0
static void
e_contact_output_and_advance(EContactPrintContext *ctxt, GnomeFont *font, double x, double width, gchar *text)
{
	ctxt->y -= .1 * gnome_font_get_size (font);
	e_contact_output(ctxt->pc, font, x, ctxt->y, width, text);
	ctxt->y -= e_contact_text_height(ctxt->pc, font, width, text);
	ctxt->y -= .1 * gnome_font_get_size (font);
}
#endif

static void
e_contact_rectangle(GnomePrintContext *pc, 
		   gdouble x0,
		   gdouble y0,
		   gdouble x1,
		   gdouble y1,
		   gdouble r,
		   gdouble g,
		   gdouble b)
{
	gnome_print_gsave(pc);
	gnome_print_setrgbcolor(pc, r, g, b);
	gnome_print_moveto(pc, x0, y0);
	gnome_print_lineto(pc, x1, y0);
	gnome_print_lineto(pc, x1, y1);
	gnome_print_lineto(pc, x0, y1);
	gnome_print_lineto(pc, x0, y0);
	gnome_print_fill(pc);
	gnome_print_grestore(pc);
}

static double
e_contact_get_letter_tab_width (EContactPrintContext *ctxt)
{
	return gnome_font_get_width_utf8(ctxt->letter_tab_font, "123") + 4 + 18;
}

static double
e_contact_print_letter_tab (EContactPrintContext *ctxt)
{
	unsigned char character;
	gdouble x, y;
	gdouble page_width = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble tab_height, tab_width;
	gdouble font_size;
	tab_height = 72 * (ctxt->style->page_height - ctxt->style->top_margin - ctxt->style->bottom_margin) / 27.0;
	font_size = tab_height / 2;
	tab_width = e_contact_get_letter_tab_width(ctxt) - 18;
	x = page_width + 72 * (ctxt->style->left_margin) - tab_width;
	y = 72 * (ctxt->style->page_height - ctxt->style->top_margin);


	gnome_print_gsave( ctxt->pc );
	if ( ctxt->style->print_using_grey )
		e_contact_rectangle( ctxt->pc, x, 72 * (ctxt->style->page_height - ctxt->style->top_margin), x + tab_width, ctxt->style->bottom_margin * 72, .85, .85, .85 );
	for ( character = 'A' - 1; character <= 'Z'; character ++ ) {
		char string[] = "123";
		if ( character >= 'A' ) {
			string[0] = tolower(character);
			string[1] = 0;
		}
		if ( character >= ctxt->first_char_on_page && character <= ctxt->last_char_on_page ) {
			e_contact_rectangle( ctxt->pc, x + 1, y - 1, x + tab_width - 1, y - (tab_height - 1), 0, 0, 0 );
			gnome_print_setrgbcolor( ctxt->pc, 1, 1, 1 );
			e_contact_output( ctxt->pc, ctxt->letter_tab_font, x + tab_width / 2 - gnome_font_get_width_utf8(ctxt->letter_tab_font, string) / 2, y - (tab_height - font_size) / 2, -1, string );
		} else {
			gnome_print_setrgbcolor( ctxt->pc, 0, 0, 0 );
			e_contact_output( ctxt->pc, ctxt->letter_tab_font, x + tab_width / 2 - gnome_font_get_width_utf8(ctxt->letter_tab_font, string) / 2, y - (tab_height - font_size) / 2, -1, string );
		}
		y -= tab_height;
	}
	gnome_print_grestore( ctxt->pc );
	return gnome_font_get_width_utf8(ctxt->style->body_font, "123") + gnome_font_get_size (ctxt->style->body_font) / 5;
}

static double
e_contact_get_letter_heading_height (EContactPrintContext *ctxt)
{
	gdouble ascender, descender;
	ascender = gnome_font_get_ascender(ctxt->letter_heading_font);
	descender = gnome_font_get_descender(ctxt->letter_heading_font);
	return ascender + descender + 9;
}

static void
e_contact_print_letter_heading (EContactPrintContext *ctxt, gchar *character)
{
	gdouble ascender, descender;
	gdouble width;

	width = gnome_font_get_width_utf8(ctxt->letter_heading_font, "m") * 1.7;
	ascender = gnome_font_get_ascender(ctxt->letter_heading_font);
	descender = gnome_font_get_descender(ctxt->letter_heading_font);
	gnome_print_gsave( ctxt->pc );
	e_contact_rectangle( ctxt->pc, ctxt->x, ctxt->y, ctxt->x + width, ctxt->y - (ascender + descender + 6), 0, 0, 0);
	gnome_print_setrgbcolor(ctxt->pc, 1, 1, 1);
	ctxt->y -= 4;
	e_contact_output(ctxt->pc, ctxt->letter_heading_font, ctxt->x + (width - gnome_font_get_width_utf8(ctxt->letter_heading_font, character))/ 2, ctxt->y, -1, character);
	ctxt->y -= ascender + descender;
	ctxt->y -= 2;
	ctxt->y -= 3;
	gnome_print_grestore( ctxt->pc );
}

static void
e_contact_start_new_page(EContactPrintContext *ctxt)
{
	ctxt->x = ctxt->style->left_margin * 72;
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	ctxt->column = 0;
	if ( ctxt->style->letter_tabs )
		e_contact_print_letter_tab(ctxt);
	gnome_print_showpage(ctxt->pc);

	gnome_print_beginpage (ctxt->pc, NULL);

	ctxt->first_char_on_page = ctxt->last_char_on_page + 1;
}

static double
e_contact_get_contact_size(EContact *contact, EContactPrintContext *ctxt)
{
	gdouble height = 0;
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	const char *file_as;
	gint field;
	if ( ctxt->style->letter_tabs )
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	height += gnome_font_get_size (ctxt->style->headings_font) * .2;

	height += gnome_font_get_size (ctxt->style->headings_font) * .2;

	file_as = e_contact_get_const (contact, E_CONTACT_FILE_AS);

	height += e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as);

	height += gnome_font_get_size (ctxt->style->headings_font) * .2;

	height += gnome_font_get_size (ctxt->style->headings_font) * .2;
	
	for(field = E_CONTACT_FILE_AS; field != E_CONTACT_LAST_SIMPLE_STRING; field++) {
		char *string;
		string = e_contact_get(contact, field);
		if (string && *string) {
			double xoff = 0;
			xoff += gnome_font_get_width_utf8(ctxt->style->body_font, e_contact_pretty_name (field));
			xoff += gnome_font_get_width_utf8(ctxt->style->body_font, ":  ");
			height += e_contact_text_height(ctxt->pc, ctxt->style->body_font, column_width - xoff, string);
			height += .2 * gnome_font_get_size (ctxt->style->body_font);
		}
		g_free(string);
	}
	height += gnome_font_get_size (ctxt->style->headings_font) * .4;

	/* g_message ("%s %g", e_card_simple_get (simple, E_CARD_SIMPLE_FIELD_FILE_AS), height); */
	return height;
}


static void
e_contact_print_contact (EContact *contact, EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	char *file_as;
	int field;

	if ( ctxt->style->letter_tabs )
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	gnome_print_gsave(ctxt->pc);

	ctxt->y -= gnome_font_get_size (ctxt->style->headings_font) * .2;
	ctxt->y -= gnome_font_get_size (ctxt->style->headings_font) * .2;

	file_as = e_contact_get (contact, E_CONTACT_FILE_AS);
	if (ctxt->style->print_using_grey)
		e_contact_rectangle(ctxt->pc, ctxt->x, ctxt->y + gnome_font_get_size (ctxt->style->headings_font) * .3, ctxt->x + column_width, ctxt->y - e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as) - gnome_font_get_size (ctxt->style->headings_font) * .3, .85, .85, .85);
	e_contact_output(ctxt->pc, ctxt->style->headings_font, ctxt->x + 2, ctxt->y, column_width - 4, file_as);
	ctxt->y -= e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as);
	g_free (file_as);

	ctxt->y -= gnome_font_get_size (ctxt->style->headings_font) * .2;
	ctxt->y -= gnome_font_get_size (ctxt->style->headings_font) * .2;
	
	for(field = E_CONTACT_FILE_AS; field != E_CONTACT_LAST_SIMPLE_STRING; field++) {
		char *string;
		string = e_contact_get(contact, field);

		if (string && *string) {
			double xoff = 0;
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, e_contact_pretty_name (field));
			xoff += gnome_font_get_width_utf8(ctxt->style->body_font, e_contact_pretty_name (field));
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, ":  ");
			xoff += gnome_font_get_width_utf8(ctxt->style->body_font, ":  ");
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, column_width - xoff, string);
			ctxt->y -= e_contact_text_height(ctxt->pc, ctxt->style->body_font, column_width - xoff, string);
			ctxt->y -= .2 * gnome_font_get_size (ctxt->style->body_font);
		}
		g_free(string);
	}
	
	ctxt->y -= gnome_font_get_size (ctxt->style->headings_font) * .4;
	gnome_print_grestore(ctxt->pc);
}

static void
e_contact_start_new_column (EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_offset;
	if ( ctxt->style->letter_tabs ) 
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_offset = (page_width + 18) / ctxt->style->num_columns;
	ctxt->column ++;
	if (ctxt->column >= ctxt->style->num_columns) {
		e_contact_start_new_page(ctxt);
		ctxt->column = 0;
	}
	ctxt->x = (72 * ctxt->style->left_margin) + column_offset * ctxt->column;
	ctxt->y = 72 * (ctxt->style->page_height - ctxt->style->top_margin);
}

static void
complete_sequence(EBookView *book_view, EBookViewStatus status, EContactPrintContext *ctxt)
{
	GList *contacts = ctxt->contacts;

	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);

	ctxt->first_contact = TRUE;
	ctxt->character = NULL;
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	ctxt->x = (ctxt->style->left_margin) * 72;
	if ( ctxt->style->letter_tabs ) 
		page_width -= e_contact_get_letter_tab_width(ctxt);
	
	ctxt->first_char_on_page = 'A' - 1;

	gnome_print_beginpage (ctxt->pc, NULL);

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
			else if ((!ctxt->first_contact) && (ctxt->y - e_contact_get_letter_heading_height(ctxt) - e_contact_get_contact_size(contact, ctxt) < ctxt->style->bottom_margin * 72))
				e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
			ctxt->first_section = FALSE;
		} 
		else if ( (!ctxt->first_contact) && (ctxt->y - e_contact_get_contact_size(contact, ctxt) < ctxt->style->bottom_margin * 72)) {
			e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
		}
		g_free (letter_str);
		ctxt->last_char_on_page = file_as ? toupper (*file_as) : ' ';
		if ( ctxt->last_char_on_page < ctxt->first_char_on_page )
			ctxt->first_char_on_page = ctxt->last_char_on_page;
		e_contact_print_contact(contact, ctxt);
		ctxt->first_contact = FALSE;
	}
	ctxt->last_char_on_page = 'Z';
	if ( ctxt->style->letter_tabs )
		e_contact_print_letter_tab(ctxt);
	gnome_print_showpage(ctxt->pc);
	gnome_print_context_close(ctxt->pc);
	g_free(ctxt->character);
	if (book_view)
		g_object_unref(book_view);
	if (ctxt->type == GNOME_PRINT_DIALOG_RESPONSE_PREVIEW) {
		GtkWidget *preview;
		preview = GTK_WIDGET(gnome_print_job_preview_new(ctxt->master, "Print Preview"));
		gtk_widget_show_all(preview);
	} else {
		gnome_print_job_print(ctxt->master);
	}
	g_object_unref(ctxt->pc);
	g_object_unref(ctxt->master);
	if (ctxt->book)
		g_object_unref(ctxt->book);
	if (ctxt->query)
		e_book_query_unref (ctxt->query);
	g_list_foreach(ctxt->contacts, (GFunc) g_object_unref, NULL);
	g_list_free(ctxt->contacts);
	g_object_unref(ctxt->style->headings_font);
	g_object_unref(ctxt->style->body_font);
	g_object_unref(ctxt->style->header_font);
	g_object_unref(ctxt->style->footer_font);
	g_object_unref(ctxt->letter_heading_font);
	g_object_unref(ctxt->letter_tab_font);
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

	g_signal_connect(book_view,
			 "sequence_complete",
			 G_CALLBACK(complete_sequence),
			 ctxt);

	e_book_view_start (book_view);
}

static void
e_contact_do_print_contacts (EBook *book, EBookQuery *query, EContactPrintContext *ctxt)
{
	e_book_async_get_book_view(book, query, NULL, -1, (EBookBookViewCallback) book_view_loaded, ctxt);
}

static void
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

static void get_font( char *data, GnomeFont **variable )
{
	if ( data ) {
		GnomeFont *font = gnome_font_find_from_full_name( data );
		if ( font ) {
			g_object_unref(*variable);
			*variable = font;
		}
	}
}


static void
e_contact_build_style(EContactPrintStyle *style)
{
	xmlDocPtr styledoc;
	gchar *filename;

	style->title = g_strdup("");
	style->type = E_CONTACT_PRINT_TYPE_CARDS;
	style->sections_start_new_page = TRUE;
	style->num_columns = 2;
	style->blank_forms = 2;
	style->letter_tabs = TRUE;
	style->letter_headings = FALSE;

	style->headings_font = gnome_font_find_closest_from_weight_slant ("Sans", GNOME_FONT_BOLD, FALSE, 8);
	style->body_font = gnome_font_find_closest_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

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

	style->header_font = gnome_font_find_closest_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

	style->left_header = g_strdup("");
	style->center_header = g_strdup("");
	style->right_header = g_strdup("");

	style->footer_font = gnome_font_find_closest_from_weight_slant ("Sans", GNOME_FONT_BOOK, FALSE, 6);

	style->left_footer = g_strdup("");
	style->center_footer = g_strdup("");
	style->right_footer = g_strdup("");
	style->reverse_on_even_pages = FALSE;
	filename = g_concat_dir_and_file(EVOLUTION_ECPSDIR, "medbook.ecps");
	styledoc = xmlParseFile(filename);
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
			} else if ( !strcmp( node->name, "letter_tabs" ) ) {
				style->letter_tabs = get_bool(data);
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

static void
e_contact_print_response(GtkWidget *dialog, gint response_id, gpointer data)
{
	EContactPrintContext *ctxt = g_new(EContactPrintContext, 1);
	EContactPrintStyle *style = g_new(EContactPrintStyle, 1);
	GnomePrintJob *master;
	GnomePrintConfig *config;
	GnomePrintContext *pc;
	gboolean uses_book = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "uses_book"));
	gboolean uses_list = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "uses_list"));
	EBook *book = NULL;
	EBookQuery *query = NULL;
	EContact *contact = NULL;
	GList *contact_list = NULL;
	gdouble font_size;

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
	switch( response_id ) {
	case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
		config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG(dialog));
		master = gnome_print_job_new( config );
		pc = gnome_print_job_get_context( master );
		e_contact_build_style(style);
		
		ctxt->x = 0;
		ctxt->y = 0;
		ctxt->column = 0;
		ctxt->style = style;
		ctxt->master = master;
		ctxt->first_section = TRUE;
		ctxt->first_char_on_page = 'A' - 1;
		ctxt->type = GNOME_PRINT_DIALOG_RESPONSE_PRINT;

		font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
		ctxt->letter_heading_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), gnome_font_get_size (ctxt->style->headings_font) * 1.5);
		ctxt->letter_tab_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), font_size);
	
		ctxt->pc = pc;
#warning FIXME gnome_print_multipage_new_from_sizes
#if 0
		ctxt->pc = GNOME_PRINT_CONTEXT(gnome_print_multipage_new_from_sizes(pc, 
										   72 * style->paper_width, 
										   72 * style->paper_height,
										   72 * style->page_width,
										   72 * style->page_height));
#endif
		
		ctxt->book = book;
		ctxt->query = query;
		if (uses_book) {
			ctxt->contacts = NULL;
			e_contact_do_print(book, ctxt->query, ctxt);
		}
		else if (uses_list) {
			ctxt->contacts = contact_list;
			complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, ctxt);
		}
		else {
			ctxt->contacts = g_list_append(NULL, contact);
			complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, ctxt);
		}
		gtk_widget_destroy (dialog);
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
		config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG(dialog));
		master = gnome_print_job_new( config );
		pc = gnome_print_job_get_context( master );
		e_contact_build_style(style);
		
		ctxt->x = 0;
		ctxt->y = 0;
		ctxt->column = 0;
		ctxt->style = style;
		ctxt->master = master;
		ctxt->first_section = TRUE;
		ctxt->first_char_on_page = 'A' - 1;
		ctxt->type = GNOME_PRINT_DIALOG_RESPONSE_PREVIEW;

		font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
		ctxt->letter_heading_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), gnome_font_get_size (ctxt->style->headings_font) * 1.5);
		ctxt->letter_tab_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), font_size);
		
		ctxt->pc = pc;
#warning FIXME gnome_print_multipage_new_from_sizes
#if 0
		ctxt->pc = GNOME_PRINT_CONTEXT(gnome_print_multipage_new_from_sizes(pc, 
										   72 * style->paper_width, 
										   72 * style->paper_height,
										   72 * style->page_width,
										   72 * style->page_height));
#endif
		ctxt->book = book;
		ctxt->query = query;

		if (uses_book) {
			ctxt->contacts = NULL;
			g_object_ref(book);
			e_contact_do_print(book, ctxt->query, ctxt);
		}
		else if (uses_list) {
			ctxt->contacts = g_list_copy (contact_list);
			g_list_foreach (ctxt->contacts, (GFunc)g_object_ref, NULL);
			complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, ctxt);
		}
		else {
			ctxt->contacts = g_list_append(NULL, contact);
			g_object_ref(contact);
			complete_sequence(NULL, E_BOOK_VIEW_STATUS_OK, ctxt);
		}
		break;
	case GNOME_PRINT_DIALOG_RESPONSE_CANCEL:
		if (uses_book)
			g_object_unref(book);
		else if (uses_list)
			e_free_object_list (contact_list);
		else
			g_object_unref(contact);
				
		if (query) 
		        e_book_query_unref (query);

		gtk_widget_destroy (dialog);
		g_free(style);
		g_free(ctxt);
		break;
	}
}

GtkWidget *
e_contact_print_dialog_new(EBook *book, char *query)
{
	GtkWidget *dialog;
	
	
	dialog = gnome_print_dialog_new(NULL, _("Print contacts"), GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
	gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
					       NULL, NULL, NULL);

	g_object_ref(book);
	g_object_set_data(G_OBJECT(dialog), "uses_book", GINT_TO_POINTER (TRUE));
	g_object_set_data(G_OBJECT(dialog), "uses_list", GINT_TO_POINTER (FALSE));
	g_object_set_data(G_OBJECT(dialog), "book", book);
	g_object_set_data(G_OBJECT(dialog), "query", e_book_query_from_string  (query));
	g_signal_connect(dialog,
			 "response", G_CALLBACK(e_contact_print_response), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_close), NULL);
	return dialog;
}

void
e_contact_print_preview(EBook *book, char *query)
{
	EContactPrintContext *ctxt = g_new(EContactPrintContext, 1);
	EContactPrintStyle *style = g_new(EContactPrintStyle, 1);
	GnomePrintJob *master;
	GnomePrintContext *pc;
	GnomePrintConfig *config;
	gdouble font_size;

	master = gnome_print_job_new(NULL);
	config = gnome_print_job_get_config (master);
	gnome_print_config_set_int (config, GNOME_PRINT_KEY_NUM_COPIES, 1);
	pc = gnome_print_job_get_context (master);
	e_contact_build_style (style);

	ctxt->x = 0;
	ctxt->y = 0;
	ctxt->column = 0;
	ctxt->style = style;
	ctxt->master = master;
	ctxt->first_section = TRUE;
	ctxt->first_char_on_page = 'A' - 1;
	ctxt->type = GNOME_PRINT_DIALOG_RESPONSE_PREVIEW;

	font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
	ctxt->letter_heading_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), gnome_font_get_size (ctxt->style->headings_font) * 1.5);
	ctxt->letter_tab_font = gnome_font_find (gnome_font_get_name(ctxt->style->headings_font), font_size);

		ctxt->pc = pc;
#warning FIXME gnome_print_multipage_new_from_sizes
#if 0
	ctxt->pc = GNOME_PRINT_CONTEXT(gnome_print_multipage_new_from_sizes(pc, 
									    72 * style->paper_width, 
									    72 * style->paper_height,
									    72 * style->page_width,
									    72 * style->page_height));
#endif
	ctxt->book = book;
	ctxt->query = e_book_query_from_string (query);
	ctxt->contacts = NULL;
	g_object_ref(book);
	e_contact_do_print(book, ctxt->query, ctxt);
}

GtkWidget *
e_contact_print_contact_dialog_new(EContact *contact)
{
	GtkWidget *dialog;
	
	dialog = gnome_print_dialog_new(NULL, _("Print contact"), GNOME_PRINT_DIALOG_COPIES);

	contact = e_contact_duplicate(contact);
	g_object_set_data(G_OBJECT(dialog), "contact", contact);
	g_object_set_data(G_OBJECT(dialog), "uses_list", GINT_TO_POINTER (FALSE));
	g_object_set_data(G_OBJECT(dialog), "uses_book", GINT_TO_POINTER (FALSE));
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

	dialog = gnome_print_dialog_new(NULL, _("Print contact"), GNOME_PRINT_DIALOG_COPIES);

	g_object_set_data(G_OBJECT(dialog), "contact_list", copied_list);
	g_object_set_data(G_OBJECT(dialog), "uses_list", GINT_TO_POINTER (TRUE));
	g_object_set_data(G_OBJECT(dialog), "uses_book", GINT_TO_POINTER (FALSE));
	g_signal_connect(dialog,
			 "response", G_CALLBACK(e_contact_print_response), NULL);
	g_signal_connect(dialog,
			 "close", G_CALLBACK(e_contact_print_close), NULL);
	return dialog;
}
