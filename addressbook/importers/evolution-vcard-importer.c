/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar importer component
 *
 * Copyright (C) 2004  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Chris Toshok <toshok@ximian.com>
 *          JP Rosevear <jpr@ximian.com>
 * 	    Michael Zucchi <notzed@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <gtk/gtkvbox.h>
#include <glib/gi18n.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-selector.h>

#include <util/eab-book-util.h>
#include <libebook/e-destination.h>

#include "e-util/e-import.h"

#include "evolution-addressbook-importers.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	int state;		/* 0 - importing, 1 - cancelled/complete */
	int total;
	int count;

	ESource *primary;
	
	GList *contactlist;
	GList *iterator;
	EBook *book;
} VCardImporter;

static void vcard_import_done(VCardImporter *gci);

static void
add_to_notes (EContact *contact, EContactField field)
{
	const gchar *old_text;
	const gchar *field_text;
	gchar       *new_text;

	old_text = e_contact_get_const (contact, E_CONTACT_NOTE);
	if (old_text && strstr (old_text, e_contact_pretty_name (field)))
		return;

	field_text = e_contact_get_const (contact, field);
	if (!field_text || !*field_text)
		return;

	new_text = g_strdup_printf ("%s%s%s: %s",
				    old_text ? old_text : "",
				    old_text && *old_text &&
				    *(old_text + strlen (old_text) - 1) != '\n' ? "\n" : "",
				    e_contact_pretty_name (field), field_text);
	e_contact_set (contact, E_CONTACT_NOTE, new_text);
	g_free (new_text);
}

static void
vcard_import_contact(VCardImporter *gci, EContact *contact)
{
	EContactPhoto *photo;
	GList *attrs, *attr;

	/* Apple's addressbook.app exports PHOTO's without a TYPE
	   param, so let's figure out the format here if there's a
	   PHOTO attribute missing a TYPE param.

	   this is sort of a hack, as EContact sets the type for us if
	   we use the setter.  so let's e_contact_get + e_contact_set
	   on E_CONTACT_PHOTO.
	*/
	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (photo) {
		e_contact_set (contact, E_CONTACT_PHOTO, photo);
		e_contact_photo_free (photo);
	}

	/* Deal with our XML EDestination stuff in EMAIL attributes, if there is any. */
	attrs = e_contact_get_attributes (contact, E_CONTACT_EMAIL);
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		GList *v = e_vcard_attribute_get_values (a);

		if (v && v->data) {
			if (!strncmp ((char*)v->data, "<?xml", 5)) {
				EDestination *dest = e_destination_import ((char*)v->data);

				e_destination_export_to_vcard_attribute (dest, a);

				g_object_unref (dest);

			}
		}
	}
	e_contact_set_attributes (contact, E_CONTACT_EMAIL, attrs);

	/*
	  Deal with TEL attributes that don't conform to what we need.

	  1. if there's no location (HOME/WORK/OTHER), default to OTHER.
	  2. if there's *only* a location specified, default to VOICE.
	*/
	attrs = e_vcard_get_attributes (E_VCARD (contact));
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		gboolean location_only = TRUE;
		gboolean no_location = TRUE;
		GList *params, *param;

		if (g_ascii_strcasecmp (e_vcard_attribute_get_name (a),
					EVC_TEL))
			continue;

		params = e_vcard_attribute_get_params (a);
		for (param = params; param; param = param->next) {
			EVCardAttributeParam *p = param->data;
			GList *vs, *v;

			if (g_ascii_strcasecmp (e_vcard_attribute_param_get_name (p),
						EVC_TYPE))
				continue;

			vs = e_vcard_attribute_param_get_values (p);
			for (v = vs; v; v = v->next) {
				if (!g_ascii_strcasecmp ((char*)v->data, "WORK") ||
				    !g_ascii_strcasecmp ((char*)v->data, "HOME") ||
				    !g_ascii_strcasecmp ((char*)v->data, "OTHER"))
					no_location = FALSE;
				else
					location_only = FALSE;
			}
		}

		if (location_only) {
			/* add VOICE */
			e_vcard_attribute_add_param_with_value (a,
								e_vcard_attribute_param_new (EVC_TYPE),
								"VOICE");
		}
		if (no_location) {
			/* add OTHER */
			e_vcard_attribute_add_param_with_value (a,
								e_vcard_attribute_param_new (EVC_TYPE),
								"OTHER");
		}
	}

	/*
	  Deal with ADR attributes that don't conform to what we need.

	  if HOME or WORK isn't specified, add TYPE=OTHER.
	*/
	attrs = e_vcard_get_attributes (E_VCARD (contact));
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		gboolean no_location = TRUE;
		GList *params, *param;

		if (g_ascii_strcasecmp (e_vcard_attribute_get_name (a),
					EVC_ADR))
			continue;

		params = e_vcard_attribute_get_params (a);
		for (param = params; param; param = param->next) {
			EVCardAttributeParam *p = param->data;
			GList *vs, *v;

			if (g_ascii_strcasecmp (e_vcard_attribute_param_get_name (p),
						EVC_TYPE))
				continue;

			vs = e_vcard_attribute_param_get_values (p);
			for (v = vs; v; v = v->next) {
				if (!g_ascii_strcasecmp ((char*)v->data, "WORK") ||
				    !g_ascii_strcasecmp ((char*)v->data, "HOME"))
					no_location = FALSE;
			}
		}

		if (no_location) {
			/* add OTHER */
			e_vcard_attribute_add_param_with_value (a,
								e_vcard_attribute_param_new (EVC_TYPE),
								"OTHER");
		}
	}

	/* Work around the fact that these fields no longer show up in the UI */
	add_to_notes (contact, E_CONTACT_OFFICE);
	add_to_notes (contact, E_CONTACT_SPOUSE);
	add_to_notes (contact, E_CONTACT_BLOG_URL);

	/* FIXME Error checking */
	e_book_add_contact (gci->book, contact, NULL);
}

static gboolean
vcard_import_contacts(void *data)
{
	VCardImporter *gci = data;
	int count = 0;
	GList *iterator = gci->iterator;

	if (gci->state == 0) {
		while (count < 50 && iterator) {
			vcard_import_contact(gci, iterator->data);
			count++;
			iterator = iterator->next;
		}
		gci->count += count;
		gci->iterator = iterator;
		if (iterator == NULL)
			gci->state = 1;
	}
	if (gci->state == 1) {
		vcard_import_done(gci);
		return FALSE;
	} else {
		e_import_status(gci->import, gci->target, _("Importing ..."), gci->count * 100 / gci->total);
		return TRUE;
	}
}

#define BOM (gunichar2)0xFEFF
#define ANTIBOM (gunichar2)0xFFFE

static gboolean
has_bom (const gunichar2 *utf16)
{
	
	if ((utf16 == NULL) || (*utf16 == '\0')) {
		return FALSE;
	}

	return ((*utf16 == BOM) || (*utf16 == ANTIBOM));
}

static void
fix_utf16_endianness (gunichar2 *utf16)
{
	gunichar2 *it;


	if ((utf16 == NULL) || (*utf16 == '\0')) {
		return;		
	}

	if (*utf16 != ANTIBOM) {
		return;
	}

	for (it = utf16; *it != '\0'; it++) {
		*it = GUINT16_SWAP_LE_BE (*it);
	}
}

/* Converts an UTF-16 string to an UTF-8 string removing the BOM character 
 * WARNING: this may modify the utf16 argument if the function detects the
 * string isn't using the local endianness
 */
static gchar *
utf16_to_utf8 (gunichar2 *utf16)
{
	
	if (utf16 == NULL) {
		return NULL;
	}

	fix_utf16_endianness (utf16);

	if (*utf16 == BOM) {
		utf16++;
	}

	return g_utf16_to_utf8 (utf16, -1, NULL, NULL, NULL);
}

enum _VCardEncoding {
	VCARD_ENCODING_NONE,
	VCARD_ENCODING_UTF8,
	VCARD_ENCODING_UTF16,
	VCARD_ENCODING_LOCALE
};

typedef enum _VCardEncoding VCardEncoding;

/* Actually check the contents of this file */
static VCardEncoding
guess_vcard_encoding (const char *filename)
{
	FILE *handle;
	char line[4096];
	char *line_utf8;
	VCardEncoding encoding = VCARD_ENCODING_NONE;

	handle = fopen (filename, "r");
	if (handle == NULL) {
		g_print ("\n");
		return VCARD_ENCODING_NONE;
	}
		
	fgets (line, 4096, handle);
	if (line == NULL) {
		fclose (handle);
		g_print ("\n");
		return VCARD_ENCODING_NONE;
	}
	fclose (handle);
	
	if (has_bom ((gunichar2*)line)) {
		gunichar2 *utf16 = (gunichar2*)line;
		/* Check for a BOM to try to detect UTF-16 encoded vcards
		 * (MacOSX address book creates such vcards for example)
		 */
		line_utf8 = utf16_to_utf8 (utf16);
		if (line_utf8 == NULL) {
			return VCARD_ENCODING_NONE;
		}
		encoding = VCARD_ENCODING_UTF16;
	} else if (g_utf8_validate (line, -1, NULL)) {
		line_utf8 = g_strdup (line);
		encoding = VCARD_ENCODING_UTF8;
	} else {
		line_utf8 = g_locale_to_utf8 (line, -1, NULL, NULL, NULL);
		if (line_utf8 == NULL) {
			return VCARD_ENCODING_NONE;
		}
		encoding = VCARD_ENCODING_LOCALE;
	}

	if (g_ascii_strncasecmp (line_utf8, "BEGIN:VCARD", 11) != 0) {
		encoding = VCARD_ENCODING_NONE;
	}

	g_free (line_utf8);
	return encoding;
}

static void
primary_selection_changed_cb (ESourceSelector *selector, EImportTarget *target)
{
	g_datalist_set_data_full(&target->data, "vcard-source",
				 g_object_ref(e_source_selector_peek_primary_selection(selector)),
				 g_object_unref);
}

static GtkWidget *
vcard_getwidget(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	GtkWidget *vbox, *selector;
	ESource *primary;
	ESourceList *source_list;	

	/* FIXME Better error handling */
	if (!e_book_get_addressbooks (&source_list, NULL))
		return NULL;

	vbox = gtk_vbox_new (FALSE, FALSE);

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, TRUE, 6);
	
	primary = g_datalist_get_data(&target->data, "vcard-source");
	if (primary == NULL) {
		primary = e_source_list_peek_source_any (source_list);
		g_object_ref(primary);
		g_datalist_set_data_full(&target->data, "vcard-source", primary, g_object_unref);
	}
	e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);
	g_object_unref (source_list);

	g_signal_connect (selector, "primary_selection_changed", G_CALLBACK (primary_selection_changed_cb), target);

	gtk_widget_show_all (vbox);

	return vbox;
}

static gboolean
vcard_supported(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *)target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp(s->uri_src, "file:///", 8) != 0)
		return FALSE;

	/* FIXME: need to parse the url properly */
	return guess_vcard_encoding(s->uri_src+7) != VCARD_ENCODING_NONE;
}

static void
vcard_import_done(VCardImporter *gci)
{
	if (gci->idle_id)
		g_source_remove(gci->idle_id);

	g_object_unref (gci->book);
	g_list_foreach (gci->contactlist, (GFunc) g_object_unref, NULL);
	g_list_free (gci->contactlist);

	e_import_complete(gci->import, gci->target);
	g_object_unref(gci->import);
	g_free (gci);
}

static void
vcard_import(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	VCardImporter *gci;
	char *contents;
	VCardEncoding encoding;
	EBook *book;
	EImportTargetURI *s = (EImportTargetURI *)target;

	/* FIXME: get filename properly */
	encoding = guess_vcard_encoding(s->uri_src+7);
	if (encoding == VCARD_ENCODING_NONE) {
		/* this check is superfluous, we've already checked otherwise we can't get here ... */
		e_import_complete(ei, target);
		return;
	}

	book = e_book_new(g_datalist_get_data(&target->data, "vcard-source"), NULL);
	if (book == NULL) {
		g_message(G_STRLOC ":Couldn't create EBook.");
		e_import_complete(ei, target);
		return;
	}

	if (!e_book_is_writable (book)) {
		g_message (G_STRLOC ":Book is readonly.");
		e_import_complete(ei, target);
		g_object_unref(book);
		return;
	}

	if (!g_file_get_contents (s->uri_src+7, &contents, NULL, NULL)) {
		g_message (G_STRLOC ":Couldn't read file.");
		e_import_complete(ei, target);
		g_object_unref(book);
		return;
	}

	gci = g_malloc0(sizeof(*gci));
	g_datalist_set_data(&target->data, "vcard-data", gci);
	gci->import = g_object_ref(ei);
	gci->target = target;
	gci->book = book;

	e_book_open (gci->book, TRUE, NULL);

	if (encoding == VCARD_ENCODING_UTF16) {
		gchar *tmp;

		gunichar2 *contents_utf16 = (gunichar2*)contents;
		tmp = utf16_to_utf8 (contents_utf16);
		g_free (contents);
		contents = tmp;
	} else if (encoding == VCARD_ENCODING_LOCALE) {
		gchar *tmp;
		tmp = g_locale_to_utf8 (contents, -1, NULL, NULL, NULL);
		g_free (contents);
		contents = tmp;
	}

	gci->contactlist = eab_contact_list_from_string (contents);
	g_free (contents);
	gci->iterator = gci->contactlist;
	gci->total = g_list_length(gci->contactlist);

	if (gci->iterator)
		gci->idle_id = g_idle_add(vcard_import_contacts, gci);
	else
		vcard_import_done(gci);
}
					   
static void
vcard_cancel(EImport *ei, EImportTarget *target, EImportImporter *im)
{
	VCardImporter *gci = g_datalist_get_data(&target->data, "vcard-data");

	if (gci)
		gci->state = 1;
}

static EImportImporter vcard_importer = {
	E_IMPORT_TARGET_URI,
	0,
	vcard_supported,
	vcard_getwidget,
	vcard_import,
	vcard_cancel,
};

EImportImporter *
evolution_vcard_importer_peek(void)
{
	vcard_importer.name = _("VCard (.vcf, .gcrd)");
	vcard_importer.description = _("Evolution VCard Importer");

	return &vcard_importer;
}
