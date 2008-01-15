#include "e-composer-from-header.h"

/* Convenience macro */
#define E_COMPOSER_FROM_HEADER_GET_COMBO_BOX(header) \
	(E_ACCOUNT_COMBO_BOX (E_COMPOSER_HEADER (header)->input_widget))

static gpointer parent_class;

static void
composer_from_header_changed_cb (EAccountComboBox *combo_box,
                                 EComposerFromHeader *header)
{
	g_signal_emit_by_name (header, "changed");
}

static void
composer_from_header_class_init (EComposerFromHeaderClass *class)
{
	parent_class = g_type_class_peek_parent (class);
}

static void
composer_from_header_init (EComposerFromHeader *header)
{
	GtkWidget *widget;

	widget = g_object_ref_sink (e_account_combo_box_new ());
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (composer_from_header_changed_cb), header);
	E_COMPOSER_HEADER (header)->input_widget = widget;
	gtk_widget_show (widget);
}

GType
e_composer_from_header_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EComposerFromHeaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) composer_from_header_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EComposerFromHeader),
			0,     /* n_preallocs */
			(GInstanceInitFunc) composer_from_header_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_COMPOSER_HEADER, "EComposerFromHeader",
			&type_info, 0);
	}

	return type;
}

EComposerHeader *
e_composer_from_header_new (const gchar *label)
{
	return g_object_new (
		E_TYPE_COMPOSER_FROM_HEADER, "label", label,
		"button", FALSE, NULL);
}

void
e_composer_from_header_set_account_list (EComposerFromHeader *header,
                                         EAccountList *account_list)
{
	EAccountComboBox *combo_box;

	g_return_if_fail (E_IS_COMPOSER_FROM_HEADER (header));

	combo_box = E_COMPOSER_FROM_HEADER_GET_COMBO_BOX (header);
	e_account_combo_box_set_account_list (combo_box, account_list);
}

EAccount *
e_composer_from_header_get_active (EComposerFromHeader *header)
{
	EAccountComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	combo_box = E_COMPOSER_FROM_HEADER_GET_COMBO_BOX (header);
	return e_account_combo_box_get_active (combo_box);
}

gboolean
e_composer_from_header_set_active (EComposerFromHeader *header,
                                   EAccount *account)
{
	EAccountComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), FALSE);

	combo_box = E_COMPOSER_FROM_HEADER_GET_COMBO_BOX (header);
	return e_account_combo_box_set_active (combo_box, account);
}

const gchar *
e_composer_from_header_get_active_name (EComposerFromHeader *header)
{
	EAccountComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	combo_box = E_COMPOSER_FROM_HEADER_GET_COMBO_BOX (header);
	return e_account_combo_box_get_active_name (combo_box);
}

gboolean
e_composer_from_header_set_active_name (EComposerFromHeader *header,
                                        const gchar *account_name)
{
	EAccountComboBox *combo_box;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), FALSE);

	combo_box = E_COMPOSER_FROM_HEADER_GET_COMBO_BOX (header);
	return e_account_combo_box_set_active_name (combo_box, account_name);
}

CamelInternetAddress *
e_composer_from_header_get_active_address (EComposerFromHeader *header)
{
	CamelInternetAddress *address;
	EAccount *account;

	g_return_val_if_fail (E_IS_COMPOSER_FROM_HEADER (header), NULL);

	account = e_composer_from_header_get_active (header);
	if (account == NULL)
		return NULL;

	address = camel_internet_address_new ();
	camel_internet_address_add (
		address, account->id->name, account->id->address);

	return address;
}
