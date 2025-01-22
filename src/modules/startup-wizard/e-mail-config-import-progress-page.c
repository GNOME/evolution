/*
 * e-mail-config-import-progress-page.c
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

#include "e-mail-config-import-progress-page.h"

struct _EMailConfigImportProgressPagePrivate {
	EActivity *activity;
};

enum {
	PROP_0,
	PROP_ACTIVITY
};

/* Forward Declarations */
static void	e_mail_config_import_progress_page_interface_init
					(EMailConfigPageInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EMailConfigImportProgressPage, e_mail_config_import_progress_page, GTK_TYPE_SCROLLED_WINDOW, 0,
	G_ADD_PRIVATE_DYNAMIC (EMailConfigImportProgressPage)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_MAIL_CONFIG_PAGE, e_mail_config_import_progress_page_interface_init))

static gboolean
mail_config_import_progress_page_is_cancelled (GBinding *binding,
                                               const GValue *source_value,
                                               GValue *target_value,
                                               gpointer unused)
{
	EActivityState state;
	gboolean is_cancelled;

	state = g_value_get_enum (source_value);
	is_cancelled = (state == E_ACTIVITY_CANCELLED);
	g_value_set_boolean (target_value, is_cancelled);

	return TRUE;
}

static gboolean
mail_config_import_progress_page_is_completed (GBinding *binding,
                                               const GValue *source_value,
                                               GValue *target_value,
                                               gpointer unused)
{
	EActivityState state;
	gboolean is_completed;

	state = g_value_get_enum (source_value);
	is_completed = (state == E_ACTIVITY_COMPLETED);
	g_value_set_boolean (target_value, is_completed);

	return TRUE;
}

static gboolean
mail_config_import_progress_page_percent_to_fraction (GBinding *binding,
                                                      const GValue *source_value,
                                                      GValue *target_value,
                                                      gpointer unused)
{
	gdouble fraction;

	fraction = g_value_get_double (source_value) / 100.0;
	g_value_set_double (target_value, CLAMP (fraction, 0.0, 1.0));

	return TRUE;
}

static void
mail_config_import_progress_page_set_activity (EMailConfigImportProgressPage *page,
                                               EActivity *activity)
{
	g_return_if_fail (E_IS_ACTIVITY (activity));
	g_return_if_fail (page->priv->activity == NULL);

	page->priv->activity = g_object_ref (activity);
}

static void
mail_config_import_progress_page_set_property (GObject *object,
                                               guint property_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			mail_config_import_progress_page_set_activity (
				E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_import_progress_page_get_property (GObject *object,
                                               guint property_id,
                                               GValue *value,
                                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVITY:
			g_value_set_object (
				value,
				e_mail_config_import_progress_page_get_activity (
				E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_config_import_progress_page_dispose (GObject *object)
{
	EMailConfigImportProgressPage *self = E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (object);

	g_clear_object (&self->priv->activity);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_import_progress_page_parent_class)->dispose (object);
}

static void
mail_config_import_progress_page_constructed (GObject *object)
{
	EMailConfigImportProgressPage *page;
	GtkSizeGroup *size_group;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *main_box;
	GtkWidget *completed_msg;
	EActivity *activity;

	page = E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_import_progress_page_parent_class)->constructed (object);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);

	gtk_widget_set_valign (GTK_WIDGET (main_box), GTK_ALIGN_CENTER);

	activity = e_mail_config_import_progress_page_get_activity (page);

	/* The activity state affects the "check-complete" result. */
	e_signal_connect_notify_swapped (
		activity, "notify::state",
		G_CALLBACK (e_mail_config_page_changed), page);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	/* Just a spacer. */
	widget = gtk_grid_new ();
	gtk_size_group_add_widget (size_group, widget);
	gtk_box_pack_start (GTK_BOX (main_box), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	widget = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (main_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		activity, "text",
		widget, "text",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		activity, "percent",
		widget, "fraction",
		G_BINDING_SYNC_CREATE,
		mail_config_import_progress_page_percent_to_fraction,
		NULL,
		NULL, (GDestroyNotify) NULL);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_size_group_add_widget (size_group, widget);
	gtk_box_pack_start (GTK_BOX (main_box), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	completed_msg = widget;

	e_binding_bind_property_full (
		activity, "state",
		widget, "visible",
		G_BINDING_SYNC_CREATE,
		mail_config_import_progress_page_is_completed,
		NULL,
		NULL, (GDestroyNotify) NULL);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		activity, "state",
		widget, "visible",
		G_BINDING_SYNC_CREATE,
		mail_config_import_progress_page_is_cancelled,
		NULL,
		NULL, (GDestroyNotify) NULL);

	container = widget;

	widget = gtk_image_new_from_icon_name ("process-stop", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new (_("Import cancelled."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = completed_msg;

	widget = gtk_image_new_from_icon_name (
		"emblem-default", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new _("Import complete.");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_object_unref (size_group);

	e_mail_config_page_set_content (E_MAIL_CONFIG_PAGE (page), main_box);
}

static gboolean
mail_config_import_progress_page_check_complete (EMailConfigPage *page)
{
	EMailConfigImportProgressPage *self = E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (page);
	gboolean complete;

	switch (e_activity_get_state (self->priv->activity)) {
		case E_ACTIVITY_CANCELLED:
		case E_ACTIVITY_COMPLETED:
			complete = TRUE;
			break;
		default:
			complete = FALSE;
			break;
	}

	return complete;
}

static void
e_mail_config_import_progress_page_class_init (EMailConfigImportProgressPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_config_import_progress_page_set_property;
	object_class->get_property = mail_config_import_progress_page_get_property;
	object_class->dispose = mail_config_import_progress_page_dispose;
	object_class->constructed = mail_config_import_progress_page_constructed;

	g_object_class_install_property (
		object_class,
		PROP_ACTIVITY,
		g_param_spec_object (
			"activity",
			"Activity",
			"Import activity",
			E_TYPE_ACTIVITY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_mail_config_import_progress_page_class_finalize (EMailConfigImportProgressPageClass *class)
{
}

static void
e_mail_config_import_progress_page_interface_init (EMailConfigPageInterface *iface)
{
	/* Keep the title identical to EMailConfigImportPage
	 * so it's only shown once in the assistant sidebar. */
	iface->title = _("Importing Files");
	iface->sort_order = E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_SORT_ORDER;
	iface->page_type = GTK_ASSISTANT_PAGE_PROGRESS;
	iface->check_complete = mail_config_import_progress_page_check_complete;
}

static void
e_mail_config_import_progress_page_init (EMailConfigImportProgressPage *page)
{
	page->priv = e_mail_config_import_progress_page_get_instance_private (page);
}

void
e_mail_config_import_progress_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_import_progress_page_register_type (type_module);
}

EMailConfigPage *
e_mail_config_import_progress_page_new (EActivity *activity)
{
	g_return_val_if_fail (E_IS_ACTIVITY (activity), NULL);

	return g_object_new (
		E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE,
		"activity", activity, NULL);
}

EActivity *
e_mail_config_import_progress_page_get_activity (EMailConfigImportProgressPage *page)
{
	g_return_val_if_fail (
		E_IS_MAIL_CONFIG_IMPORT_PROGRESS_PAGE (page), NULL);

	return page->priv->activity;
}

