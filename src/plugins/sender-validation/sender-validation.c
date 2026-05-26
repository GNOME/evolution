/*
 * SPDX-FileCopyrightText: (C) 2020 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gmodule.h>
#include <libebackend/libebackend.h>

#include <camel/camel.h>

#include "e-util/e-util.h"
#include "composer/e-msg-composer.h"
#include "gui/itip-utils.h"
#include "shell/e-shell.h"

#define RA_CONF_KEY_NAME "assignments"
#define AR_CONF_KEY_NAME "account-for-recipients"

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Standard GObject macros */
#define E_TYPE_SENDER_VALIDATION \
	(e_sender_validation_get_type ())
#define E_SENDER_VALIDATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SENDER_VALIDATION, ESenderValidation))
#define E_SENDER_VALIDATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SENDER_VALIDATION, ESenderValidationClass))
#define E_IS_SENDER_VALIDATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SENDER_VALIDATION))

typedef struct _ESenderValidation ESenderValidation;
typedef struct _ESenderValidationClass ESenderValidationClass;

struct _ESenderValidation {
	EExtension parent;
};

struct _ESenderValidationClass {
	EExtensionClass parent_class;
};

GType e_sender_validation_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (ESenderValidation, e_sender_validation, E_TYPE_EXTENSION)

typedef struct _Assignment {
	const gchar *recipient;
	const gchar *account;
} Assignment;

static void
e_sender_validation_free_assignment (gpointer ptr)
{
	if (!ptr)
		return;

	g_slice_free (Assignment, ptr);
}

/* (transfer full): Internal data uses strings from in_assignments */
static GSList *
e_sender_validation_parse_assignments (gchar **in_assignments)
{
	GSList *items = NULL;
	guint ii;

	if (!in_assignments || !*in_assignments)
		return NULL;

	for (ii = 0; in_assignments[ii]; ii++) {
		Assignment *assignment;
		gchar *value = in_assignments[ii];
		gchar *tab;

		tab = strchr (value, '\t');
		if (!tab || tab == value || !tab[1])
			continue;

		*tab = '\0';

		assignment = g_slice_new (Assignment);
		assignment->recipient = value;
		assignment->account = tab + 1;

		items = g_slist_prepend (items, assignment);
	}

	return g_slist_reverse (items);
}

static gboolean
e_sender_validation_ask_ra (GtkWindow *window,
			    const gchar *recipient,
			    const gchar *expected_account,
			    const gchar *used_account)
{
	gint response;

	response = e_alert_run_dialog_for_args (window,
		"org.gnome.evolution.plugins.sender-validation:sender-validation",
		recipient, expected_account, used_account,
		NULL);

	return response == GTK_RESPONSE_YES;
}

static gboolean
e_sender_validation_ask_ar (GtkWindow *window,
			    const gchar *recipient,
			    const gchar *expected_recipient,
			    const gchar *used_account)
{
	gint response;

	response = e_alert_run_dialog_for_args (window,
		"org.gnome.evolution.plugins.sender-validation:sender-validation-ar",
		recipient, expected_recipient, used_account,
		NULL);

	return response == GTK_RESPONSE_YES;
}

static gboolean
e_sender_validation_check (EMsgComposer *composer)
{
	GSettings *settings;
	GSList *assignments; /* Assignment * */
	gchar **strv;
	gboolean can_send = TRUE;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.sender-validation");

	strv = g_settings_get_strv (settings, RA_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	if (assignments) {
		EComposerHeaderTable *header_table;
		const gchar *from_address;

		header_table = e_msg_composer_get_header_table (composer);
		from_address = e_composer_header_table_get_from_address (header_table);

		if (from_address && *from_address) {
			EDestination **destinations;
			guint ii;

			destinations = e_composer_header_table_get_destinations (header_table);

			for (ii = 0; destinations && destinations[ii]; ii++) {
				EDestination *dest = destinations[ii];
				const gchar *recipient;

				recipient = e_destination_get_address (dest);

				if (recipient && *recipient) {
					const Assignment *has_mismatch = NULL;
					gboolean has_match = FALSE;
					GSList *link;

					for (link = assignments; link && !has_match; link = g_slist_next (link)) {
						const Assignment *assignment = link->data;

						if (camel_strstrcase (recipient, assignment->recipient)) {
							if (camel_strstrcase (from_address, assignment->account)) {
								has_match = TRUE;
							} else if (!has_mismatch) {
								has_mismatch = assignment;
							}
						}
					}

					if (!has_match && has_mismatch) {
						can_send = e_sender_validation_ask_ra (GTK_WINDOW (composer), recipient, has_mismatch->account, from_address);
						break;
					}
				}
			}

			e_destination_freev (destinations);
		}
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	/* Can free 'strv' only after 'assignments', because 'assignments' is using the memory from 'strv' */
	g_strfreev (strv);

	if (!can_send) {
		g_clear_object (&settings);
		return can_send;
	}

	strv = g_settings_get_strv (settings, AR_CONF_KEY_NAME);
	assignments = e_sender_validation_parse_assignments (strv);

	if (assignments) {
		EComposerHeaderTable *header_table;
		const gchar *from_address;

		header_table = e_msg_composer_get_header_table (composer);
		from_address = e_composer_header_table_get_from_address (header_table);

		if (from_address && *from_address) {
			GSList *link, *usable_assignments = NULL;

			for (link = assignments; link; link = g_slist_next (link)) {
				const Assignment *assignment = link->data;

				/* one account can be in the list multiple times */
				if (camel_strstrcase (from_address, assignment->account))
					usable_assignments = g_slist_prepend (usable_assignments, (gpointer) assignment);
			}

			usable_assignments = g_slist_reverse (usable_assignments);

			if (usable_assignments) {
				EDestination **destinations;
				guint ii;

				destinations = e_composer_header_table_get_destinations (header_table);

				for (ii = 0; destinations && destinations[ii]; ii++) {
					EDestination *dest = destinations[ii];
					const gchar *recipient;

					recipient = e_destination_get_address (dest);

					if (recipient && *recipient) {
						const Assignment *has_mismatch = NULL;
						gboolean has_match = FALSE;

						for (link = usable_assignments; link && !has_match; link = g_slist_next (link)) {
							const Assignment *assignment = link->data;

							if (camel_strstrcase (recipient, assignment->recipient))
								has_match = TRUE;
							else if (!has_mismatch)
								has_mismatch = assignment;
						}

						if (!has_match && has_mismatch) {
							can_send = e_sender_validation_ask_ar (GTK_WINDOW (composer), recipient, has_mismatch->recipient, from_address);
							break;
						}
					}
				}

				e_destination_freev (destinations);
			}
		}
	}

	g_slist_free_full (assignments, e_sender_validation_free_assignment);
	/* Can free 'strv' only after 'assignments', because 'assignments' is using the memory from 'strv' */
	g_strfreev (strv);
	g_clear_object (&settings);

	return can_send;
}

static gboolean
sender_validation_presend_cb (EMsgComposer *composer,
                              gpointer user_data)
{
	return e_sender_validation_check (composer);
}

static void
sender_validation_constructed (GObject *object)
{
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_sender_validation_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	g_signal_connect (
		extensible, "presend",
		G_CALLBACK (sender_validation_presend_cb), NULL);
}

static void
e_sender_validation_class_init (ESenderValidationClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = sender_validation_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_sender_validation_class_finalize (ESenderValidationClass *class)
{
}

static void
e_sender_validation_init (ESenderValidation *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_sender_validation_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
