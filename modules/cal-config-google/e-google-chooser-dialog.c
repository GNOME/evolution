/*
 * e-google-chooser-dialog.c
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

#include "e-google-chooser-dialog.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_GOOGLE_CHOOSER_DIALOG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_GOOGLE_CHOOSER_DIALOG, EGoogleChooserDialogPrivate))

struct _EGoogleChooserDialogPrivate {
	EGoogleChooser *chooser;
	GCancellable *cancellable;

	GtkWidget *info_bar;		/* not referenced */
	GtkWidget *info_bar_label;	/* not referenced */
};

enum {
	PROP_0,
	PROP_CHOOSER
};

G_DEFINE_DYNAMIC_TYPE (
	EGoogleChooserDialog,
	e_google_chooser_dialog,
	GTK_TYPE_DIALOG)

static void
google_chooser_dialog_populated_cb (EGoogleChooser *chooser,
                                    GAsyncResult *result,
                                    EGoogleChooserDialog *dialog)
{
	GdkWindow *window;
	GError *error = NULL;

	e_google_chooser_populate_finish (chooser, result, &error);

	/* Ignore cancellations, and leave the mouse cursor alone
	 * since the GdkWindow may have already been destroyed. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto exit;

	/* Reset the mouse cursor to normal. */
	window = gtk_widget_get_window (GTK_WIDGET (dialog));
	gdk_window_set_cursor (window, NULL);

	if (error != NULL) {
		GtkLabel *label;

		label = GTK_LABEL (dialog->priv->info_bar_label);
		gtk_label_set_text (label, error->message);
		gtk_widget_show (dialog->priv->info_bar);

		g_error_free (error);
		goto exit;
	}

exit:
	g_object_unref (dialog);
}

static void
google_chooser_dialog_row_activated_cb (GtkTreeView *tree_view,
                                        GtkTreePath *path,
                                        GtkTreeViewColumn *column,
                                        GtkDialog *dialog)
{
	gtk_dialog_response (dialog, GTK_RESPONSE_APPLY);
}

static void
google_chooser_dialog_selection_changed_cb (GtkTreeSelection *selection,
                                            GtkDialog *dialog)
{
	gboolean sensitive;

	sensitive = (gtk_tree_selection_count_selected_rows (selection) > 0);

	gtk_dialog_set_response_sensitive (
		dialog, GTK_RESPONSE_APPLY, sensitive);
}

static void
google_chooser_dialog_set_chooser (EGoogleChooserDialog *dialog,
                                   EGoogleChooser *chooser)
{
	g_return_if_fail (E_IS_GOOGLE_CHOOSER (chooser));
	g_return_if_fail (dialog->priv->chooser == NULL);

	dialog->priv->chooser = g_object_ref_sink (chooser);
}

static void
google_chooser_dialog_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHOOSER:
			google_chooser_dialog_set_chooser (
				E_GOOGLE_CHOOSER_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_dialog_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CHOOSER:
			g_value_set_object (
				value,
				e_google_chooser_dialog_get_chooser (
				E_GOOGLE_CHOOSER_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_dialog_dispose (GObject *object)
{
	EGoogleChooserDialogPrivate *priv;

	priv = E_GOOGLE_CHOOSER_DIALOG_GET_PRIVATE (object);

	if (priv->chooser != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->chooser, google_chooser_dialog_row_activated_cb,
			object);
		g_object_unref (priv->chooser);
		priv->chooser = NULL;
	}

	if (priv->cancellable != NULL) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_google_chooser_dialog_parent_class)->dispose (object);
}

static void
google_chooser_dialog_constructed (GObject *object)
{
	EGoogleChooserDialog *dialog;
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *vbox;
	const gchar *title;

	dialog = E_GOOGLE_CHOOSER_DIALOG (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_google_chooser_dialog_parent_class)->constructed (object);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (
		GTK_DIALOG (dialog),
		_("_Apply"), GTK_RESPONSE_APPLY);

	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_APPLY);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_APPLY, FALSE);

	title = _("Choose a Calendar");
	gtk_window_set_title (GTK_WINDOW (dialog), title);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 400);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = vbox = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = GTK_WIDGET (dialog->priv->chooser);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	g_signal_connect (
		widget, "row-activated",
		G_CALLBACK (google_chooser_dialog_row_activated_cb), dialog);

	/* Build the info bar, but hide it initially. */

	container = vbox;

	widget = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (
		GTK_INFO_BAR (widget), GTK_MESSAGE_WARNING);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	dialog->priv->info_bar = widget;  /* do not reference */
	gtk_widget_hide (widget);

	container = gtk_info_bar_get_content_area (GTK_INFO_BAR (widget));

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	dialog->priv->info_bar_label = widget;  /* do not reference */
	gtk_widget_show (widget);

	/* Listen for tree view selection changes. */

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (dialog->priv->chooser));

	g_signal_connect (
		selection, "changed",
		G_CALLBACK (google_chooser_dialog_selection_changed_cb),
		dialog);
}

static void
google_chooser_dialog_realize (GtkWidget *widget)
{
	EGoogleChooserDialogPrivate *priv;
	GdkCursor *cursor;
	GdkWindow *window;
	GdkDisplay *display;

	priv = E_GOOGLE_CHOOSER_DIALOG_GET_PRIVATE (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_google_chooser_dialog_parent_class)->
		realize (widget);

	g_return_if_fail (priv->cancellable == NULL);
	priv->cancellable = g_cancellable_new ();

	/* Show a busy mouse cursor while populating. */
	window = gtk_widget_get_window (widget);
	display = gtk_widget_get_display (widget);
	cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
	gdk_window_set_cursor (window, cursor);
	g_object_unref (cursor);

	e_google_chooser_populate (
		priv->chooser, priv->cancellable, (GAsyncReadyCallback)
		google_chooser_dialog_populated_cb, g_object_ref (widget));
}

static void
google_chooser_dialog_response (GtkDialog *dialog,
                                gint response_id)
{
	EGoogleChooserDialogPrivate *priv;

	priv = E_GOOGLE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

	if (response_id == GTK_RESPONSE_APPLY)
		e_google_chooser_apply_selected (priv->chooser);
}

static void
e_google_chooser_dialog_class_init (EGoogleChooserDialogClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkDialogClass *dialog_class;

	g_type_class_add_private (class, sizeof (EGoogleChooserDialogPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = google_chooser_dialog_set_property;
	object_class->get_property = google_chooser_dialog_get_property;
	object_class->dispose = google_chooser_dialog_dispose;
	object_class->constructed = google_chooser_dialog_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = google_chooser_dialog_realize;

	dialog_class = GTK_DIALOG_CLASS (class);
	dialog_class->response = google_chooser_dialog_response;

	g_object_class_install_property (
		object_class,
		PROP_CHOOSER,
		g_param_spec_object (
			"chooser",
			NULL,
			NULL,
			E_TYPE_GOOGLE_CHOOSER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_google_chooser_dialog_class_finalize (EGoogleChooserDialogClass *class)
{
}

static void
e_google_chooser_dialog_init (EGoogleChooserDialog *dialog)
{
	dialog->priv = E_GOOGLE_CHOOSER_DIALOG_GET_PRIVATE (dialog);
}

void
e_google_chooser_dialog_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_google_chooser_dialog_register_type (type_module);
}

GtkWidget *
e_google_chooser_dialog_new (EGoogleChooser *chooser,
                             GtkWindow *parent)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER (chooser), NULL);
	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

	return g_object_new (
		E_TYPE_GOOGLE_CHOOSER_DIALOG,
		"chooser", chooser, "transient-for", parent, NULL);
}

EGoogleChooser *
e_google_chooser_dialog_get_chooser (EGoogleChooserDialog *dialog)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER_DIALOG (dialog), NULL);

	return dialog->priv->chooser;
}

