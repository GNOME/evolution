#include "e-composer-text-header.h"

static gpointer parent_class;

static void
composer_text_header_changed_cb (GtkEntry *entry,
                                 EComposerTextHeader *header)
{
	g_signal_emit_by_name (header, "changed");
}

static gboolean
composer_text_header_query_tooltip_cb (GtkEntry *entry,
                                       gint x,
                                       gint y,
                                       gboolean keyboard_mode,
                                       GtkTooltip *tooltip)
{
	const gchar *text;

	text = gtk_entry_get_text (entry);

	if (keyboard_mode || text == NULL || *text == '\0')
		return FALSE;

	gtk_tooltip_set_text (tooltip, text);

	return TRUE;
}

static void
composer_text_header_class_init (EComposerTextHeaderClass *class)
{
	parent_class = g_type_class_peek_parent (class);
}

static void
composer_text_header_init (EComposerTextHeader *header)
{
	GtkWidget *widget;

	widget = g_object_ref_sink (gtk_entry_new ());
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (composer_text_header_changed_cb), header);
	g_signal_connect (
		widget, "query-tooltip",
		G_CALLBACK (composer_text_header_query_tooltip_cb), NULL);
	gtk_widget_set_has_tooltip (widget, TRUE);
	E_COMPOSER_HEADER (header)->input_widget = widget;
}

GType
e_composer_text_header_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EComposerTextHeaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) composer_text_header_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EComposerTextHeader),
			0,     /* n_preallocs */
			(GInstanceInitFunc) composer_text_header_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_COMPOSER_HEADER, "EComposerTextHeader",
			&type_info, 0);
	}

	return type;
}

EComposerHeader *
e_composer_text_header_new_label (const gchar *label)
{
	return g_object_new (
		E_TYPE_COMPOSER_TEXT_HEADER, "label", label,
		"button", FALSE, NULL);
}

EComposerHeader *
e_composer_text_header_new_button (const gchar *label)
{
	return g_object_new (
		E_TYPE_COMPOSER_TEXT_HEADER, "label", label,
		"button", TRUE, NULL);
}

const gchar *
e_composer_text_header_get_text (EComposerTextHeader *header)
{
	GtkWidget *input_widget;

	g_return_val_if_fail (E_IS_COMPOSER_TEXT_HEADER (header), NULL);

	input_widget = E_COMPOSER_HEADER (header)->input_widget;
	return gtk_entry_get_text (GTK_ENTRY (input_widget));
}

void
e_composer_text_header_set_text (EComposerTextHeader *header,
                                 const gchar *text)
{
	GtkWidget *input_widget;

	g_return_if_fail (E_IS_COMPOSER_TEXT_HEADER (header));

	if (text == NULL)
		text = "";

	input_widget = E_COMPOSER_HEADER (header)->input_widget;
	gtk_entry_set_text (GTK_ENTRY (input_widget), text);
}
