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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtknotebook.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-control.h>

#include <libebook/e-book.h>
#include <libedataserverui/e-source-selector.h>

#include <importer/evolution-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <util/eab-book-util.h>
#include <util/e-destination.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_ImporterFactory:" BASE_VERSION
#define COMPONENT_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_Importer:" BASE_VERSION

typedef struct {
	ESource *primary;
	
	GList *contactlist;
	GList *iterator;
	EBook *book;
	gboolean ready;
} VCardImporter;

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

/* EvolutionImporter methods */
static void
process_item_fn (EvolutionImporter *importer,
		 CORBA_Object listener,
		 void *closure,
		 CORBA_Environment *ev)
{
	VCardImporter *gci = (VCardImporter *) closure;
	EContact *contact;
	EContactPhoto *photo;
	GList *attrs, *attr;

	if (gci->iterator == NULL)
		gci->iterator = gci->contactlist;
	
	if (gci->ready == FALSE) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_NOT_READY,
							       gci->iterator ? TRUE : FALSE, 
							       ev);
		return;
	}
	
	if (gci->iterator == NULL) {
		GNOME_Evolution_ImporterListener_notifyResult (listener,
							       GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION,
							       FALSE, ev);
		return;
	}
	
	contact = gci->iterator->data;

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
	
	gci->iterator = gci->iterator->next;
	
	GNOME_Evolution_ImporterListener_notifyResult (listener,
						       GNOME_Evolution_ImporterListener_OK,
						       gci->iterator ? TRUE : FALSE, 
						       ev);
	if (ev->_major != CORBA_NO_EXCEPTION) {
		g_warning ("Error notifying listeners.");
	}
	
	return;
}

static char *supported_extensions[3] = {
	".vcf",
	".gcrd",
	NULL
};

/* Actually check the contents of this file */
static gboolean
check_file_is_vcard (const char *filename)
{
	FILE *handle;
	char line[4096];
	gboolean result;

	handle = fopen (filename, "r");
	if (handle == NULL) {
		g_print ("\n");
		return FALSE;
	}
		
	fgets (line, 4096, handle);
	if (line == NULL) {
		fclose (handle);
		g_print ("\n");
		return FALSE;
	}

	if (g_ascii_strncasecmp (line, "BEGIN:VCARD", 11) == 0) {
		result = TRUE;
	} else {
		result = FALSE;
	}

	fclose (handle);
	return result;
}

static void
primary_selection_changed_cb (ESourceSelector *selector, gpointer data)
{
	VCardImporter *gci = data;

	if (gci->primary)
		g_object_unref (gci->primary);
	gci->primary = g_object_ref (e_source_selector_peek_primary_selection (selector));
}

static void
create_control_fn (EvolutionImporter *importer, Bonobo_Control *control, void *closure)
{
	VCardImporter *gci = closure;
	GtkWidget *vbox, *selector;
	ESource *primary;
	ESourceList *source_list;	
	
	vbox = gtk_vbox_new (FALSE, FALSE);
	
	/* FIXME Better error handling */
	if (!e_book_get_addressbooks (&source_list, NULL))
		return;		

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, TRUE, 6);
	
	/* FIXME What if no sources? */
	primary = e_source_list_peek_source_any (source_list);
	e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);
	if (!gci->primary)
		gci->primary = g_object_ref (primary);
	g_object_unref (source_list);
		
	g_signal_connect (G_OBJECT (selector), "primary_selection_changed",
			  G_CALLBACK (primary_selection_changed_cb), gci);

	gtk_widget_show_all (vbox);
	
	*control = BONOBO_OBJREF (bonobo_control_new (vbox));
}

static gboolean
support_format_fn (EvolutionImporter *importer,
		   const char *filename,
		   void *closure)
{
	char *ext;
	int i;

	ext = strrchr (filename, '.');
	if (ext == NULL) {
		return check_file_is_vcard (filename);
	}
	for (i = 0; supported_extensions[i] != NULL; i++) {
		if (g_ascii_strcasecmp (supported_extensions[i], ext) == 0)
			return check_file_is_vcard (filename);
	}

	return FALSE;
}

static void
importer_destroy_cb (gpointer data,
		     GObject *where_object_was)
{
	VCardImporter *gci = data;

	if (gci->primary)
		g_object_unref (gci->primary);
	
	if (gci->book)
		g_object_unref (gci->book);

	g_list_foreach (gci->contactlist, (GFunc) g_object_unref, NULL);
	g_list_free (gci->contactlist);

	g_free (gci);
}

static gboolean
load_file_fn (EvolutionImporter *importer,
	      const char *filename,
	      void *closure)
{
	VCardImporter *gci;
	char *contents;
	
	if (check_file_is_vcard (filename) == FALSE) {
		return FALSE;
	}

	gci = (VCardImporter *) closure;
	gci->contactlist = NULL;
	gci->iterator = NULL;
	gci->ready = FALSE;
	
	/* Load the book */
	gci->book = e_book_new (gci->primary, NULL);
	if (!gci->book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_open (gci->book, TRUE, NULL);

	/* Load the file and the contacts */
	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		g_message (G_STRLOC ":Couldn't read file.");
		return FALSE;
	}	
	gci->contactlist = eab_contact_list_from_string (contents);
	g_free (contents);

	gci->ready = TRUE;

	return TRUE;
}
					   
static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    const char *component_id,
	    void *closure)
{
	EvolutionImporter *importer;
	VCardImporter *gci;

	if (!strcmp (component_id, COMPONENT_IID)) {
		gci = g_new0 (VCardImporter, 1);
		importer = evolution_importer_new (create_control_fn, support_format_fn, 
						   load_file_fn, process_item_fn, NULL, gci);
	
		g_object_weak_ref (G_OBJECT (importer),
				   importer_destroy_cb, gci);
		return BONOBO_OBJECT (importer);
	}
	else {
		g_warning (COMPONENT_FACTORY_IID ": Don't know what to do with %s", component_id);
		return NULL;
	}
}

BONOBO_ACTIVATION_SHLIB_FACTORY (COMPONENT_FACTORY_IID, "Evolution VCard Importer Factory", factory_fn, NULL)
