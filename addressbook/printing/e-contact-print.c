/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print.c
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


#include "e-contact-print.h"
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-dialog.h>
#include <libgnomeprint/gnome-print-master.h>
#include <libgnomeprint/gnome-print-master-preview.h>
#include <libgnomeprint/gnome-print-multipage.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <ctype.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-card.h>
#include <addressbook/backend/ebook/e-card-simple.h>

#define SCALE 5
#define HYPHEN_PIXELS 20
#define HYPHEN_PENALTY ( (SCALE) * (SCALE) * (HYPHEN_PIXELS) * (HYPHEN_PIXELS) )

typedef struct _EContactPrintContext EContactPrintContext;

struct _EContactPrintContext
{
	GnomePrintContext *pc;
	GnomePrintMaster *master;
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

	int type;
	EBook *book;
	gchar *query;

	GList *cards;
};

static gint
e_contact_divide_text(GnomePrintContext *pc, GnomeFont *font, double width, const gchar *text, GList **return_val /* Of type char[] */)
{
	if ( width == -1 || gnome_font_get_width_string(font, text) <= width ) {
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
				if ( (!firstword) && x + gnome_font_get_width_string_n(font, text + lastend, i - lastend) > width ) {
					if (return_val) {
						*return_val = g_list_append(*return_val, g_strndup(text + linestart, lastend - linestart));
					}
					x = gnome_font_get_width_string(font, "    ");
					linestart = lastend + 1;
					x += gnome_font_get_width_string_n(font, text + linestart, i - linestart);
					lastend = i;
					linecount ++;
				} else {
					x += gnome_font_get_width_string_n(font, text + lastend, i - lastend);
					lastend = i;
				}
				firstword = 0;
			} else if ( text[i] == '\n' ) {
				if ( (!firstword) && x + gnome_font_get_width_string_n(font, text + lastend, i - lastend) > width ) {
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
				x = gnome_font_get_width_string(font, "    ");

				firstword = 1;
			}
		}
		if ( (!firstword) && x + gnome_font_get_width_string_n(font, text + lastend, i - lastend) > width ) {
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
		double x = - gnome_font_get_width_string(font, "    ") * SCALE;
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
		y -= .2 * font->size;
		if ( first_line ) {
			x += gnome_font_get_width_string(font, "    ");
			first_line = 0;
		}
	}
	g_list_foreach( list_start, (GFunc) g_free, NULL );
	g_list_free( list_start );
	gnome_print_grestore(pc);
}

static gdouble
e_contact_text_height(GnomePrintContext *pc, GnomeFont *font, double width, gchar *text)
{
	int line_count = e_contact_divide_text(pc, font, width, text, NULL);
	return line_count * (gnome_font_get_ascender(font) + gnome_font_get_descender(font)) +
		line_count * .2 * font->size;
}

#if 0
static void
e_contact_output_and_advance(EContactPrintContext *ctxt, GnomeFont *font, double x, double width, gchar *text)
{
	ctxt->y -= .1 * font->size;
	e_contact_output(ctxt->pc, font, x, ctxt->y, width, text);
	ctxt->y -= e_contact_text_height(ctxt->pc, font, width, text);
	ctxt->y -= .1 * font->size;
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
	return gnome_font_get_width_string(ctxt->letter_tab_font, "123") + 4 + 18;
}

static double
e_contact_print_letter_tab (EContactPrintContext *ctxt)
{
	char character;
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
			e_contact_output( ctxt->pc, ctxt->letter_tab_font, x + tab_width / 2 - gnome_font_get_width_string(ctxt->letter_tab_font, string) / 2, y - (tab_height - font_size) / 2, -1, string );
		} else {
			gnome_print_setrgbcolor( ctxt->pc, 0, 0, 0 );
			e_contact_output( ctxt->pc, ctxt->letter_tab_font, x + tab_width / 2 - gnome_font_get_width_string(ctxt->letter_tab_font, string) / 2, y - (tab_height - font_size) / 2, -1, string );
		}
		y -= tab_height;
	}
	gnome_print_grestore( ctxt->pc );
	return gnome_font_get_width_string(ctxt->style->body_font, "123") + ctxt->style->body_font->size / 5;
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

	width = gnome_font_get_width_string(ctxt->letter_heading_font, "m") * 1.7;
	ascender = gnome_font_get_ascender(ctxt->letter_heading_font);
	descender = gnome_font_get_descender(ctxt->letter_heading_font);
	gnome_print_gsave( ctxt->pc );
	e_contact_rectangle( ctxt->pc, ctxt->x, ctxt->y, ctxt->x + width, ctxt->y - (ascender + descender + 6), 0, 0, 0);
	gnome_print_setrgbcolor(ctxt->pc, 1, 1, 1);
	ctxt->y -= 4;
	e_contact_output(ctxt->pc, ctxt->letter_heading_font, ctxt->x + (width - gnome_font_get_width_string(ctxt->letter_heading_font, character))/ 2, ctxt->y, -1, character);
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

	ctxt->first_char_on_page = ctxt->last_char_on_page + 1;
}

static double
e_contact_get_card_size(ECardSimple *simple, EContactPrintContext *ctxt)
{
	gdouble height = 0;
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	char *file_as;
	gint field;
	if ( ctxt->style->letter_tabs )
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	height += ctxt->style->headings_font->size * .2;

	height += ctxt->style->headings_font->size * .2;

	gtk_object_get(GTK_OBJECT(simple->card),
		       "file_as", &file_as,
		       NULL);
	height += e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as);
	height += ctxt->style->headings_font->size * .2;

	height += ctxt->style->headings_font->size * .2;
	
	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST; field++) {
		char *string;
		string = e_card_simple_get(simple, field);
		if (string && *string) {
			double xoff = 0;
			xoff += gnome_font_get_width_string(ctxt->style->body_font, e_card_simple_get_name(simple, field));
			xoff += gnome_font_get_width_string(ctxt->style->body_font, ":  ");
			height += e_contact_text_height(ctxt->pc, ctxt->style->body_font, column_width - xoff, string);
			height += .2 * ctxt->style->body_font->size;
		}
		g_free(string);
	}
	height += ctxt->style->headings_font->size * .4;
	return height;
}


static void
e_contact_print_card (ECardSimple *simple, EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	char *file_as;
	int field;

	if ( ctxt->style->letter_tabs )
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	gnome_print_gsave(ctxt->pc);

	ctxt->y -= ctxt->style->headings_font->size * .2;

	ctxt->y -= ctxt->style->headings_font->size * .2;

	gtk_object_get(GTK_OBJECT(simple->card),
		       "file_as", &file_as,
		       NULL);
	if (ctxt->style->print_using_grey)
		e_contact_rectangle(ctxt->pc, ctxt->x, ctxt->y + ctxt->style->headings_font->size * .2, ctxt->x + column_width, ctxt->y - e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as) - ctxt->style->headings_font->size * .2, .85, .85, .85);
	e_contact_output(ctxt->pc, ctxt->style->headings_font, ctxt->x + 2, ctxt->y, column_width - 4, file_as);
	ctxt->y -= e_contact_text_height(ctxt->pc, ctxt->style->headings_font, column_width - 4, file_as);
	ctxt->y -= ctxt->style->headings_font->size * .2;

	ctxt->y -= ctxt->style->headings_font->size * .2;
	
	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST; field++) {
		char *string;
		string = e_card_simple_get(simple, field);
		if (string && *string) {
			double xoff = 0;
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, e_card_simple_get_name(simple, field));
			xoff += gnome_font_get_width_string(ctxt->style->body_font, e_card_simple_get_name(simple, field));
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, ":  ");
			xoff += gnome_font_get_width_string(ctxt->style->body_font, ":  ");
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, column_width - xoff, string);
			ctxt->y -= e_contact_text_height(ctxt->pc, ctxt->style->body_font, column_width - xoff, string);
			ctxt->y -= .2 * ctxt->style->body_font->size;
		}
		g_free(string);
	}
	
	ctxt->y -= ctxt->style->headings_font->size * .4;
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
complete_sequence(EBookView *book_view, EContactPrintContext *ctxt)
{
	GList *cards = ctxt->cards;
	for(; cards; cards = cards->next) {
		ECard *card = cards->data;
		ECardSimple *simple = e_card_simple_new(card);
		gchar *file_as;

		gtk_object_get(GTK_OBJECT(card),
			       "file_as", &file_as,
			       NULL);
		if ( file_as && (!ctxt->character || *ctxt->character != tolower(*file_as)) ) {
			if (ctxt->style->sections_start_new_page && ! ctxt->first_contact) {
				e_contact_start_new_page(ctxt);
			}
			else if ((!ctxt->first_contact) && (ctxt->y - e_contact_get_letter_heading_height(ctxt) - e_contact_get_card_size(simple, ctxt) < ctxt->style->bottom_margin * 72))
				e_contact_start_new_column(ctxt);
			if (!ctxt->character)
				ctxt->character = g_strdup(" ");
			*ctxt->character = tolower(*file_as);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
			ctxt->first_section = FALSE;
		} 
		else if ( (!ctxt->first_contact) && (ctxt->y - e_contact_get_card_size(simple, ctxt) < ctxt->style->bottom_margin * 72)) {
			e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, ctxt->character);
		}
		ctxt->last_char_on_page = toupper(*file_as);
		if ( ctxt->last_char_on_page < ctxt->first_char_on_page )
			ctxt->first_char_on_page = ctxt->last_char_on_page;
		e_contact_print_card(simple, ctxt);
		ctxt->first_contact = FALSE;
		gtk_object_unref(GTK_OBJECT(simple));
	}
	ctxt->last_char_on_page = 'Z';
	if ( ctxt->style->letter_tabs )
		e_contact_print_letter_tab(ctxt);
	gnome_print_showpage(ctxt->pc);
	gnome_print_context_close(ctxt->pc);
	g_free(ctxt->character);
	gtk_object_unref(GTK_OBJECT(book_view));
	if (ctxt->type == GNOME_PRINT_PREVIEW) {
		GtkWidget *preview;
		preview = GTK_WIDGET(gnome_print_master_preview_new(ctxt->master, "Print Preview"));
		gtk_widget_show_all(preview);
	} else {
		gnome_print_master_print(ctxt->master);
	}
	gtk_object_unref(GTK_OBJECT(ctxt->pc));
	gtk_object_unref(GTK_OBJECT(ctxt->master));
	gtk_object_unref(GTK_OBJECT(ctxt->book));
	g_free(ctxt->query);
	g_list_foreach(ctxt->cards, (GFunc) gtk_object_unref, NULL);
	g_list_free(ctxt->cards);
	gtk_object_unref(GTK_OBJECT(ctxt->style->headings_font));
	gtk_object_unref(GTK_OBJECT(ctxt->style->body_font));
	gtk_object_unref(GTK_OBJECT(ctxt->style->header_font));
	gtk_object_unref(GTK_OBJECT(ctxt->style->footer_font));
	gtk_object_unref(GTK_OBJECT(ctxt->letter_heading_font));
	gtk_object_unref(GTK_OBJECT(ctxt->letter_tab_font));
	g_free(ctxt->style);
	g_free(ctxt);
}

static int
card_compare (ECard *card1, ECard *card2) {
	if (card1 && card2) {
		char *file_as1, *file_as2;
		gtk_object_get(GTK_OBJECT(card1),
			       "file_as", &file_as1,
			       NULL);
		gtk_object_get(GTK_OBJECT(card2),
			       "file_as", &file_as2,
			       NULL);
		if (file_as1 && file_as2)
			return strcmp(file_as1, file_as2);
		if (file_as1)
			return -1;
		if (file_as2)
			return 1;
		return strcmp(e_card_get_id(card1), e_card_get_id(card2));
	} else {
		return 0;
	}
}

static void
create_card(EBookView *book_view, const GList *cards, EContactPrintContext *ctxt)
{
	for(; cards; cards = cards->next) {
		ECard *card = cards->data;
		gtk_object_ref(GTK_OBJECT(card));
		ctxt->cards = g_list_insert_sorted(ctxt->cards, card, (GCompareFunc) card_compare);
	}
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, EContactPrintContext *ctxt)
{
	gtk_object_ref(GTK_OBJECT(book_view));

	gtk_signal_connect(GTK_OBJECT(book_view),
			   "card_added",
			   GTK_SIGNAL_FUNC(create_card),
			   ctxt);

	gtk_signal_connect(GTK_OBJECT(book_view),
			   "sequence_complete",
			   GTK_SIGNAL_FUNC(complete_sequence),
			   ctxt);
}

static void
e_contact_do_print_cards (EBook *book, char *query, EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	ctxt->first_contact = TRUE;
	ctxt->character = NULL;
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	ctxt->x = (ctxt->style->left_margin) * 72;
	if ( ctxt->style->letter_tabs ) 
		page_width -= e_contact_get_letter_tab_width(ctxt);
	
	ctxt->first_char_on_page = 'A' - 1;

	e_book_get_book_view(book, query, (EBookBookViewCallback) book_view_loaded, ctxt);
}

#if 0
static double
e_contact_get_phone_list_size(ECardSimple *simple, EContactPrintContext *ctxt)
{
	double height = 0;
	int field;

	height += ctxt->style->headings_font->size * .2;

	height += ctxt->style->headings_font->size * .2;
	
	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST; field++) {
		char *string;
		string = e_card_simple_get(simple, field);
		if (string && *string) {
			if ( 1 ) /* field is a phone field. */ {
				gchar *field = string;
				height += e_contact_text_height(ctxt->pc, ctxt->style->body_font, 100, field);
				height += .2 * ctxt->style->body_font->size;
			}
		}
		g_free(string);
	}
	height += ctxt->style->headings_font->size * .4;
	return height;
}


static void
e_contact_print_phone_list (ECard *card, EContactPrintContext *ctxt)
{
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	double xoff, dotwidth;
	int dotcount;
	char *dots;
	int i;
	char *file_as;
	if ( ctxt->style->letter_tabs )
		page_width -= e_contact_get_letter_tab_width(ctxt);
	column_width = (page_width + 18) / ctxt->style->num_columns - 18;

	gnome_print_gsave(ctxt->pc);

	ctxt->y -= ctxt->style->headings_font->size * .2;

	ctxt->y -= ctxt->style->headings_font->size * .2;

	e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x, ctxt->y, -1, e_card_get_string_fileas(card));
	
	xoff = column_width - 9 * ctxt->style->body_font->size;
	dotwidth = xoff - 
		gnome_font_get_width_string(ctxt->style->body_font, e_card_get_string_fileas(card)) - 
		gnome_font_get_width_string(ctxt->style->body_font, " ");
	dotcount = dotwidth / gnome_font_get_width(ctxt->style->body_font, '.');
	dots = g_new(gchar, dotcount + 1);
	for (i = 0; i < dotcount; i++)
		dots[i] = '.';
	dots[dotcount] = 0;
	e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff - dotcount * gnome_font_get_width(ctxt->style->body_font, '.'), ctxt->y, -1, dots);
	g_free(dots);
		
	for(; shown_fields; shown_fields = g_list_next(shown_fields)) {
		if ( 1 ) /* field is a phone field. */ {
			gchar *field = e_card_get_string(card, shown_fields->data);
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, -1, shown_fields->data);
			e_contact_output(ctxt->pc, ctxt->style->body_font, 
					 ctxt->x + column_width - gnome_font_get_width_string(ctxt->style->body_font, 
											      field),
					 ctxt->y,
					 -1,
					 field);
			ctxt->y -= e_contact_text_height(ctxt->pc, ctxt->style->body_font, 100, field);
			ctxt->y -= .2 * ctxt->style->body_font->size;
		}
	}
	ctxt->y -= ctxt->style->headings_font->size * .4;
	gnome_print_grestore(ctxt->pc);
}

static void
e_contact_do_print_phone_list (EBook *book, char *query, EContactPrintContext *ctxt)
{
	ECard *card = NULL;
	int i;
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	gdouble column_width;
	ctxt->first_contact = TRUE;
	ctxt->character = NULL;
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	ctxt->x = (ctxt->style->left_margin) * 72;
	if ( ctxt->style->letter_tabs ) 
		page_width -= e_contact_get_letter_tab_width(ctxt);
	
	ctxt->first_char_on_page = 'A' - 1;

	column_width = (page_width + 18) / ctxt->style->num_columns - 18;
	/*
	for(card = e_book_get_first(book); card; card = e_book_get_next(book)) {
	*/
	for (i=0; i < 30; i++) {
		gchar *file_as = e_card_get_string_fileas(card);
		if ( file_as && (!character || *character != tolower(*file_as)) ) {
			if (ctxt->style->sections_start_new_page && ! first_contact) {
				e_contact_start_new_page(ctxt);
			}
			else if ((!first_contact) && (ctxt->y - e_contact_get_letter_heading_height(ctxt) - e_contact_get_phone_list_size(card, ctxt, shown_fields) < ctxt->style->bottom_margin * 72))
				e_contact_start_new_column(ctxt);
			if (!character)
				character = g_strdup(" ");
			*character = tolower(*file_as);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, character);
			ctxt->first_section = FALSE;
		} 
		else if ( (!first_contact) && (ctxt->y - e_contact_get_card_size(card, ctxt, shown_fields) < ctxt->style->bottom_margin * 72)) {
			e_contact_start_new_column(ctxt);
			if ( ctxt->style->letter_headings )
				e_contact_print_letter_heading(ctxt, character);
		}
		ctxt->last_char_on_page = toupper(*file_as);
		if ( ctxt->last_char_on_page < ctxt->first_char_on_page )
			ctxt->first_char_on_page = ctxt->last_char_on_page;
		e_contact_print_phone_list(card, ctxt, shown_fields);
		first_contact = FALSE;
	}
	ctxt->last_char_on_page = 'Z';
	if ( ctxt->style->letter_tabs )
		e_contact_print_letter_tab(ctxt);
	gnome_print_showpage(ctxt->pc);
	gnome_print_context_close(ctxt->pc);
	g_free(character);
}
#endif

static void
e_contact_do_print (EBook *book, char *query, EContactPrintContext *ctxt)
{
	switch ( ctxt->style->type ) {
	case E_CONTACT_PRINT_TYPE_CARDS:
		e_contact_do_print_cards( book, query, ctxt);
		break;
#if 0
	case E_CONTACT_PRINT_TYPE_PHONE_LIST:
		e_contact_do_print_phone_list( book, query, ctxt );
		break;
#endif
	default:
		break;
	}
}

static void lowify( char *data )
{
	for ( ; *data; data++ )
		*data = tolower(*data);
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
		GnomeFont *font = gnome_font_new_from_full_name( data );
		if ( font ) {
			gtk_object_unref( GTK_OBJECT(*variable) );
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
	style->headings_font = gnome_font_new("Helvetica-Bold", 8);
	style->body_font = gnome_font_new("Helvetica", 6);
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
	style->header_font = gnome_font_new("Helvetica", 6);
	style->left_header = g_strdup("");
	style->center_header = g_strdup("");
	style->right_header = g_strdup("");
	style->footer_font = gnome_font_new("Helvetica", 6);
	style->left_footer = g_strdup("");
	style->center_footer = g_strdup("");
	style->right_footer = g_strdup("");
	style->reverse_on_even_pages = FALSE;
	filename = g_concat_dir_and_file(EVOLUTION_ECPSDIR, "smallbook.ecps");
	styledoc = xmlParseFile(filename);
	g_free(filename);
	if (styledoc) {
		xmlNodePtr stylenode = xmlDocGetRootElement(styledoc);
		xmlNodePtr node;
		for (node = stylenode->childs; node; node = node->next) {
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
e_contact_print_button(GnomeDialog *dialog, gint button, gpointer data)
{
	EContactPrintContext *ctxt = g_new(EContactPrintContext, 1);
	EContactPrintStyle *style = g_new(EContactPrintStyle, 1);
	GnomePrintMaster *master;
	GnomePrintContext *pc;
	EBook *book = gtk_object_get_data(GTK_OBJECT(dialog), "book");
	char *query = gtk_object_get_data(GTK_OBJECT(dialog), "query");
	gdouble font_size;
	switch( button ) {
	case GNOME_PRINT_PRINT:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
		e_contact_build_style(style);
		
		ctxt->x = 0;
		ctxt->y = 0;
		ctxt->column = 0;
		ctxt->style = style;
		ctxt->master = master;
		ctxt->first_section = TRUE;
		ctxt->first_char_on_page = 'A' - 1;
		ctxt->type = GNOME_PRINT_PRINT;

		font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
		ctxt->letter_heading_font = gnome_font_new(gnome_font_get_name(ctxt->style->headings_font), ctxt->style->headings_font->size * 1.5);
		ctxt->letter_tab_font = gnome_font_new(gnome_font_get_name(ctxt->style->headings_font), font_size);
	
		ctxt->pc = GNOME_PRINT_CONTEXT(gnome_print_multipage_new_from_sizes(pc, 
										   72 * style->paper_width, 
										   72 * style->paper_height,
										   72 * style->page_width,
										   72 * style->page_height));
		
		ctxt->book = book;
		ctxt->query = query;
		ctxt->cards = NULL;
		e_contact_do_print(book, ctxt->query, ctxt);
		gnome_dialog_close(dialog);
		break;
	case GNOME_PRINT_PREVIEW:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
		e_contact_build_style(style);
		
		ctxt->x = 0;
		ctxt->y = 0;
		ctxt->column = 0;
		ctxt->style = style;
		ctxt->master = master;
		ctxt->first_section = TRUE;
		ctxt->first_char_on_page = 'A' - 1;
		ctxt->type = GNOME_PRINT_PREVIEW;

		font_size = 72 * ctxt->style->page_height / 27.0 / 2.0;
		ctxt->letter_heading_font = gnome_font_new(gnome_font_get_name(ctxt->style->headings_font), ctxt->style->headings_font->size * 1.5);
		ctxt->letter_tab_font = gnome_font_new(gnome_font_get_name(ctxt->style->headings_font), font_size);
		
		ctxt->pc = GNOME_PRINT_CONTEXT(gnome_print_multipage_new_from_sizes(pc, 
										   72 * style->paper_width, 
										   72 * style->paper_height,
										   72 * style->page_width,
										   72 * style->page_height));
		
		gtk_object_ref(GTK_OBJECT(book));
		ctxt->book = book;
		ctxt->query = g_strdup(query);
		ctxt->cards = NULL;
		e_contact_do_print(book, ctxt->query, ctxt);
		break;
	case GNOME_PRINT_CANCEL:
		gtk_object_unref(GTK_OBJECT(book));
		g_free(query);
		gnome_dialog_close(dialog);
		g_free(style);
		g_free(ctxt);
		break;
	}
}

GtkWidget *
e_contact_print_dialog_new(EBook *book, char *query)
{
	GtkWidget *dialog;
	
	
	dialog = gnome_print_dialog_new("Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
	gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
					       NULL, NULL, NULL);

	gtk_object_ref(GTK_OBJECT(book));
	gtk_object_set_data(GTK_OBJECT(dialog), "book", book);
	gtk_object_set_data(GTK_OBJECT(dialog), "query", g_strdup(query));
	gtk_signal_connect(GTK_OBJECT(dialog),
			   "clicked", GTK_SIGNAL_FUNC(e_contact_print_button), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog),
			   "close", GTK_SIGNAL_FUNC(e_contact_print_close), NULL);
	return dialog;
}
