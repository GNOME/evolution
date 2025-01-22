/*
 * evolution-cal-config-contacts.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <e-util/e-util.h>

#include "e-contacts-selector.h"

/* This file contains two extension classes: an ESourceConfigBackend
 * for "contacts" calendars and an EExtension for EBookSourceConfig. */

/**************************** ECalConfigContacts *****************************/

typedef ESourceConfigBackend ECalConfigContacts;
typedef ESourceConfigBackendClass ECalConfigContactsClass;

/* Forward Declarations */
GType e_cal_config_contacts_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigContacts,
	e_cal_config_contacts,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static gboolean
cal_config_contacts_allow_creation (ESourceConfigBackend *backend)
{
	return FALSE;
}

static void
cal_config_contacts_insert_widgets (ESourceConfigBackend *backend,
                                    ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceRegistry *registry;
	GtkWidget *container;
	GtkWidget *widget;

	config = e_source_config_backend_get_config (backend);
	registry = e_source_config_get_registry (config);


	widget = gtk_label_new (_("Choose which address books to use."));
	gtk_widget_set_margin_top (widget, 12);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_size_request (widget, 320, 200);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_contacts_selector_new (registry);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);
}

static void
e_cal_config_contacts_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "contacts-stub";
	class->backend_name = "contacts";
	class->allow_creation = cal_config_contacts_allow_creation;
	class->insert_widgets = cal_config_contacts_insert_widgets;
}

static void
e_cal_config_contacts_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_contacts_init (ESourceConfigBackend *backend)
{
}

/*************************** EBookConfigBirthdays ****************************/

/* Standard GObject macros */
#define E_TYPE_BOOK_CONFIG_BIRTHDAYS \
	(e_book_config_birthdays_get_type ())
#define E_BOOK_CONFIG_BIRTHDAYS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BOOK_CONFIG_BIRTHDAYS, EBookConfigBirthdays))

typedef struct _EBookConfigBirthdays EBookConfigBirthdays;
typedef struct _EBookConfigBirthdaysClass EBookConfigBirthdaysClass;

struct _EBookConfigBirthdays {
	EExtension parent;
	GtkWidget *button;
};

struct _EBookConfigBirthdaysClass {
	EExtensionClass parent_class;
};

/* Forward Declarations */
GType e_book_config_birthdays_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigBirthdays,
	e_book_config_birthdays,
	E_TYPE_EXTENSION)

static ESourceConfig *
book_config_birthdays_get_config (EBookConfigBirthdays *birthdays)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (birthdays));

	return E_SOURCE_CONFIG (extensible);
}

static void
book_config_birthdays_dispose (GObject *object)
{
	EBookConfigBirthdays *birthdays;

	birthdays = E_BOOK_CONFIG_BIRTHDAYS (object);

	g_clear_object (&birthdays->button);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_book_config_birthdays_parent_class)->dispose (object);
}

static void
book_config_birthdays_init_candidate (ESourceConfig *config,
                                      ESource *scratch_source,
                                      EBookConfigBirthdays *birthdays)
{
	ESourceExtension *extension;
	const gchar *extension_name;

	extension_name = E_SOURCE_EXTENSION_CONTACTS_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_property (
		extension, "include-me",
		birthdays->button, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static void
book_config_birthdays_constructed (GObject *object)
{
	ESourceConfig *config;
	EBookConfigBirthdays *birthdays;
	GtkWidget *widget;
	const gchar *label;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_book_config_birthdays_parent_class)->constructed (object);

	birthdays = E_BOOK_CONFIG_BIRTHDAYS (object);
	config = book_config_birthdays_get_config (birthdays);

	label = _("Use in Birthdays & Anniversaries calendar");
	widget = gtk_check_button_new_with_label (label);
	e_source_config_insert_widget (config, NULL, NULL, widget);
	birthdays->button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	g_signal_connect (
		config, "init-candidate",
		G_CALLBACK (book_config_birthdays_init_candidate),
		birthdays);
}

static void
e_book_config_birthdays_class_init (EBookConfigBirthdaysClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = book_config_birthdays_dispose;
	object_class->constructed = book_config_birthdays_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_BOOK_SOURCE_CONFIG;
}

static void
e_book_config_birthdays_class_finalize (EBookConfigBirthdaysClass *class)
{
}

static void
e_book_config_birthdays_init (EBookConfigBirthdays *birthdays)
{
}

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_contacts_selector_type_register (type_module);

	e_cal_config_contacts_register_type (type_module);
	e_book_config_birthdays_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
