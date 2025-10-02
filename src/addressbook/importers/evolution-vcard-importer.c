/*
 * Evolution calendar importer component
 *
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
 *		Chris Toshok <toshok@ximian.com>
 *      JP Rosevear <jpr@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/libebook.h>

#include <util/eab-book-util.h>

#include <shell/e-shell.h>

#include "evolution-addressbook-importers.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	gint state;		/* 0 - importing, 1 - cancelled/complete */
	gint total;
	gint count;

	ESource *primary;

	GSList *contactlist;
	GSList *iterator;
	EBookClient *book_client;

	/* when opening book */
	gchar *contents;
} VCardImporter;

static void vcard_import_done (VCardImporter *gci);

static void
vcard_import_contact (VCardImporter *gci,
                      EContact *contact)
{
	EContactPhoto *photo;
	GList *attrs, *attr;
	gchar *uid = NULL;

	/* Apple's addressbook.app exports PHOTO's without a TYPE
	 * param, so let's figure out the format here if there's a
	 * PHOTO attribute missing a TYPE param.
	 *
	 * this is sort of a hack, as EContact sets the type for us if
	 * we use the setter.  so let's e_contact_get + e_contact_set
	 * on E_CONTACT_PHOTO.
	*/
	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (photo) {
		e_contact_set (contact, E_CONTACT_PHOTO, photo);
		e_contact_photo_free (photo);
	}

	/* Deal with our XML EDestination stuff in EMAIL attributes, if there is any. */
	attrs = e_vcard_get_attributes_by_name (E_VCARD (contact), EVC_EMAIL);
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		GList *v = e_vcard_attribute_get_values (a);

		if (v && v->data) {
			if (!strncmp ((gchar *) v->data, "<?xml", 5)) {
				EDestination *dest = e_destination_import ((gchar *) v->data);

				e_destination_export_to_vcard_attribute (dest, a);

				g_object_unref (dest);
			}
		}
	}
	g_list_free (attrs);

	/* Deal with TEL attributes that don't conform to what we need.
	 *
	 * 1. if there's no location (HOME/WORK/OTHER), default to OTHER.
	 * 2. if there's *only* a location specified, default to VOICE.
	 */
	attrs = e_vcard_get_attributes (E_VCARD (contact));
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		gboolean location_only = TRUE;
		gboolean no_location = TRUE;
		gboolean is_work_home = FALSE;
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
				is_work_home = is_work_home ||
					!g_ascii_strcasecmp ((gchar *) v->data, "WORK") ||
					!g_ascii_strcasecmp ((gchar *) v->data, "HOME");

				if (!g_ascii_strcasecmp ((gchar *) v->data, "WORK") ||
				    !g_ascii_strcasecmp ((gchar *) v->data, "HOME") ||
				    !g_ascii_strcasecmp ((gchar *) v->data, "OTHER"))
					no_location = FALSE;
				else
					location_only = FALSE;
			}
		}

		if (is_work_home) {
			/* only WORK and HOME phone numbers require locations,
			 * the rest should be kept as is */
			if (location_only) {
				/* add VOICE */
				e_vcard_attribute_add_param_with_value (
					a,
					e_vcard_attribute_param_new (EVC_TYPE),
					"VOICE");
			}
			if (no_location) {
				/* add OTHER */
				e_vcard_attribute_add_param_with_value (
					a,
					e_vcard_attribute_param_new (EVC_TYPE),
					"OTHER");
			}
		}
	}

	/* Deal with ADR and EMAIL attributes that don't conform to what
	 * we need.  If HOME or WORK isn't specified, add TYPE=OTHER. */
	attrs = e_vcard_get_attributes (E_VCARD (contact));
	for (attr = attrs; attr; attr = attr->next) {
		EVCardAttribute *a = attr->data;
		gboolean no_location = TRUE;
		GList *params, *param;

		if (g_ascii_strcasecmp (e_vcard_attribute_get_name (a), EVC_ADR) &&
		    g_ascii_strcasecmp (e_vcard_attribute_get_name (a), EVC_EMAIL))
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
				if (!g_ascii_strcasecmp ((gchar *) v->data, "WORK") ||
				    !g_ascii_strcasecmp ((gchar *) v->data, "HOME"))
					no_location = FALSE;
			}
		}

		if (no_location) {
			/* add OTHER */
			e_vcard_attribute_add_param_with_value (
				a,
								e_vcard_attribute_param_new (EVC_TYPE),
								"OTHER");
		}
	}

	/* FIXME Error checking */
	e_book_client_add_contact_sync (
		gci->book_client, contact, E_BOOK_OPERATION_FLAG_NONE, &uid, NULL, NULL);
	if (uid != NULL) {
		e_contact_set (contact, E_CONTACT_UID, uid);
		g_free (uid);
	}
}

static gboolean
vcard_import_contacts (gpointer data)
{
	VCardImporter *gci = data;
	gint count = 0;
	GSList *iterator = gci->iterator;

	if (gci->state == 0) {
		while (count < 50 && iterator) {
			vcard_import_contact (gci, iterator->data);
			count++;
			iterator = iterator->next;
		}
		gci->count += count;
		gci->iterator = iterator;
		if (iterator == NULL)
			gci->state = 1;
	}
	if (gci->state == 1) {
		vcard_import_done (gci);
		return FALSE;
	} else {
		e_import_status (
			gci->import, gci->target, _("Importing…"),
			gci->count * 100 / gci->total);
		return TRUE;
	}
}

static void
primary_selection_changed_cb (ESourceSelector *selector,
                              EImportTarget *target)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	g_datalist_set_data_full (
		&target->data, "vcard-source",
		source, (GDestroyNotify) g_object_unref);
}

static GtkWidget *
vcard_getwidget (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	EShell *shell;
	GtkWidget *vbox, *selector, *scrolled_window;
	ESourceRegistry *registry;
	ESource *primary;
	const gchar *extension_name;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (scrolled_window),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 6);

	selector = e_source_selector_new (registry, extension_name);
	e_source_selector_set_show_toggles (
		E_SOURCE_SELECTOR (selector), FALSE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);

	primary = g_datalist_get_data (&target->data, "vcard-source");
	if (primary == NULL) {
		GList *list;

		list = e_source_registry_list_sources (registry, extension_name);
		if (list != NULL) {
			primary = g_object_ref (list->data);
			g_datalist_set_data_full (
				&target->data, "vcard-source", primary,
				(GDestroyNotify) g_object_unref);
		}

		g_list_free_full (list, (GDestroyNotify) g_object_unref);
	}
	e_source_selector_set_primary_selection (
		E_SOURCE_SELECTOR (selector), primary);

	g_signal_connect (
		selector, "primary_selection_changed",
		G_CALLBACK (primary_selection_changed_cb), target);

	gtk_widget_show_all (vbox);

	return vbox;
}

static gboolean
vcard_supported (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	EImportTargetURI *s;
	gchar *filename, *contents;
	gboolean retval;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *) target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp (s->uri_src, "file:///", 8) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL)
		return FALSE;
	contents = e_import_util_get_file_contents (filename, 32, NULL);
	retval = contents && g_ascii_strncasecmp (contents, "BEGIN:VCARD", 11) == 0;
	g_free (contents);
	g_free (filename);

	return retval;
}

static void
vcard_import_done (VCardImporter *gci)
{
	if (gci->idle_id)
		g_source_remove (gci->idle_id);

	g_free (gci->contents);
	g_object_unref (gci->book_client);
	g_slist_free_full (gci->contactlist, (GDestroyNotify) g_object_unref);

	e_import_complete (gci->import, gci->target, NULL);
	g_object_unref (gci->import);
	g_free (gci);
}

static void
book_client_connect_cb (GObject *source_object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	VCardImporter *gci = user_data;
	EClient *client;

	client = e_book_client_connect_finish (result, NULL);

	if (client == NULL) {
		vcard_import_done (gci);
		return;
	}

	gci->book_client = E_BOOK_CLIENT (client);
	gci->contactlist = eab_contact_list_from_string (gci->contents);
	g_free (gci->contents);
	gci->contents = NULL;
	gci->iterator = gci->contactlist;
	gci->total = g_slist_length (gci->contactlist);

	if (gci->iterator)
		gci->idle_id = g_idle_add (vcard_import_contacts, gci);
	else
		vcard_import_done (gci);
}

static void
vcard_import (EImport *ei,
              EImportTarget *target,
              EImportImporter *im)
{
	VCardImporter *gci;
	ESource *source;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	gchar *contents;
	GError *error = NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, &error);
	if (filename == NULL) {
		e_import_complete (ei, target, error);
		g_clear_error (&error);

		return;
	}

	contents = e_import_util_get_file_contents (filename, 0, &error);
	if (!contents) {
		g_free (filename);
		e_import_complete (ei, target, error);
		g_clear_error (&error);

		return;
	}

	g_free (filename);
	gci = g_malloc0 (sizeof (*gci));
	g_datalist_set_data (&target->data, "vcard-data", gci);
	gci->import = g_object_ref (ei);
	gci->target = target;
	gci->contents = contents;

	source = g_datalist_get_data (&target->data, "vcard-source");

	e_book_client_connect (source, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, book_client_connect_cb, gci);
}

static void
vcard_cancel (EImport *ei,
              EImportTarget *target,
              EImportImporter *im)
{
	VCardImporter *gci = g_datalist_get_data (&target->data, "vcard-data");

	if (gci)
		gci->state = 1;
}

static GtkWidget *
vcard_get_preview (EImport *ei,
                   EImportTarget *target,
                   EImportImporter *im)
{
	GtkWidget *preview;
	GSList *contacts;
	gchar *contents;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	GError *error = NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, &error);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s': %s", s->uri_src, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return NULL;
	}

	contents = e_import_util_get_file_contents (filename, 128 * 1024, &error);
	if (!contents) {
		g_message (G_STRLOC ": Couldn't read file '%s': %s", filename, error ? error->message : "Unknown error");
		g_clear_error (&error);
		g_free (filename);
		return NULL;
	}

	g_free (filename);

	contacts = eab_contact_list_from_string (contents);
	g_free (contents);

	preview = evolution_contact_importer_get_preview_widget (contacts);

	g_slist_free_full (contacts, (GDestroyNotify) g_object_unref);

	return preview;
}

static EImportImporter vcard_importer = {
	E_IMPORT_TARGET_URI,
	0,
	vcard_supported,
	vcard_getwidget,
	vcard_import,
	vcard_cancel,
	vcard_get_preview,
};

EImportImporter *
evolution_vcard_importer_peek (void)
{
	vcard_importer.name = _("vCard (.vcf, .gcrd)");
	vcard_importer.description = _("Evolution vCard Importer");

	return &vcard_importer;
}

/* utility functions shared between all contact importers */
static void
preview_contact (EWebViewPreview *preview,
                 EContact *contact)
{
	gint idx;
	gboolean had_value = FALSE;

	const gint fields[] = {
		E_CONTACT_FILE_AS,
		E_CONTACT_CATEGORIES,

		E_CONTACT_IS_LIST,
		E_CONTACT_LIST_SHOW_ADDRESSES,
		E_CONTACT_WANTS_HTML,

		E_CONTACT_FULL_NAME,
		E_CONTACT_GIVEN_NAME,
		E_CONTACT_FAMILY_NAME,
		E_CONTACT_NICKNAME,
		E_CONTACT_SPOUSE,
		E_CONTACT_BIRTH_DATE,
		E_CONTACT_ANNIVERSARY,
		E_CONTACT_MAILER,
		E_CONTACT_EMAIL,

		-1,

		E_CONTACT_ORG,
		E_CONTACT_ORG_UNIT,
		E_CONTACT_OFFICE,
		E_CONTACT_TITLE,
		E_CONTACT_ROLE,
		E_CONTACT_MANAGER,
		E_CONTACT_ASSISTANT,

		-1,

		E_CONTACT_PHONE_ASSISTANT,
		E_CONTACT_PHONE_BUSINESS,
		E_CONTACT_PHONE_BUSINESS_2,
		E_CONTACT_PHONE_BUSINESS_FAX,
		E_CONTACT_PHONE_CALLBACK,
		E_CONTACT_PHONE_CAR,
		E_CONTACT_PHONE_COMPANY,
		E_CONTACT_PHONE_HOME,
		E_CONTACT_PHONE_HOME_2,
		E_CONTACT_PHONE_HOME_FAX,
		E_CONTACT_PHONE_ISDN,
		E_CONTACT_PHONE_MOBILE,
		E_CONTACT_PHONE_OTHER,
		E_CONTACT_PHONE_OTHER_FAX,
		E_CONTACT_PHONE_PAGER,
		E_CONTACT_PHONE_PRIMARY,
		E_CONTACT_PHONE_RADIO,
		E_CONTACT_PHONE_TELEX,
		E_CONTACT_PHONE_TTYTDD,

		-1,

		E_CONTACT_ADDRESS_HOME,
		E_CONTACT_ADDRESS_WORK,
		E_CONTACT_ADDRESS_OTHER,

		-1,

		E_CONTACT_HOMEPAGE_URL,
		E_CONTACT_BLOG_URL,
		E_CONTACT_CALENDAR_URI,
		E_CONTACT_FREEBUSY_URL,
		E_CONTACT_ICS_CALENDAR,
		E_CONTACT_VIDEO_URL,

		-1,

		E_CONTACT_IMPP,

		-1,

		E_CONTACT_NOTE
	};

	g_return_if_fail (preview != NULL);
	g_return_if_fail (contact != NULL);

	for (idx = 0; idx < G_N_ELEMENTS (fields); idx++) {
		EContactField field;

		if (fields[idx] == -1) {
			if (had_value)
				e_web_view_preview_add_empty_line (preview);
			had_value = FALSE;
			continue;
		}

		field = fields[idx];

		if (field == E_CONTACT_BIRTH_DATE || field == E_CONTACT_ANNIVERSARY) {
			EContactDate *dt = e_contact_get (contact, field);
			if (dt) {
				GDate gd = { 0 };
				struct tm tm;
				gchar *value;

				g_date_set_dmy (&gd, dt->day, dt->month, dt->year);
				g_date_to_struct_tm (&gd, &tm);

				value = e_datetime_format_format_tm (
					"addressbook", "table",
					DTFormatKindDate, &tm);
				if (value) {
					e_web_view_preview_add_section (
						preview,
						e_contact_pretty_name (field),
						value);
					had_value = TRUE;
				}

				g_free (value);
				e_contact_date_free (dt);
			}
		} else if (field == E_CONTACT_IS_LIST ||
			   field == E_CONTACT_WANTS_HTML ||
			   field == E_CONTACT_LIST_SHOW_ADDRESSES) {
			if (e_contact_get (contact, field)) {
				e_web_view_preview_add_text (
					preview, e_contact_pretty_name (field));
				had_value = TRUE;
			}
		} else if (field == E_CONTACT_ADDRESS_HOME ||
			   field == E_CONTACT_ADDRESS_WORK ||
			   field == E_CONTACT_ADDRESS_OTHER) {
			EContactAddress *addr = e_contact_get (contact, field);
			if (addr) {
				gboolean have = FALSE;

				#define add_it(_what) \
					if (addr->_what && *addr->_what) { \
						e_web_view_preview_add_section ( \
							preview, have ? NULL : \
							e_contact_pretty_name (field), addr->_what); \
						have = TRUE; \
						had_value = TRUE; \
					}

				add_it (po);
				add_it (ext);
				add_it (street);
				add_it (locality);
				add_it (region);
				add_it (code);
				add_it (country);

				#undef add_it

				e_contact_address_free (addr);
			}
		} else if (field == E_CONTACT_EMAIL) {
			GList *attrs, *a;
			gboolean have = FALSE;
			const gchar *pretty_name;

			pretty_name = e_contact_pretty_name (field);

			attrs = e_vcard_get_attributes_by_name (E_VCARD (contact), EVC_EMAIL);
			for (a = attrs; a; a = a->next) {
				EVCardAttribute *attr = a->data;
				GList *value;

				if (!attr)
					continue;

				value = e_vcard_attribute_get_values (attr);

				while (value != NULL) {
					const gchar *str = value->data;

					if (str && *str) {
						e_web_view_preview_add_section (
							preview, have ? NULL :
							pretty_name, str);
						have = TRUE;
						had_value = TRUE;
					}

					value = value->next;
				}
			}

			g_list_free (attrs);

		} else if (field == E_CONTACT_IMPP) {
			GList *values, *link;
			gboolean have = FALSE;
			const gchar *pretty_name;

			pretty_name = e_contact_pretty_name (field);

			values = e_contact_get (contact, E_CONTACT_IMPP);
			for (link = values; link; link = g_list_next (link)) {
				const gchar *value = link->data;

				if (!value || !*value)
					continue;

				e_web_view_preview_add_section (preview, have ? NULL : pretty_name, value);
				have = TRUE;
				had_value = TRUE;
			}

			g_list_free_full (values, g_free);

		} else if (field == E_CONTACT_CATEGORIES) {
			const gchar *pretty_name;
			const gchar *value;

			pretty_name = e_contact_pretty_name (field);
			value = e_contact_get_const (contact, field);

			if (value != NULL && *value != '\0') {
				e_web_view_preview_add_section (
					preview, pretty_name, value);
				had_value = TRUE;
			}

		} else {
			const gchar *pretty_name;
			const gchar *value;

			pretty_name = e_contact_pretty_name (field);
			value = e_contact_get_const (contact, field);

			if (value != NULL && *value != '\0') {
				e_web_view_preview_add_section (
					preview, pretty_name, value);
				had_value = TRUE;
			}
		}
	}
}

static void
preview_selection_changed_cb (GtkTreeSelection *selection,
                              EWebViewPreview *preview)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (preview != NULL);

	e_web_view_preview_begin_update (preview);

	if (gtk_tree_selection_get_selected (selection, &model, &iter) && model) {
		EContact *contact = NULL;

		gtk_tree_model_get (model, &iter, 1, &contact, -1);

		if (contact) {
			preview_contact (preview, contact);
			g_object_unref (contact);
		}
	}

	e_web_view_preview_end_update (preview);
}

GtkWidget *
evolution_contact_importer_get_preview_widget (const GSList *contacts)
{
	GtkWidget *preview;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkTreeIter iter;
	const GSList *c;

	if (!contacts)
		return NULL;

	store = gtk_list_store_new (2, G_TYPE_STRING, E_TYPE_CONTACT);

	for (c = contacts; c; c = c->next) {
		const gchar *description;
		gchar *free_description = NULL;
		EContact *contact = (EContact *) c->data;

		if (!contact || !E_IS_CONTACT (contact))
			continue;

		description = e_contact_get_const (contact, E_CONTACT_FILE_AS);
		if (!description)
			description = e_contact_get_const (contact, E_CONTACT_UID);
		if (!description)
			description = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
		if (!description) {
			description = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
			if (description) {
				const gchar *at = strchr (description, '@');
				if (at) {
					free_description = g_strndup (
						description,
						(gsize) (at - description));
					description = free_description;
				}
			}
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, description ? description : "",
			1, contact,
			-1);

		g_free (free_description);
	}

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		g_object_unref (store);
		return NULL;
	}

	preview = e_web_view_preview_new ();
	gtk_widget_show (preview);

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (preview));
	g_return_val_if_fail (tree_view != NULL, NULL);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_tree_view_insert_column_with_attributes (
		tree_view, -1, _("Contact"),
		gtk_cell_renderer_text_new (), "text", 0, NULL);

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) > 1)
		e_web_view_preview_show_tree_view (E_WEB_VIEW_PREVIEW (preview));

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_select_iter (selection, &iter);
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (preview_selection_changed_cb), preview);

	preview_selection_changed_cb (selection, E_WEB_VIEW_PREVIEW (preview));

	return preview;
}
