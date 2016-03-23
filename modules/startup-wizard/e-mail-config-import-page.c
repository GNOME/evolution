/*
 * e-mail-config-import-page.c
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

#include "e-mail-config-import-page.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_CONFIG_IMPORT_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PAGE, EMailConfigImportPagePrivate))

typedef struct _AsyncContext AsyncContext;

struct _EMailConfigImportPagePrivate {
	EImport *import;
	EImportTarget *import_target;
	GSList *available_importers;
};

struct _AsyncContext {
	EMailConfigImportPage *page;
	GQueue pending_importers;
	EActivity *activity;
	GCancellable *cancellable;
	gulong cancel_id;
};

/* Forward Declarations */
static void	e_mail_config_import_page_interface_init
					(EMailConfigPageInterface *iface);
static gboolean	mail_config_import_page_next	(gpointer user_data);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailConfigImportPage,
	e_mail_config_import_page,
	GTK_TYPE_BOX,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_CONFIG_PAGE,
		e_mail_config_import_page_interface_init))

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->page != NULL)
		g_object_unref (async_context->page);

	if (async_context->activity != NULL)
		g_object_unref (async_context->activity);

	if (async_context->cancellable != NULL) {
		g_cancellable_disconnect (
			async_context->cancellable,
			async_context->cancel_id);
		g_object_unref (async_context->cancellable);
	}

	g_queue_clear (&async_context->pending_importers);

	g_slice_free (AsyncContext, async_context);
}

static void
mail_config_import_page_status (EImport *import,
				const gchar *what,
				gint percent,
				gpointer user_data)
{
	GSimpleAsyncResult *simple = user_data;
	AsyncContext *async_context;

	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	e_activity_set_text (async_context->activity, what);
	e_activity_set_percent (async_context->activity, (gdouble) percent);
}

static void
mail_config_import_page_complete (EImport *import,
				  const GError *error,
                                  gpointer user_data)
{
	GSimpleAsyncResult *simple = user_data;

	if (error) {
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
	} else {
		/* Schedule the next importer to start. */
		g_idle_add (mail_config_import_page_next, simple);
	}
}

static gboolean
mail_config_import_page_next (gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GCancellable *cancellable;
	EImportImporter *next_importer;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);
	cancellable = async_context->cancellable;

	/* Pop the completed importer and peek at the next one. */
	g_queue_pop_head (&async_context->pending_importers);
	next_importer = g_queue_peek_head (&async_context->pending_importers);

	if (g_cancellable_set_error_if_cancelled (cancellable, &error)) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);

	} else if (next_importer != NULL) {
		e_import_import (
			async_context->page->priv->import,
			async_context->page->priv->import_target,
			next_importer,
			mail_config_import_page_status,
			mail_config_import_page_complete,
			simple);

	} else {
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
	}

	return FALSE;
}

static void
mail_config_import_page_cancelled (GCancellable *cancellable,
                                   AsyncContext *async_context)
{
	GQueue *pending_importers;
	EImportImporter *current_importer;

	pending_importers = &async_context->pending_importers;
	current_importer = g_queue_peek_head (pending_importers);
	g_return_if_fail (current_importer != NULL);

	e_import_cancel (
		async_context->page->priv->import,
		async_context->page->priv->import_target,
		current_importer);
}

static void
mail_config_import_page_dispose (GObject *object)
{
	EMailConfigImportPagePrivate *priv;

	priv = E_MAIL_CONFIG_IMPORT_PAGE_GET_PRIVATE (object);

	if (priv->import != NULL) {
		e_import_target_free (
			priv->import,
			priv->import_target);
		g_object_unref (priv->import);
		priv->import_target = NULL;
		priv->import = NULL;
	}

	g_slist_free (priv->available_importers);
	priv->available_importers = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_config_import_page_parent_class)->
		dispose (object);
}

static void
mail_config_import_page_constructed (GObject *object)
{
	EMailConfigImportPage *page;
	GtkWidget *widget;
	GtkWidget *container;
	GSList *list, *link;
	const gchar *text;
	gint row = 0;

	page = E_MAIL_CONFIG_IMPORT_PAGE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_config_import_page_parent_class)->constructed (object);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing (GTK_BOX (page), 24);

	text = _("Please select the information "
		 "that you would like to import:");
	widget = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (widget), 12);
	gtk_grid_set_column_spacing (GTK_GRID (widget), 12);
	gtk_box_pack_start (GTK_BOX (page), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	list = page->priv->available_importers;

	for (link = list; link != NULL; link = link->next) {
		EImportImporter *importer = link->data;
		gchar *from_text;

		widget = e_import_get_widget (
			page->priv->import,
			page->priv->import_target, importer);
		if (widget == NULL)
			continue;
		gtk_grid_attach (GTK_GRID (container), widget, 1, row, 1, 1);
		gtk_widget_show (widget);

		from_text = g_strdup_printf (_("From %s:"), importer->name);
		widget = gtk_label_new (from_text);
		gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
		gtk_grid_attach (GTK_GRID (container), widget, 0, row, 1, 1);
		gtk_widget_show (widget);

		row++;
	}
}

static void
e_mail_config_import_page_class_init (EMailConfigImportPageClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailConfigImportPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_config_import_page_dispose;
	object_class->constructed = mail_config_import_page_constructed;
}

static void
e_mail_config_import_page_class_finalize (EMailConfigImportPageClass *class)
{
}

static void
e_mail_config_import_page_interface_init (EMailConfigPageInterface *iface)
{
	iface->title = _("Importing Files");
	iface->sort_order = E_MAIL_CONFIG_IMPORT_PAGE_SORT_ORDER;
}

static void
e_mail_config_import_page_init (EMailConfigImportPage *page)
{
	page->priv = E_MAIL_CONFIG_IMPORT_PAGE_GET_PRIVATE (page);

	page->priv->import =
		e_import_new ("org.gnome.evolution.shell.importer");
	page->priv->import_target = (EImportTarget *)
		e_import_target_new_home (page->priv->import);
	page->priv->available_importers = e_import_get_importers (
		page->priv->import, page->priv->import_target);
}

void
e_mail_config_import_page_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_mail_config_import_page_register_type (type_module);
}

EMailConfigPage *
e_mail_config_import_page_new (void)
{
	return g_object_new (E_TYPE_MAIL_CONFIG_IMPORT_PAGE, NULL);
}

guint
e_mail_config_import_page_get_n_importers (EMailConfigImportPage *page)
{
	g_return_val_if_fail (E_IS_MAIL_CONFIG_IMPORT_PAGE (page), 0);

	return g_slist_length (page->priv->available_importers);
}

void
e_mail_config_import_page_import (EMailConfigImportPage *page,
                                  EActivity *activity,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	GCancellable *cancellable;
	EImportImporter *first_importer;
	GSList *list, *link;

	g_return_if_fail (E_IS_MAIL_CONFIG_IMPORT_PAGE (page));
	g_return_if_fail (E_IS_ACTIVITY (activity));

	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->page = g_object_ref (page);
	async_context->activity = g_object_ref (activity);

	list = page->priv->available_importers;

	for (link = list; link != NULL; link = g_slist_next (link)) {
		EImportImporter *importer = link->data;
		g_queue_push_tail (&async_context->pending_importers, importer);
	}

	if (G_IS_CANCELLABLE (cancellable)) {
		async_context->cancellable = g_object_ref (cancellable);
		async_context->cancel_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (mail_config_import_page_cancelled),
			async_context, (GDestroyNotify) NULL);
	}

	simple = g_simple_async_result_new (
		G_OBJECT (page), callback, user_data,
		e_mail_config_import_page_import);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	/* Start the first importer. */

	first_importer = g_queue_peek_head (&async_context->pending_importers);

	if (first_importer != NULL)
		e_import_import (
			async_context->page->priv->import,
			async_context->page->priv->import_target,
			first_importer,
			mail_config_import_page_status,
			mail_config_import_page_complete,
			simple);
	else
		g_simple_async_result_complete_in_idle (simple);
}

gboolean
e_mail_config_import_page_import_finish (EMailConfigImportPage *page,
                                         GAsyncResult *result,
                                         GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (page),
		e_mail_config_import_page_import), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

