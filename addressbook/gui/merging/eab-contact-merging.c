/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Code for checking for duplicates when doing EContact work.
 *
 * Authors:
 *   Christopher James Lahey <clahey@ximian.com>
 *   Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 2001, 2002, 2003, Ximian, Inc.
 */

#include <config.h>

#include "eab-contact-merging.h"
#include "eab-contact-compare.h"
#include <glade/glade.h>
#include <gtk/gtksignal.h>
#include "addressbook/gui/widgets/eab-contact-display.h"

typedef enum {
	E_CONTACT_MERGING_ADD,
	E_CONTACT_MERGING_COMMIT
} EContactMergingOpType;

typedef struct {
	EContactMergingOpType op;
	EBook *book;
	EContact *contact;
	GList *avoid;
	EBookIdCallback id_cb;
	EBookCallback   cb;
	gpointer closure;
} EContactMergingLookup;

static void match_query_callback (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure);

#define SIMULTANEOUS_MERGING_REQUESTS 20

static GList *merging_queue = NULL;
static int running_merge_requests = 0;


static void
add_lookup (EContactMergingLookup *lookup)
{
	if (running_merge_requests < SIMULTANEOUS_MERGING_REQUESTS) {
		running_merge_requests++;
		eab_contact_locate_match_full (lookup->book, lookup->contact, lookup->avoid, match_query_callback, lookup);
	}
	else {
		merging_queue = g_list_append (merging_queue, lookup);
	}
}

static void
finished_lookup (void)
{
	running_merge_requests--;

	while (running_merge_requests < SIMULTANEOUS_MERGING_REQUESTS) {
		EContactMergingLookup *lookup;
		
		if (!merging_queue)
			break;

		lookup = merging_queue->data;

		merging_queue = g_list_remove_link (merging_queue, merging_queue);

		running_merge_requests++;
		eab_contact_locate_match_full (lookup->book, lookup->contact, lookup->avoid, match_query_callback, lookup);
	}
}

static void
free_lookup (EContactMergingLookup *lookup)
{
	g_object_unref (lookup->book);
	g_object_unref (lookup->contact);
	g_list_free (lookup->avoid);

	g_free (lookup);
}

static void
final_id_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->id_cb)
		lookup->id_cb (lookup->book, status, id, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
final_cb (EBook *book, EBookStatus status, gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if (lookup->cb)
		lookup->cb (lookup->book, status, lookup->closure);

	free_lookup (lookup);

	finished_lookup ();
}

static void
doit (EContactMergingLookup *lookup)
{
	if (lookup->op == E_CONTACT_MERGING_ADD)
		e_book_async_add_contact (lookup->book, lookup->contact, final_id_cb, lookup);
	else if (lookup->op == E_CONTACT_MERGING_COMMIT)
		e_book_async_commit_contact (lookup->book, lookup->contact, final_cb, lookup);
}

static void
cancelit (EContactMergingLookup *lookup)
{
	if (lookup->op == E_CONTACT_MERGING_ADD) {
		if (lookup->id_cb)
			final_id_cb (lookup->book, E_BOOK_ERROR_CANCELLED, NULL, lookup);
	} else if (lookup->op == E_CONTACT_MERGING_COMMIT) {
		if (lookup->cb)
			final_cb (lookup->book, E_BOOK_ERROR_CANCELLED, lookup);
	}
}

static void
response (GtkWidget *dialog, int response, EContactMergingLookup *lookup)
{
	gtk_widget_destroy (dialog);

	switch (response) {
	case 0:
		doit (lookup);
		break;
	case 1:
		cancelit (lookup);
		break;
	}
}

static void
match_query_callback (EContact *contact, EContact *match, EABContactMatchType type, gpointer closure)
{
	EContactMergingLookup *lookup = closure;

	if ((gint) type <= (gint) EAB_CONTACT_MATCH_VAGUE) {
		doit (lookup);
	} else {
		GladeXML *ui;
		
		GtkWidget *widget;

		if (lookup->op == E_CONTACT_MERGING_ADD)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/eab-contact-duplicate-detected.glade", NULL, NULL);
		else if (lookup->op == E_CONTACT_MERGING_COMMIT)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/eab-contact-commit-duplicate-detected.glade", NULL, NULL);
		else {
			doit (lookup);
			return;
		}

		widget = glade_xml_get_widget (ui, "custom-old-contact");
		eab_contact_display_render (EAB_CONTACT_DISPLAY (widget),
					    match, EAB_CONTACT_DISPLAY_RENDER_COMPACT);

		widget = glade_xml_get_widget (ui, "custom-new-contact");
		eab_contact_display_render (EAB_CONTACT_DISPLAY (widget),
					    contact, EAB_CONTACT_DISPLAY_RENDER_COMPACT);

		widget = glade_xml_get_widget (ui, "dialog-duplicate-contact");

		g_signal_connect (widget, "response",
				  G_CALLBACK (response), lookup);

		gtk_widget_show_all (widget);
	}
}

gboolean
eab_merging_book_add_contact (EBook           *book,
			      EContact           *contact,
			      EBookIdCallback  cb,
			      gpointer         closure)
{
	EContactMergingLookup *lookup;

	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_ADD;
	lookup->book = g_object_ref (book);
	lookup->contact = g_object_ref (contact);
	lookup->id_cb = cb;
	lookup->closure = closure;
	lookup->avoid = NULL;

	add_lookup (lookup);

	return TRUE;
}

gboolean
eab_merging_book_commit_contact (EBook                 *book,
				 EContact                 *contact,
				 EBookCallback          cb,
				 gpointer               closure)
{
	EContactMergingLookup *lookup;
	
	lookup = g_new (EContactMergingLookup, 1);

	lookup->op = E_CONTACT_MERGING_COMMIT;
	lookup->book = g_object_ref (book);
	lookup->contact = g_object_ref (contact);
	lookup->cb = cb;
	lookup->closure = closure;
	lookup->avoid = g_list_append (NULL, contact);

	add_lookup (lookup);

	return TRUE;
}

GtkWidget *
_eab_contact_merging_create_contact_display(gchar *name,
					    gchar *string1, gchar *string2,
					    gint int1, gint int2);

GtkWidget *
_eab_contact_merging_create_contact_display(gchar *name,
					    gchar *string1, gchar *string2,
					    gint int1, gint int2)
{
	return eab_contact_display_new();
}
