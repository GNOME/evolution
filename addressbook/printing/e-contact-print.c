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
#include <ctype.h>

typedef struct _EContactPrintContext EContactPrintContext;

struct _EContactPrintContext
{
	GnomePrintContext *pc;
	GnomePrintMaster *master;
	gdouble x;
	gdouble y;
	EContactPrintStyle *style;
	gboolean first_section;
};

static void
e_contact_output(GnomePrintContext *pc, GnomeFont *font, double x, double y, gchar *text)
{
	gnome_print_gsave(pc);
	gnome_print_setfont(pc, font);
	y -= gnome_font_get_ascender(font);
	gnome_print_moveto(pc, x, y);
	gnome_print_show(pc, text);
	gnome_print_grestore(pc);
}

static void
e_contact_output_and_advance(EContactPrintContext *ctxt, GnomeFont *font, double x, gchar *text)
{
	ctxt->y -= .1 * font->size;
	e_contact_output(ctxt->pc, font, x, ctxt->y, text);
	ctxt->y -= gnome_font_get_ascender(font) - gnome_font_get_descender(font);
	ctxt->y -= .1 * font->size;
}

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

static gchar *e_card_get_string(void *card, gchar *key)
{
	return key;
}

static gchar *e_card_get_string_fileas(void *card)
{
	return "Lahey, Chris";
}

static void
e_contact_print_letter_heading (EContactPrintContext *ctxt, gchar *character)
{
	gdouble ascender, descender;
	gdouble width;

	width = gnome_font_get_width_string(ctxt->style->headings_font, "m ");
	ascender = gnome_font_get_ascender(ctxt->style->headings_font);
	descender = gnome_font_get_descender(ctxt->style->headings_font);
	gnome_print_gsave( ctxt->pc );
	e_contact_rectangle( ctxt->pc, ctxt->x, ctxt->y, ctxt->x + width, ctxt->y - (ctxt->style->headings_font->size + 6), 0, 0, 0);
	gnome_print_setrgbcolor(ctxt->pc, 1, 1, 1);
	ctxt->y -= 4;
	e_contact_output(ctxt->pc, ctxt->style->headings_font, ctxt->x + (width - gnome_font_get_width_string(ctxt->style->headings_font, character))/ 2, ctxt->y, character);
	ctxt->y -= ascender - descender;
	ctxt->y -= 2;
	ctxt->y -= 3;
	gnome_print_grestore( ctxt->pc );
}

static double
e_contact_get_letter_tab_width (EContactPrintContext *ctxt)
{
	return gnome_font_get_width_string(ctxt->style->body_font, "123") + ctxt->style->body_font->size / 5;
}

static double
e_contact_get_letter_heading_height (EContactPrintContext *ctxt)
{
	return ctxt->style->headings_font->size * 2;
}

static void
e_contact_start_new_page(EContactPrintContext *ctxt)
{
	ctxt->x = ctxt->style->left_margin * 72;
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	gnome_print_showpage(ctxt->pc);
}

static double
e_contact_get_card_size(void *card, EContactPrintContext *ctxt, GList *shown_fields)
{
	double height = 0;
	height += ctxt->style->headings_font->size;
	for(; shown_fields; shown_fields = g_list_next(shown_fields)) {
		gchar *field = e_card_get_string(card, shown_fields->data);
		gchar *start = field;
		gchar *text;
		gchar *temp;
		for( text = start, temp = strchr(text, '\n'); temp; text = temp + 1, temp = strchr(text, '\n')) {
			height += ctxt->style->body_font->size;
		}
		height += ctxt->style->body_font->size;
	}
	return height;
}


static void
e_contact_print_card (void *card, EContactPrintContext *ctxt, GList *shown_fields)
{
	gnome_print_gsave(ctxt->pc);
	ctxt->y -= 2;
	e_contact_rectangle(ctxt->pc, ctxt->x, ctxt->y + 2, ctxt->x + (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin) * 72, ctxt->y - (gnome_font_get_ascender(ctxt->style->headings_font) - gnome_font_get_descender(ctxt->style->headings_font)) - 2, .85, .85, .85);
	e_contact_output(ctxt->pc, ctxt->style->headings_font, ctxt->x + 2, ctxt->y, e_card_get_string_fileas(card));
	ctxt->y -= gnome_font_get_ascender(ctxt->style->headings_font) - gnome_font_get_descender(ctxt->style->headings_font);
	ctxt->y -= 2;
	
	for(; shown_fields; shown_fields = g_list_next(shown_fields)) {
		double xoff = 0;
		gchar *field = e_card_get_string(card, shown_fields->data);
		ctxt->y -= .1 * ctxt->style->body_font->size;
		e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, shown_fields->data);
		xoff += gnome_font_get_width_string(ctxt->style->body_font, shown_fields->data);
		e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, ":  ");
		xoff += gnome_font_get_width_string(ctxt->style->body_font, ":  ");
		if (strchr(field, '\n')) {
			gchar *start = g_strdup(field);
			gchar *text;
			gchar *temp;
			for( text = start, temp = strchr(text, '\n'); temp; text = temp + 1, temp = strchr(text, '\n')) {
				*temp = 0;
				e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, text);
				ctxt->y -= gnome_font_get_ascender(ctxt->style->body_font) - gnome_font_get_descender(ctxt->style->body_font);
				ctxt->y -= .2 * ctxt->style->body_font->size;
				xoff = 0;
			}
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, text);
			ctxt->y -= gnome_font_get_ascender(ctxt->style->body_font) - gnome_font_get_descender(ctxt->style->body_font);
			ctxt->y -= .1 * ctxt->style->body_font->size;
			g_free(text);
		} else {
			e_contact_output(ctxt->pc, ctxt->style->body_font, ctxt->x + xoff, ctxt->y, field);
			ctxt->y -= gnome_font_get_ascender(ctxt->style->body_font) - gnome_font_get_descender(ctxt->style->body_font);
			ctxt->y -= .1 * ctxt->style->body_font->size;
		}
	}
	gnome_print_grestore(ctxt->pc);
}

static void
e_contact_do_print (void *book, EContactPrintContext *ctxt, GList *shown_fields)
{
	gchar *character = NULL;
	gchar first_page_character = 'a' - 1;
	void *card = NULL;
	int i;
	gdouble page_height = 72 * (ctxt->style->page_height - ctxt->style->top_margin - ctxt->style->bottom_margin);
	gdouble page_width  = 72 * (ctxt->style->page_width - ctxt->style->left_margin - ctxt->style->right_margin);
	ctxt->y = (ctxt->style->page_height - ctxt->style->top_margin) * 72;
	ctxt->x = (ctxt->style->left_margin) * 72;
	if ( ctxt->style->letter_tabs ) 
		page_width -= e_contact_get_letter_tab_width(ctxt);
	/*
	for(card = e_book_get_first(book); card; card = e_book_get_next(book)) {
	*/
	for (i=0; i < 40; i++) {
		gchar *file_as = e_card_get_string_fileas(card);
		if ( ctxt->style->letter_headings && file_as && (!character || *character != tolower(*file_as)) ) {
			if (ctxt->style->sections_start_new_page)
				e_contact_start_new_page(ctxt);
			else if (ctxt->y - e_contact_get_letter_heading_height(ctxt) - e_contact_get_card_size(card, ctxt, shown_fields) < ctxt->style->bottom_margin * 72)
				e_contact_start_new_page(ctxt);
			if (!character)
				character = g_strdup(" ");
			*character = tolower(*file_as);
			e_contact_print_letter_heading(ctxt, character);
			ctxt->first_section = FALSE;
		} 
		if (ctxt->y - e_contact_get_card_size(card, ctxt, shown_fields) < ctxt->style->bottom_margin * 72) {
			e_contact_start_new_page(ctxt);
			e_contact_print_letter_heading(ctxt, character);
		}
		e_contact_print_card(card, ctxt, shown_fields);
	}
	gnome_print_showpage(ctxt->pc);
	gnome_print_context_close(ctxt->pc);
	g_free(character);
}

static void
e_contact_build_style(EContactPrintStyle *style)
{
	xmlDocPtr styledoc;
	style->title = "";
	style->sections_start_new_page = FALSE;
	style->num_columns = 1;
	style->blank_forms = 2;
	style->letter_tabs = TRUE;
	style->letter_headings = TRUE;
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
	style->orientation_portrait = FALSE;
	style->header_font = gnome_font_new("Helvetica", 6);
	style->left_header = "";
	style->center_header = "";
	style->right_header = "";
	style->footer_font = gnome_font_new("Helvetica", 6);
	style->left_footer = "";
	style->center_footer = "";
	style->right_footer = "";
	style->reverse_on_even_pages = FALSE;
	
	styledoc = xmlParseFile("smallbook.ecps");
	if (styledoc) {
#if 0
		xmlNodePtr stylenode = xmlDocGetRootElement(styledoc);
#endif
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
	EContactPrintContext ctxt;
	EContactPrintStyle style;
	GnomePrintMaster *master;
	GtkWidget *preview;
	GnomePrintContext *pc;
	void *book = gtk_object_get_data(GTK_OBJECT(dialog), "book");
	GList *shown_fields = gtk_object_get_data(GTK_OBJECT(dialog), "shown_fields");
	switch( button ) {
	case GNOME_PRINT_PRINT:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
		e_contact_build_style(&style);
		
		ctxt.x = 0;
		ctxt.y = 0;
		ctxt.style = &style;
		ctxt.master = master;
		ctxt.first_section = TRUE;
		
		ctxt.pc = gnome_print_multipage_new_from_sizes(pc, 
							       72 * style.paper_width, 
							       72 * style.paper_height,
							       72 * style.page_width,
							       72 * style.page_height);
		
		e_contact_do_print(book, &ctxt, shown_fields);
		gnome_print_master_print(master);
		gtk_object_unref(GTK_OBJECT(ctxt.pc));
		gtk_object_unref(GTK_OBJECT(master));
		gnome_dialog_close(dialog);
		break;
	case GNOME_PRINT_PREVIEW:
		master = gnome_print_master_new_from_dialog( GNOME_PRINT_DIALOG(dialog) );
		pc = gnome_print_master_get_context( master );
		e_contact_build_style(&style);
		
		ctxt.x = 0;
		ctxt.y = 0;
		ctxt.style = &style;
		ctxt.master = master;
		ctxt.first_section = TRUE;
		
		ctxt.pc = gnome_print_multipage_new_from_sizes(pc, 
							       72 * style.paper_width, 
							       72 * style.paper_height,
							       72 * style.page_width,
							       72 * style.page_height);
		
		e_contact_do_print(book, &ctxt, shown_fields);
		preview = GTK_WIDGET(gnome_print_master_preview_new(master, "Print Preview"));
		gtk_widget_show_all(preview);
		gtk_object_unref(GTK_OBJECT(ctxt.pc));
		gtk_object_unref(GTK_OBJECT(master));
		break;
	case GNOME_PRINT_CANCEL:
		gnome_dialog_close(dialog);
		break;
	}
}

GtkWidget *
e_contact_print_dialog_new(void *book, GList *shown_fields)
{
	GtkWidget *dialog;
	
	
	dialog = gnome_print_dialog_new("Print cards", GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
	gnome_print_dialog_construct_range_any(GNOME_PRINT_DIALOG(dialog), GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_SELECTION,
					       NULL, NULL, NULL);

	gtk_object_set_data(GTK_OBJECT(dialog), "book", book);
	gtk_object_set_data(GTK_OBJECT(dialog), "shown_fields", shown_fields);
	gtk_signal_connect(GTK_OBJECT(dialog),
			   "clicked", e_contact_print_button, NULL);
	gtk_signal_connect(GTK_OBJECT(dialog),
			   "close", e_contact_print_close, NULL);
	return dialog;
}
