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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <glib/gi18n.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <gtk/gtk.h>

#include <libecal/libecal.h>
#include <libical/icalvcal.h>
#include <libical/vcc.h>

#include "shell/e-shell.h"

#include "evolution-calendar-importer.h"
#include "gui/calendar-config-keys.h"

typedef struct {
	EImport *import;
	EImportTarget *target;

	guint idle_id;

	ECalClient *cal_client;
	ECalClientSourceType source_type;

	ICalComponent *icomp;

	GCancellable *cancellable;
} ICalImporter;

typedef struct {
	EImport *ei;
	EImportTarget *target;
	GList *tasks;
	ICalComponent *icomp;
	GCancellable *cancellable;
} ICalIntelligentImporter;

static const gint import_type_map[] = {
	E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
	E_CAL_CLIENT_SOURCE_TYPE_TASKS,
	-1
};

static const gchar *import_type_strings[] = {
	N_("Appointments and Meetings"),
	N_("Tasks"),
	NULL
};

/*
 * Functions shared by iCalendar & vCalendar importer.
 */

static GtkWidget *ical_get_preview (ICalComponent *icomp);

static gboolean
is_icomp_usable (ICalComponent *icomp,
		 const gchar *contents)
{
	ICalComponent *vevent, *vtodo;
	gboolean usable;

	/* components can be somewhere in the middle of a MIME message, thus check
	   the `contents`, if provided, begins with expected iCalendar strings */
	if (contents &&
	    g_ascii_strncasecmp (contents, "BEGIN:VCALENDAR", strlen ("BEGIN:VCALENDAR")) != 0 &&
	    g_ascii_strncasecmp (contents, "BEGIN:VEVENT", strlen ("BEGIN:VEVENT")) != 0 &&
	    g_ascii_strncasecmp (contents, "BEGIN:VTODO", strlen ("BEGIN:VTODO")) != 0)
		return FALSE;

	if (contents)
		return TRUE;

	if (!icomp || !i_cal_component_is_valid (icomp))
		return FALSE;

	vevent = i_cal_component_get_first_component (icomp, I_CAL_VEVENT_COMPONENT);
	vtodo = i_cal_component_get_first_component (icomp, I_CAL_VTODO_COMPONENT);

	usable = vevent || vtodo;

	g_clear_object (&vevent);
	g_clear_object (&vtodo);

	return usable;
}

static void
ivcal_import_done (ICalImporter *ici,
		   const GError *error)
{
	g_clear_object (&ici->cal_client);
	g_clear_object (&ici->icomp);

	e_import_complete (ici->import, ici->target, error);
	g_object_unref (ici->import);
	g_object_unref (ici->cancellable);
	g_free (ici);
}

/* This removes all components except VEVENTs and VTIMEZONEs from the toplevel */
static void
prepare_events (ICalComponent *icomp,
                GList **vtodos) /* ICalComponent * */
{
	ICalComponent *subcomp;
	ICalCompIter *iter;

	if (vtodos)
		*vtodos = NULL;

	iter = i_cal_component_begin_component (icomp, I_CAL_ANY_COMPONENT);
	subcomp = i_cal_comp_iter_deref (iter);
	while (subcomp) {
		ICalComponent *next_subcomp;
		ICalComponentKind child_kind = i_cal_component_isa (subcomp);

		next_subcomp = i_cal_comp_iter_next (iter);

		if (child_kind != I_CAL_VEVENT_COMPONENT &&
		    child_kind != I_CAL_VTIMEZONE_COMPONENT) {
			i_cal_component_remove_component (icomp, subcomp);
			if (child_kind == I_CAL_VTODO_COMPONENT && vtodos)
				*vtodos = g_list_prepend (*vtodos, g_object_ref (subcomp));
		}

		g_clear_object (&subcomp);
		subcomp = next_subcomp;
	}

	g_clear_object (&iter);
}

/* This removes all components except VTODOs and VTIMEZONEs from the toplevel
 * ICalComponent, and adds the given list of VTODO components. The list is
 * freed afterwards. */
static void
prepare_tasks (ICalComponent *icomp,
	       GList *vtodos)
{
	ICalComponent *subcomp;
	ICalCompIter *iter;
	GList *elem;

	iter = i_cal_component_begin_component (icomp, I_CAL_ANY_COMPONENT);
	subcomp = i_cal_comp_iter_deref (iter);
	while (subcomp) {
		ICalComponent *next_subcomp;
		ICalComponentKind child_kind = i_cal_component_isa (subcomp);

		next_subcomp = i_cal_comp_iter_next (iter);

		if (child_kind != I_CAL_VTODO_COMPONENT &&
		    child_kind != I_CAL_VTIMEZONE_COMPONENT) {
			i_cal_component_remove_component (icomp, subcomp);
		}

		g_clear_object (&subcomp);
		subcomp = next_subcomp;
	}

	g_clear_object (&iter);

	for (elem = vtodos; elem; elem = elem->next) {
		i_cal_component_take_component (icomp, elem->data);
	}

	g_list_free (vtodos);
}

struct UpdateObjectsData
{
	void (*done_cb) (gpointer user_data, const GError *error);
	gpointer user_data;
};

static void
receive_objects_ready_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	ECalClient *cal_client = E_CAL_CLIENT (source_object);
	struct UpdateObjectsData *uod = user_data;
	GError *error = NULL;

	g_return_if_fail (uod != NULL);

	e_cal_client_receive_objects_finish (cal_client, result, &error);

	if (uod->done_cb)
		uod->done_cb (uod->user_data, error);
	g_clear_error (&error);

	g_free (uod);
}

static void
update_objects (ECalClient *cal_client,
                ICalComponent *icomp,
                GCancellable *cancellable,
                void (*done_cb) (gpointer user_data, const GError *error),
                gpointer user_data)
{
	ICalComponentKind kind;
	ICalComponent *vcal;
	struct UpdateObjectsData *uod;

	kind = i_cal_component_isa (icomp);
	if (kind == I_CAL_VTODO_COMPONENT || kind == I_CAL_VEVENT_COMPONENT) {
		vcal = e_cal_util_new_top_level ();
		if (i_cal_component_get_method (icomp) == I_CAL_METHOD_CANCEL)
			i_cal_component_set_method (vcal, I_CAL_METHOD_CANCEL);
		else
			i_cal_component_set_method (vcal, I_CAL_METHOD_PUBLISH);
		i_cal_component_take_component (vcal, i_cal_component_clone (icomp));
	} else if (kind == I_CAL_VCALENDAR_COMPONENT) {
		vcal = i_cal_component_clone (icomp);
		if (!e_cal_util_component_has_property (vcal, I_CAL_METHOD_PROPERTY))
			i_cal_component_set_method (vcal, I_CAL_METHOD_PUBLISH);
	} else {
		if (done_cb)
			done_cb (user_data, NULL);
		return;
	}

	uod = g_new0 (struct UpdateObjectsData, 1);
	uod->done_cb = done_cb;
	uod->user_data = user_data;

	e_cal_client_receive_objects (cal_client, vcal, E_CAL_OPERATION_FLAG_NONE, cancellable, receive_objects_ready_cb, uod);

	g_object_unref (vcal);

	return;
}

struct _selector_data {
	EImportTarget *target;
	GtkWidget *selector;
	GtkWidget *notebook;
	gint page;
};

static void
button_toggled_cb (GtkWidget *widget,
                   struct _selector_data *sd)
{
	ESourceSelector *selector;
	ESource *source;
	GtkNotebook *notebook;

	selector = E_SOURCE_SELECTOR (sd->selector);
	source = e_source_selector_ref_primary_selection (selector);

	e_import_set_widget_complete (sd->target->import, source != NULL);

	if (source) {
		g_datalist_set_data_full (
			&sd->target->data, "primary-source",
			source, (GDestroyNotify) g_object_unref);
	}

	g_datalist_set_data (
		&sd->target->data, "primary-type",
		GINT_TO_POINTER (import_type_map[sd->page]));

	notebook = GTK_NOTEBOOK (sd->notebook);
	gtk_notebook_set_current_page (notebook, sd->page);
}

static void
primary_selection_changed_cb (ESourceSelector *selector,
                              EImportTarget *target)
{
	ESource *source;

	source = e_source_selector_ref_primary_selection (selector);

	e_import_set_widget_complete (target->import, source != NULL);

	if (source) {
		g_datalist_set_data_full (
			&target->data, "primary-source",
			source, (GDestroyNotify) g_object_unref);
	}
}

static void
create_calendar_clicked_cb (GtkWidget *button,
			    ESourceSelector *selector)
{
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GtkWindow *window;

	toplevel = gtk_widget_get_toplevel (button);
	if (!GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	registry = e_shell_get_registry (e_shell_get_default ());
	source_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "source-type"));
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));
	window = GTK_WINDOW (dialog);

	if (toplevel)
		gtk_window_set_transient_for (window, GTK_WINDOW (toplevel));

	if (source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
		gtk_window_set_icon_name (window, "x-office-calendar");
		gtk_window_set_title (window, _("New Calendar"));
	} else {
		gtk_window_set_icon_name (window, "stock_todo");
		gtk_window_set_title (window, _("New Task List"));
	}

	gtk_widget_show (dialog);
}

static gboolean
ivcal_source_selector_filter_source_readonly_cb (ESourceSelector *selector,
						 ESource *source,
						 gpointer user_data)
{
	GHashTable *known_readonly = user_data;
	gboolean hidden = FALSE;

	if (E_IS_SOURCE (source)) {
		if ((e_source_get_uid (source) && g_hash_table_contains (known_readonly, e_source_get_uid (source))) ||
		    (e_source_get_parent (source) && g_hash_table_contains (known_readonly, e_source_get_parent (source)))) {
			hidden = TRUE;
		} else {
			const gchar *ext_name = e_source_selector_get_extension_name (selector);

			if (e_source_has_extension (source, ext_name)) {
				gpointer extension = e_source_get_extension (source, ext_name);

				if (E_IS_SOURCE_BACKEND (extension)) {
					ESourceBackend *backend = E_SOURCE_BACKEND (extension);

					if (e_source_backend_get_backend_name (backend) &&
					    g_hash_table_contains (known_readonly, e_source_backend_get_backend_name (backend))) {
						hidden = TRUE;
					}
				}
			}
		}
	}

	return hidden;
}

static GHashTable *
ivcal_new_known_readonly_hash_table (void)
{
	GHashTable *hash_table;
	const gchar *known_readonly[] = { "webcal-stub", "weather-stub", "contacts-stub",
		"webcal", "weather", "contacts", "birthdays" };
	guint ii;

	hash_table = g_hash_table_new (g_str_hash, g_str_equal);

	for (ii = 0; ii < G_N_ELEMENTS (known_readonly); ii++) {
		g_hash_table_add (hash_table, (gpointer) known_readonly[ii]);
	}

	return hash_table;
}

static GtkWidget *
ivcal_getwidget (EImport *ei,
                 EImportTarget *target,
                 EImportImporter *im)
{
	EShell *shell;
	ESourceRegistry *registry;
	GtkWidget *top_vbox, *hbox, *first = NULL;
	GHashTable *known_readonly;
	GSList *group = NULL;
	gint i;
	GtkWidget *nb;

	known_readonly = ivcal_new_known_readonly_hash_table ();

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	top_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, TRUE, 6);

	nb = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (nb), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (nb), FALSE);
	gtk_box_pack_start (GTK_BOX (top_vbox), nb, TRUE, TRUE, 6);

	/* Type of iCalendar items */
	for (i = 0; import_type_map[i] != -1; i++) {
		GtkWidget *selector, *rb, *create_button, *vbox;
		GtkWidget *scrolled;
		struct _selector_data *sd;
		const gchar *extension_name;
		const gchar *create_label;

		switch (import_type_map[i]) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				extension_name = E_SOURCE_EXTENSION_CALENDAR;
				create_label = _("Cre_ate new calendar");
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				extension_name = E_SOURCE_EXTENSION_TASK_LIST;
				create_label = _("Cre_ate new task list");
				break;
			default:
				g_warn_if_reached ();
				continue;
		}

		selector = e_source_selector_new (registry, extension_name);
		/* flip the property to force rebuild of the model also when the "filter-source" signal is connected */
		e_source_selector_set_show_toggles (E_SOURCE_SELECTOR (selector), TRUE);
		g_signal_connect_data (selector, "filter-source",
			G_CALLBACK (ivcal_source_selector_filter_source_readonly_cb),
			g_hash_table_ref (known_readonly),
			(GClosureNotify) g_hash_table_unref, 0);
		e_source_selector_set_show_toggles (E_SOURCE_SELECTOR (selector), FALSE);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

		gtk_notebook_append_page (GTK_NOTEBOOK (nb), vbox, NULL);

		scrolled = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_container_add ((GtkContainer *) scrolled, selector);
		gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

		create_button = gtk_button_new_with_mnemonic (create_label);
		g_object_set_data (G_OBJECT (create_button), "source-type", GINT_TO_POINTER (import_type_map[i]));
		g_object_set (G_OBJECT (create_button),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_END,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_START,
			NULL);
		gtk_box_pack_start (GTK_BOX (vbox), create_button, FALSE, FALSE, 0);

		g_signal_connect (create_button, "clicked", G_CALLBACK (create_calendar_clicked_cb), selector);
		g_signal_connect (
			selector, "primary_selection_changed",
			G_CALLBACK (primary_selection_changed_cb), target);

		rb = gtk_radio_button_new_with_label (group, _(import_type_strings[i]));
		gtk_box_pack_start (GTK_BOX (hbox), rb, FALSE, FALSE, 6);

		sd = g_malloc0 (sizeof (*sd));
		sd->target = target;
		sd->selector = selector;
		sd->notebook = nb;
		sd->page = i;
		g_object_set_data_full ((GObject *) rb, "selector-data", sd, g_free);
		g_signal_connect (
			rb, "toggled",
			G_CALLBACK (button_toggled_cb), sd);

		if (!group)
			group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rb));
		if (first == NULL) {
			/* Set primary-source */
			primary_selection_changed_cb (E_SOURCE_SELECTOR (selector), target);
			g_datalist_set_data (&target->data, "primary-type", GINT_TO_POINTER (import_type_map[i]));
			first = rb;
		}
	}
	if (first)
		gtk_toggle_button_set_active ((GtkToggleButton *) first, TRUE);

	gtk_widget_show_all (top_vbox);
	g_hash_table_unref (known_readonly);

	return top_vbox;
}

static void
ivcal_call_import_done (gpointer user_data,
			const GError *error)
{
	ivcal_import_done (user_data, error);
}

static gboolean
ivcal_import_items (gpointer d)
{
	ICalImporter *ici = d;

	ici->idle_id = 0;

	switch (ici->source_type) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		prepare_events (ici->icomp, NULL);
		update_objects (ici->cal_client, ici->icomp, ici->cancellable, ivcal_call_import_done, ici);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		prepare_tasks (ici->icomp, NULL);
		update_objects (ici->cal_client, ici->icomp, ici->cancellable, ivcal_call_import_done, ici);
		break;
	default:
		g_warn_if_reached ();

		ivcal_import_done (ici, NULL);
		return FALSE;
	}

	return FALSE;
}

static void
ivcal_connect_cb (GObject *source_object,
                  GAsyncResult *result,
                  gpointer user_data)
{
	EClient *client;
	ICalImporter *ici = user_data;
	GError *error = NULL;

	g_return_if_fail (ici != NULL);

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		ivcal_import_done (ici, error);
		g_error_free (error);
		return;
	}

	ici->cal_client = E_CAL_CLIENT (client);

	e_import_status (ici->import, ici->target, _("Importing…"), 0);
	ici->idle_id = g_idle_add (ivcal_import_items, ici);
}

static void
ivcal_import (EImport *ei,
              EImportTarget *target,
              ICalComponent *icomp)
{
	ECalClientSourceType type;
	ICalImporter *ici = g_malloc0 (sizeof (*ici));

	type = GPOINTER_TO_INT (g_datalist_get_data (&target->data, "primary-type"));

	ici->import = ei;
	g_datalist_set_data (&target->data, "ivcal-data", ici);
	g_object_ref (ei);
	ici->target = target;
	ici->icomp = icomp;
	ici->cal_client = NULL;
	ici->source_type = type;
	ici->cancellable = g_cancellable_new ();
	e_import_status (ei, target, _("Opening calendar"), 0);

	e_cal_client_connect (
		g_datalist_get_data (&target->data, "primary-source"),
		type, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, ici->cancellable, ivcal_connect_cb, ici);
}

static void
ivcal_cancel (EImport *ei,
              EImportTarget *target,
              EImportImporter *im)
{
	ICalImporter *ici = g_datalist_get_data (&target->data, "ivcal-data");

	if (ici)
		g_cancellable_cancel (ici->cancellable);
}

/* ********************************************************************** */
/*
 * iCalendar importer functions.
 */

static gboolean
ical_supported (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	gboolean ret = FALSE;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *) target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp (s->uri_src, "file:///", 8) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename)
		return FALSE;

	contents = e_import_util_get_file_contents (filename, 128 * 1024, NULL);
	if (contents) {
		ICalComponent *icomp;

		icomp = e_cal_util_parse_ics_string (contents);
		ret = is_icomp_usable (icomp, contents);

		g_clear_object (&icomp);
		g_free (contents);
	}
	g_free (filename);

	return ret;
}

static void
ical_import (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	ICalComponent *icomp;
	GError *error = NULL;
	EImportTargetURI *s = (EImportTargetURI *) target;

	filename = g_filename_from_uri (s->uri_src, NULL, &error);
	if (!filename) {
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

	icomp = e_cal_util_parse_ics_string (contents);
	g_free (contents);

	if (icomp)
		ivcal_import (ei, target, icomp);
	else
		e_import_complete (ei, target, error);
}

static GtkWidget *
ivcal_get_preview (EImport *ei,
                   EImportTarget *target,
                   EImportImporter *im)
{
	GtkWidget *preview;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	ICalComponent *icomp;
	gchar *contents;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	contents = e_import_util_get_file_contents (filename, 128 * 1024, NULL);
	if (!contents) {
		g_free (filename);
		return NULL;
	}
	g_free (filename);

	icomp = e_cal_util_parse_ics_string (contents);
	g_free (contents);

	if (!icomp)
		return NULL;

	preview = ical_get_preview (icomp);

	g_object_unref (icomp);

	return preview;
}

static EImportImporter ical_importer = {
	E_IMPORT_TARGET_URI,
	0,
	ical_supported,
	ivcal_getwidget,
	ical_import,
	ivcal_cancel,
	ivcal_get_preview,
};

EImportImporter *
ical_importer_peek (void)
{
	ical_importer.name = _("iCalendar files (.ics)");
	ical_importer.description = _("Evolution iCalendar importer");

	return &ical_importer;
}

/* ********************************************************************** */
/*
 * vCalendar importer functions.
 */

static ICalComponent *
load_vcalendar_content (const gchar *contents)
{
	icalcomponent *icalcomp = NULL;
	VObject *vcal;

	if (!contents || !*contents)
		return NULL;

	/* parse the file */
	vcal = Parse_MIME (contents, strlen (contents));

	if (vcal) {
		icalcomp = icalvcal_convert (vcal);
		cleanVObject (vcal);
	}

	if (icalcomp) {
		return i_cal_object_construct (I_CAL_TYPE_COMPONENT, icalcomp,
			(GDestroyNotify) icalcomponent_free, FALSE, NULL);
	}

	return NULL;
}

static gboolean
vcal_supported (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	gchar *filename;
	gchar *contents;
	gboolean ret = FALSE;
	EImportTargetURI *s;

	if (target->type != E_IMPORT_TARGET_URI)
		return FALSE;

	s = (EImportTargetURI *) target;
	if (s->uri_src == NULL)
		return TRUE;

	if (strncmp (s->uri_src, "file:///", 8) != 0)
		return FALSE;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (!filename)
		return FALSE;

	contents = e_import_util_get_file_contents (filename, 128 * 1024, NULL);
	if (contents) {
		ICalComponent *icomp;

		icomp = e_cal_util_parse_ics_string (contents);

		if (is_icomp_usable (icomp, contents)) {
			/* If we can create proper iCalendar from the file, then
			 * rather use ics importer, because it knows to read more
			 * information than older version, the vCalendar. */
			ret = FALSE;

			g_clear_object (&icomp);
		} else {
			g_clear_object (&icomp);

			icomp = load_vcalendar_content (contents);
			ret = is_icomp_usable (icomp, NULL);
			g_clear_object (&icomp);
		}

		g_free (contents);
	}
	g_free (filename);

	return ret;
}

/* This tries to load in a vCalendar file and convert it to an ICalComponent.
 * It returns NULL on failure. */
static ICalComponent *
load_vcalendar_file (const gchar *filename)
{
	ICalComponent *icomp;
	gchar *contents;

	contents = e_import_util_get_file_contents (filename, 0, NULL);
	icomp = load_vcalendar_content (contents);
	g_free (contents);

	return icomp;
}

static void
vcal_import (EImport *ei,
             EImportTarget *target,
             EImportImporter *im)
{
	gchar *filename;
	ICalComponent *icomp;
	EImportTargetURI *s = (EImportTargetURI *) target;
	GError *error = NULL;

	filename = g_filename_from_uri (s->uri_src, NULL, &error);
	if (!filename) {
		e_import_complete (ei, target, error);
		return;
	}

	icomp = load_vcalendar_file (filename);
	g_free (filename);
	if (icomp)
		ivcal_import (ei, target, icomp);
	else
		e_import_complete (ei, target, error);
}

static GtkWidget *
vcal_get_preview (EImport *ei,
                  EImportTarget *target,
                  EImportImporter *im)
{
	GtkWidget *preview;
	EImportTargetURI *s = (EImportTargetURI *) target;
	gchar *filename;
	ICalComponent *icomp;

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	if (filename == NULL) {
		g_message (G_STRLOC ": Couldn't get filename from URI '%s'", s->uri_src);
		return NULL;
	}

	icomp = load_vcalendar_file (filename);
	g_free (filename);

	if (!icomp)
		return NULL;

	preview = ical_get_preview (icomp);

	g_object_unref (icomp);

	return preview;
}

static EImportImporter vcal_importer = {
	E_IMPORT_TARGET_URI,
	0,
	vcal_supported,
	ivcal_getwidget,
	vcal_import,
	ivcal_cancel,
	vcal_get_preview,
};

EImportImporter *
vcal_importer_peek (void)
{
	vcal_importer.name = _("vCalendar files (.vcs)");
	vcal_importer.description = _("Evolution vCalendar importer");

	return &vcal_importer;
}

/* ********************************************************************** */

static gboolean
gnome_calendar_supported (EImport *ei,
                          EImportTarget *target,
                          EImportImporter *im)
{
	gchar *filename;
	gboolean res;

	if (target->type != E_IMPORT_TARGET_HOME)
		return FALSE;

	filename = g_build_filename (g_get_home_dir (), "user-cal.vcf", NULL);
	res = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
	g_free (filename);

	return res;
}

static void
free_ici (gpointer ptr)
{
	ICalIntelligentImporter *ici = ptr;

	if (!ici)
		return;

	g_clear_object (&ici->icomp);
	g_object_unref (ici->cancellable);
	g_free (ici);
}

struct OpenDefaultSourceData
{
	ICalIntelligentImporter *ici;
	void (* opened_cb) (ECalClient *cal_client, const GError *error, ICalIntelligentImporter *ici);
};

static void
default_client_connect_cb (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	EClient *client;
	struct OpenDefaultSourceData *odsd = user_data;
	GError *error = NULL;

	g_return_if_fail (odsd != NULL);
	g_return_if_fail (odsd->ici != NULL);
	g_return_if_fail (odsd->opened_cb != NULL);

	client = e_cal_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Client may be NULL; don't use a type cast macro. */
	odsd->opened_cb ((ECalClient *) client, error, odsd->ici);

	g_clear_object (&client);

	if (error != NULL)
		g_error_free (error);

	g_slice_free (struct OpenDefaultSourceData, odsd);
}

static void
open_default_source (ICalIntelligentImporter *ici,
                     ECalClientSourceType source_type,
                     void (* opened_cb) (ECalClient *cal_client,
                                         const GError *error,
                                         ICalIntelligentImporter *ici))
{
	EShell *shell;
	ESource *source;
	ESourceRegistry *registry;
	struct OpenDefaultSourceData *odsd;

	g_return_if_fail (ici != NULL);
	g_return_if_fail (opened_cb != NULL);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			source = e_source_registry_ref_default_calendar (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			source = e_source_registry_ref_default_task_list (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			source = e_source_registry_ref_default_memo_list (registry);
			break;
		default:
			g_return_if_reached ();
	}

	odsd = g_slice_new0 (struct OpenDefaultSourceData);
	odsd->ici = ici;
	odsd->opened_cb = opened_cb;

	e_import_status (ici->ei, ici->target, _("Opening calendar"), 0);

	e_cal_client_connect (
		source, source_type, 30, ici->cancellable,
		default_client_connect_cb, odsd);

	g_object_unref (source);
}

static void
continue_done_cb (gpointer user_data,
		  const GError *error)
{
	ICalIntelligentImporter *ici = user_data;

	g_return_if_fail (ici != NULL);

	e_import_complete (ici->ei, ici->target, error);
}

static void
gc_import_tasks (ECalClient *cal_client,
                 const GError *error,
                 ICalIntelligentImporter *ici)
{
	g_return_if_fail (ici != NULL);

	if (error != NULL) {
		e_import_complete (ici->ei, ici->target, error);
		return;
	}

	e_import_status (ici->ei, ici->target, _("Importing…"), 0);

	prepare_tasks (ici->icomp, ici->tasks);

	update_objects (
		cal_client, ici->icomp,
		ici->cancellable, continue_done_cb, ici);
}

static void
continue_tasks_cb (gpointer user_data,
		   const GError *error)
{
	ICalIntelligentImporter *ici = user_data;

	g_return_if_fail (ici != NULL);

	if (error)
		continue_done_cb (ici, error);
	else
		open_default_source (ici, E_CAL_CLIENT_SOURCE_TYPE_TASKS, gc_import_tasks);
}

static void
gc_import_events (ECalClient *cal_client,
                  const GError *error,
                  ICalIntelligentImporter *ici)
{
	g_return_if_fail (ici != NULL);

	if (error != NULL) {
		if (ici->tasks)
			open_default_source (
				ici, E_CAL_CLIENT_SOURCE_TYPE_TASKS,
				gc_import_tasks);
		else
			e_import_complete (ici->ei, ici->target, error);
		return;
	}

	e_import_status (ici->ei, ici->target, _("Importing…"), 0);

	update_objects (
		cal_client, ici->icomp, ici->cancellable,
		ici->tasks ? continue_tasks_cb : continue_done_cb, ici);
}

static void
gnome_calendar_import (EImport *ei,
                       EImportTarget *target,
                       EImportImporter *im)
{
	ICalComponent *icomp = NULL;
	gchar *filename;
	gint do_calendar, do_tasks;
	ICalIntelligentImporter *ici;

	/* This is pretty shitty, everything runs in the gui thread and can block
	 * for quite some time */

	do_calendar = GPOINTER_TO_INT (g_datalist_get_data (&target->data, "gnomecal-do-cal"));
	do_tasks = GPOINTER_TO_INT (g_datalist_get_data (&target->data, "gnomecal-do-tasks"));

	/* If neither is selected, just return. */
	if (!do_calendar && !do_tasks)
		return;

	/* Load the Gnome Calendar file and convert to iCalendar. */
	filename = g_build_filename (g_get_home_dir (), "user-cal.vcf", NULL);
	icomp = load_vcalendar_file (filename);
	g_free (filename);

	/* If we couldn't load the file, just return. FIXME: Error message? */
	if (icomp) {
		ici = g_malloc0 (sizeof (*ici));
		ici->ei = ei;
		ici->target = target;
		ici->cancellable = g_cancellable_new ();
		ici->icomp = icomp;

		g_datalist_set_data_full (&target->data, "gnomecal-data", ici, free_ici);

		prepare_events (ici->icomp, &ici->tasks);
		if (do_calendar) {
			open_default_source (ici, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, gc_import_events);
			return;
		}

		prepare_tasks (ici->icomp, ici->tasks);
		if (do_tasks) {
			open_default_source (ici, E_CAL_CLIENT_SOURCE_TYPE_TASKS, gc_import_tasks);
			return;
		}
	}

	e_import_complete (ei, target, NULL);
}

static void
calendar_toggle_cb (GtkToggleButton *tb,
                    EImportTarget *target)
{
	g_datalist_set_data (&target->data, "gnomecal-do-cal", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
tasks_toggle_cb (GtkToggleButton *tb,
                 EImportTarget *target)
{
	g_datalist_set_data (&target->data, "gnomecal-do-tasks", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static GtkWidget *
gnome_calendar_getwidget (EImport *ei,
                          EImportTarget *target,
                          EImportImporter *im)
{
	GtkWidget *hbox, *w;
	GSettings *settings;
	gboolean done_cal, done_tasks;

	settings = e_util_ref_settings ("org.gnome.evolution.importer");
	done_cal = g_settings_get_boolean (settings, "gnome-calendar-done-calendar");
	done_tasks = g_settings_get_boolean (settings, "gnome-calendar-done-tasks");
	g_object_unref (settings);

	g_datalist_set_data (&target->data, "gnomecal-do-cal", GINT_TO_POINTER (!done_cal));
	g_datalist_set_data (&target->data, "gnomecal-do-tasks", GINT_TO_POINTER (!done_tasks));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

	w = gtk_check_button_new_with_label (_("Calendar Events"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, !done_cal);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (calendar_toggle_cb), target);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	w = gtk_check_button_new_with_label (_("Tasks"));
	gtk_toggle_button_set_active ((GtkToggleButton *) w, !done_tasks);
	g_signal_connect (
		w, "toggled",
		G_CALLBACK (tasks_toggle_cb), target);
	gtk_box_pack_start (GTK_BOX (hbox), w, FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
gnome_calendar_cancel (EImport *ei,
                       EImportTarget *target,
                       EImportImporter *im)
{
	ICalIntelligentImporter *ici = g_datalist_get_data (&target->data, "gnomecal-data");

	if (ici)
		g_cancellable_cancel (ici->cancellable);
}

static EImportImporter gnome_calendar_importer = {
	E_IMPORT_TARGET_HOME,
	0,
	gnome_calendar_supported,
	gnome_calendar_getwidget,
	gnome_calendar_import,
	gnome_calendar_cancel,
	NULL, /* get_preview */
};

EImportImporter *
gnome_calendar_importer_peek (void)
{
	gnome_calendar_importer.name = _("GNOME Calendar");
	gnome_calendar_importer.description = _("Evolution Calendar intelligent importer");

	return &gnome_calendar_importer;
}

/* ********************************************************************** */

static gchar *
format_dt (const ECalComponentDateTime *dt,
           GHashTable *timezones,
           ICalTimezone *users_zone)
{
	ICalTime *tt;
	struct tm tm;

	g_return_val_if_fail (timezones != NULL, NULL);

	if (!dt || !e_cal_component_datetime_get_value (dt))
		return NULL;

	tt = e_cal_component_datetime_get_value (dt);

	if (e_cal_component_datetime_get_tzid (dt)) {
		const gchar *tzid = e_cal_component_datetime_get_tzid (dt);

		i_cal_time_set_timezone (tt, g_hash_table_lookup (timezones, tzid));

		if (!i_cal_time_get_timezone (tt))
			i_cal_time_set_timezone (tt, i_cal_timezone_get_builtin_timezone_from_tzid (tzid));

		if (!i_cal_time_get_timezone (tt))
			i_cal_time_set_timezone (tt, i_cal_timezone_get_builtin_timezone (tzid));

		if (!i_cal_time_get_timezone (tt) && g_ascii_strcasecmp (tzid, "UTC") == 0)
			i_cal_time_set_timezone (tt, i_cal_timezone_get_utc_timezone ());
	} else if (!i_cal_time_is_utc (tt)) {
		i_cal_time_set_timezone (tt, NULL);
	}

	if (i_cal_time_get_timezone (tt))
		tm = e_cal_util_icaltime_to_tm_with_zone (tt, i_cal_time_get_timezone (tt), users_zone);
	else
		tm = e_cal_util_icaltime_to_tm (tt);

	return e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (tt) ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

static void
add_url_section (EWebViewPreview *preview,
		 const gchar *section,
		 const gchar *raw_value)
{
	gchar *html;

	g_return_if_fail (raw_value != NULL);

	html = camel_text_to_html (raw_value, CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);

	if (html) {
		e_web_view_preview_add_section_html (preview, section, html);
		g_free (html);
	} else {
		e_web_view_preview_add_section (preview, section, raw_value);
	}
}

static void
preview_comp (EWebViewPreview *preview,
              ECalComponent *comp)
{
	ECalComponentText *text;
	ECalComponentDateTime *dt;
	ECalComponentClassification classif;
	const gchar *str;
	gchar *tmp;
	gint percent;
	gboolean have;
	GHashTable *timezones;
	ICalTimezone *users_zone;
	GSList *slist, *l;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (comp != NULL);

	timezones = g_object_get_data (G_OBJECT (preview), "iCalImp-timezones");
	users_zone = g_object_get_data (G_OBJECT (preview), "iCalImp-userszone");

	str = NULL;
	switch (e_cal_component_get_vtype (comp)) {
	case E_CAL_COMPONENT_EVENT:
		str = e_cal_component_has_attendees (comp) ? C_("iCalImp", "Meeting") : C_("iCalImp", "Event");
		break;
	case E_CAL_COMPONENT_TODO:
		str = C_("iCalImp", "Task");
		break;
	case E_CAL_COMPONENT_JOURNAL:
		str = C_("iCalImp", "Memo");
		break;
	default:
		str = "??? Other ???";
		break;
	}

	have = FALSE;
	if (e_cal_component_is_instance (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "is an instance"));
		have = TRUE;
	} else if (e_cal_component_has_recurrences (comp)) {
		e_web_view_preview_add_section (preview, /*have ? NULL :*/ str, C_("iCalImp", "has recurrences"));
		have = TRUE;
	}

	if (e_cal_component_has_alarms (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "has reminders"));
		have = TRUE;
	}

	if (e_cal_component_has_attachments (comp)) {
		e_web_view_preview_add_section (preview, have ? NULL : str, C_("iCalImp", "has attachments"));
		have = TRUE;
	}

	if (!have) {
		e_web_view_preview_add_section (preview, str, "");
	}

	str = NULL;
	classif = e_cal_component_get_classification (comp);
	if (classif == E_CAL_COMPONENT_CLASS_PUBLIC) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Public");
	} else if (classif == E_CAL_COMPONENT_CLASS_PRIVATE) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Private");
	} else if (classif == E_CAL_COMPONENT_CLASS_CONFIDENTIAL) {
		/* Translators: Appointment's classification */
		str = C_("iCalImp", "Confidential");
	}
	if (str)
		/* Translators: Appointment's classification section name */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Classification"), str);

	text = e_cal_component_dup_summary_for_locale (comp, NULL);
	if (text && (e_cal_component_text_get_value (text) || e_cal_component_text_get_altrep (text)))
		/* Translators: Appointment's summary */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Summary"),
			e_cal_component_text_get_value (text) ? e_cal_component_text_get_value (text) : e_cal_component_text_get_altrep (text));
	e_cal_component_text_free (text);

	tmp = e_cal_component_get_location (comp);
	if (tmp && *tmp)
		/* Translators: Appointment's location */
		add_url_section (preview, C_("iCalImp", "Location"), tmp);
	g_free (tmp);

	dt = e_cal_component_get_dtstart (comp);
	if (dt && e_cal_component_datetime_get_value (dt)) {
		tmp = format_dt (dt, timezones, users_zone);
		if (tmp)
			/* Translators: Appointment's start time */
			e_web_view_preview_add_section (preview, C_("iCalImp", "Start"), tmp);
		g_free (tmp);
	}
	e_cal_component_datetime_free (dt);

	dt = e_cal_component_get_due (comp);
	if (dt && e_cal_component_datetime_get_value (dt)) {
		tmp = format_dt (dt, timezones, users_zone);
		if (tmp)
			/* Translators: 'Due' like the time due a task should be finished */
			e_web_view_preview_add_section (preview, C_("iCalImp", "Due"), tmp);
		g_free (tmp);
	} else {
		e_cal_component_datetime_free (dt);

		dt = e_cal_component_get_dtend (comp);
		if (dt && e_cal_component_datetime_get_value (dt)) {
			tmp = format_dt (dt, timezones, users_zone);

			if (tmp)
				/* Translators: Appointment's end time */
				e_web_view_preview_add_section (preview, C_("iCalImp", "End"), tmp);
			g_free (tmp);
		}
	}
	e_cal_component_datetime_free (dt);

	tmp = e_cal_component_get_categories (comp);
	if (tmp && *tmp)
		/* Translators: Appointment's categories */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Categories"), tmp);
	g_free (tmp);

	percent = e_cal_component_get_percent_complete (comp);
	if (percent >= 0) {
		tmp = NULL;
		if (percent == 100) {
			ICalTime *completed;

			completed = e_cal_component_get_completed (comp);

			if (completed) {
				dt = e_cal_component_datetime_new (completed, "UTC");

				tmp = format_dt (dt, timezones, users_zone);

				e_cal_component_datetime_free (dt);
				g_object_unref (completed);
			}
		}

		if (!tmp)
			tmp = g_strdup_printf ("%d%%", percent);

		/* Translators: Appointment's complete value (either percentage, or a date/time of a completion) */
		e_web_view_preview_add_section (preview, C_("iCalImp", "Completed"), tmp);
		g_free (tmp);
	}

	tmp = e_cal_component_get_url (comp);
	if (tmp && *tmp)
		/* Translators: Appointment's URL */
		add_url_section (preview, C_("iCalImp", "URL"), tmp);
	g_free (tmp);

	if (e_cal_component_has_organizer (comp)) {
		ECalComponentOrganizer *organizer;
		const gchar *organizer_email;

		organizer = e_cal_component_get_organizer (comp);
		organizer_email = e_cal_util_get_organizer_email (organizer);

		if (organizer_email) {
			const gchar *cn;

			cn = e_cal_component_organizer_get_cn (organizer);

			if (cn && *cn) {
				tmp = g_strconcat (cn, " <", organizer_email, ">", NULL);
				/* Translators: Appointment's organizer */
				e_web_view_preview_add_section (preview, C_("iCalImp", "Organizer"), tmp);
				g_free (tmp);
			} else {
				e_web_view_preview_add_section (preview, C_("iCalImp", "Organizer"), organizer_email);
			}
		}

		e_cal_component_organizer_free (organizer);
	}

	if (e_cal_component_has_attendees (comp)) {
		GSList *attendees, *link;
		have = FALSE;

		attendees = e_cal_component_get_attendees (comp);

		for (link = attendees; link; link = g_slist_next (link)) {
			ECalComponentAttendee *attnd = link->data;
			ECalComponentParameterBag *param_bag;
			GString *tmp_str;
			const gchar *value, *cn;

			if (!attnd)
				continue;

			value = e_cal_util_get_attendee_email (attnd);
			if (!value || !*value)
				continue;

			cn = e_cal_component_attendee_get_cn (attnd);

			tmp_str = g_string_new ("");

			if (cn && *cn) {
				g_string_append_printf (tmp_str, "%s <%s>", cn, e_cal_util_strip_mailto (value));
			} else {
				g_string_append (tmp_str, e_cal_util_strip_mailto (value));
			}

			param_bag = e_cal_component_attendee_get_parameter_bag (attnd);
			if (param_bag) {
				ICalParameter *num_guests = NULL;
				ICalParameter *response_comment = NULL;
				guint ii, count;

				count = e_cal_component_parameter_bag_get_count (param_bag);
				for (ii = 0; ii < count && (!num_guests || !response_comment); ii++) {
					ICalParameter *param = e_cal_component_parameter_bag_get (param_bag, ii);

					if (param && i_cal_parameter_isa (param) == I_CAL_X_PARAMETER) {
						const gchar *xname = i_cal_parameter_get_xname (param);

						if (!xname)
							continue;

						if (!num_guests && g_ascii_strcasecmp (xname, "X-NUM-GUESTS") == 0)
							num_guests = param;

						if (!response_comment && g_ascii_strcasecmp (xname, "X-RESPONSE-COMMENT") == 0)
							response_comment = param;
					}
				}

				if (num_guests && i_cal_parameter_get_xvalue (num_guests)) {
					gint n_guests;

					n_guests = (gint) g_ascii_strtoll (i_cal_parameter_get_xvalue (num_guests), NULL, 10);

					if (n_guests > 0) {
						g_string_append_c (tmp_str, ' ');
						g_string_append_printf (tmp_str, g_dngettext (GETTEXT_PACKAGE, "with one guest", "with %d guests", n_guests), n_guests);
					}
				}

				if (response_comment) {
					value = i_cal_parameter_get_xvalue (response_comment);

					if (value && *value) {
						g_string_append (tmp_str, " (");
						g_string_append (tmp_str, value);
						g_string_append_c (tmp_str, ')');
					}
				}
			}

			/* Translators: Appointment's attendees */
			e_web_view_preview_add_section (preview, have ? NULL : C_("iCalImp", "Attendees"), tmp_str->str);

			g_string_free (tmp_str, TRUE);

			have = TRUE;
		}

		g_slist_free_full (attendees, e_cal_component_attendee_free);
	}

	if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
		slist = e_cal_component_get_descriptions (comp);

		for (l = slist; l; l = l->next) {
			ECalComponentText *txt = l->data;
			const gchar *value;

			value = txt ? e_cal_component_text_get_value (txt) : NULL;

			e_web_view_preview_add_section (preview, l != slist ? NULL : C_("iCalImp", "Description"), value ? value : "");
		}

		g_slist_free_full (slist, e_cal_component_text_free);
	} else {
		text = e_cal_component_dup_description_for_locale (comp, NULL);
		if (text) {
			const gchar *value = e_cal_component_text_get_value (text);
			e_web_view_preview_add_section (preview, C_("iCalImp", "Description"), value ? value : "");
		}
		e_cal_component_text_free (text);
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
		ECalComponent *comp = NULL;

		gtk_tree_model_get (model, &iter, 3, &comp, -1);

		if (comp) {
			preview_comp (preview, comp);
			g_object_unref (comp);
		}
	}

	e_web_view_preview_end_update (preview);
}

static ICalTimezone *
get_users_timezone (void)
{
	/* more or less copy&paste of calendar_config_get_icaltimezone */
	GSettings *settings;
	ICalTimezone *zone = NULL;
	gchar *location;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone")) {
		location = e_cal_util_get_system_timezone_location ();
	} else {
		location = g_settings_get_string (settings, "timezone");
	}

	g_object_unref (settings);

	if (location) {
		zone = i_cal_timezone_get_builtin_timezone (location);

		g_free (location);
	}

	return zone;
}

static GtkWidget *
ical_get_preview (ICalComponent *icomp)
{
	GtkWidget *preview;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkListStore *store;
	GtkTreeIter iter;
	GHashTable *timezones;
	ICalComponent *subcomp;
	ICalTimezone *users_zone;

	if (!icomp || !is_icomp_usable (icomp, NULL))
		return NULL;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, E_TYPE_CAL_COMPONENT);

	timezones = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	users_zone = get_users_timezone ();

	/* get timezones first */
	for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_VTIMEZONE_COMPONENT);
	     subcomp;
	     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp,  I_CAL_VTIMEZONE_COMPONENT)) {
		ICalTimezone *zone = i_cal_timezone_new ();
		if (!i_cal_timezone_set_component (zone, i_cal_component_clone (subcomp)) || !i_cal_timezone_get_tzid (zone)) {
			g_object_unref (zone);
		} else {
			g_hash_table_insert (timezones, (gchar *) i_cal_timezone_get_tzid (zone), zone);
		}
	}

	/* then each component */
	for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_ANY_COMPONENT);
	     subcomp;
	     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp,  I_CAL_ANY_COMPONENT)) {
		ICalComponentKind kind = i_cal_component_isa (subcomp);

		if (kind == I_CAL_VEVENT_COMPONENT ||
		    kind == I_CAL_VTODO_COMPONENT ||
		    kind == I_CAL_VJOURNAL_COMPONENT) {
			ECalComponent *comp;
			ECalComponentText *summary;
			ECalComponentDateTime *dt;
			gchar *formatted_dt;
			const gchar *summary_txt = NULL;

			comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (subcomp));
			if (!comp)
				continue;

			summary = e_cal_component_dup_summary_for_locale (comp, NULL);
			if (summary) {
				const gchar *value, *altrep;

				value = e_cal_component_text_get_value (summary);
				altrep = e_cal_component_text_get_altrep (summary);

				summary_txt = (value && *value) ? value : (altrep && *altrep) ? altrep : NULL;
			}

			dt = e_cal_component_get_dtstart (comp);
			formatted_dt = format_dt (dt, timezones, users_zone);

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (
				store, &iter,
				0, kind == I_CAL_VEVENT_COMPONENT ? (e_cal_component_has_attendees (comp) ? C_("iCalImp", "Meeting") : C_("iCalImp", "Event")) :
				kind == I_CAL_VTODO_COMPONENT ? C_("iCalImp", "Task") :
				kind == I_CAL_VJOURNAL_COMPONENT ? C_("iCalImp", "Memo") : "??? Other ???",
				1, formatted_dt ? formatted_dt : "",
				2, summary_txt ? summary_txt : "",
				3, comp,
				-1);

			e_cal_component_datetime_free (dt);
			e_cal_component_text_free (summary);
			g_object_unref (comp);
			g_free (formatted_dt);
		}
	}

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		g_object_unref (store);
		g_hash_table_destroy (timezones);
		return NULL;
	}

	preview = e_web_view_preview_new ();
	gtk_widget_show (preview);

	g_object_set_data_full (G_OBJECT (preview), "iCalImp-timezones", timezones, (GDestroyNotify) g_hash_table_destroy);
	g_object_set_data (G_OBJECT (preview), "iCalImp-userszone", users_zone);

	tree_view = e_web_view_preview_get_tree_view (E_WEB_VIEW_PREVIEW (preview));
	g_return_val_if_fail (tree_view != NULL, NULL);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_tree_view_insert_column_with_attributes (
		/* Translators: Column header for a component type; it can be Event, Task or Memo */
		tree_view, -1, C_("iCalImp", "Type"),
		gtk_cell_renderer_text_new (), "text", 0, NULL);

	gtk_tree_view_insert_column_with_attributes (
		/* Translators: Column header for a component start date/time */
		tree_view, -1, C_("iCalImp", "Start"),
		gtk_cell_renderer_text_new (), "text", 1, NULL);

	gtk_tree_view_insert_column_with_attributes (
		/* Translators: Column header for a component summary */
		tree_view, -1, C_("iCalImp", "Summary"),
		gtk_cell_renderer_text_new (), "text", 2, NULL);

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
