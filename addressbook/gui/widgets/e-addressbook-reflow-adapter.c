/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include <config.h>
#include <string.h>

#include <libgnome/gnome-i18n.h>
#include "eab-marshal.h"
#include "e-addressbook-reflow-adapter.h"
#include "e-addressbook-model.h"
#include "e-addressbook-view.h"
#include "eab-gui-util.h"

#include "e-minicard.h"
#include <gal/widgets/e-gui-utils.h>
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"


struct _EAddressbookReflowAdapterPrivate {
	EABModel *model;

	gboolean loading;

	int create_contact_id, remove_contact_id, modify_contact_id, model_changed_id;
	int search_started_id, search_result_id;
};

#define PARENT_TYPE e_reflow_model_get_type()
static EReflowModel *parent_class;

#define d(x)

enum {
	PROP_0,
	PROP_BOOK,
	PROP_QUERY,
	PROP_EDITABLE,
	PROP_MODEL,
};

enum {
	DRAG_BEGIN,
	LAST_SIGNAL
};

static guint e_addressbook_reflow_adapter_signals [LAST_SIGNAL] = {0, };

static void
unlink_model(EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	if (priv->model && priv->create_contact_id)
		g_signal_handler_disconnect (priv->model,
					     priv->create_contact_id);
	if (priv->model && priv->remove_contact_id)
		g_signal_handler_disconnect (priv->model,
					     priv->remove_contact_id);
	if (priv->model && priv->modify_contact_id)
		g_signal_handler_disconnect (priv->model,
					     priv->modify_contact_id);
	if (priv->model && priv->model_changed_id)
		g_signal_handler_disconnect (priv->model,
					     priv->model_changed_id);
	if (priv->model && priv->search_started_id)
		g_signal_handler_disconnect (priv->model,
					     priv->search_started_id);
	if (priv->model && priv->search_result_id)
		g_signal_handler_disconnect (priv->model,
					     priv->search_result_id);

	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;
	priv->search_started_id = 0;
	priv->search_result_id = 0;

	if (priv->model)
		g_object_unref (priv->model);

	priv->model = NULL;
}


static int
text_height (PangoLayout *layout, const gchar *text)
{
	int height;

	pango_layout_set_text (layout, text, -1);

	pango_layout_get_pixel_size (layout, NULL, &height);

	return height;
}

static void
addressbook_dispose(GObject *object)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(object);

	unlink_model (adapter);
}

static void
addressbook_set_width (EReflowModel *erm, int width)
{
}

/* This function returns the number of items in our EReflowModel. */
static int
addressbook_count (EReflowModel *erm)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	return eab_model_contact_count (priv->model);
}

/* This function returns the height of the minicontact in question */
static int
addressbook_height (EReflowModel *erm, int i, GnomeCanvasGroup *parent)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	EContactField field;
	int count = 0;
	char *string;
	EContact *contact = (EContact*)eab_model_contact_at (priv->model, i);
	PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (GNOME_CANVAS_ITEM (parent)->canvas), "");
	int height;

	string = e_contact_get(contact, E_CONTACT_FILE_AS);
	height = text_height (layout, string ? string : "") + 10.0;
	g_free(string);

	for(field = E_CONTACT_FULL_NAME; field != E_CONTACT_LAST_SIMPLE_STRING && count < 5; field++) {

		if (field == E_CONTACT_FAMILY_NAME || field == E_CONTACT_GIVEN_NAME)
			continue;

		string = e_contact_get(contact, field);
		if (string && *string) {
			int this_height;
			int field_text_height;

			this_height = text_height (layout, e_contact_pretty_name(field));

			field_text_height = text_height (layout, string);
			if (this_height < field_text_height)
				this_height = field_text_height;

			this_height += 3;

			height += this_height;
			count ++;
		}
		g_free (string);
	}
	height += 2;

	g_object_unref (layout);

	return height;
}

static int
addressbook_compare (EReflowModel *erm, int n1, int n2)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	EContact *contact1, *contact2;

	if (priv->loading) {
		return n1-n2;
	}
	else {
		contact1 = (EContact*)eab_model_contact_at (priv->model, n1);
		contact2 = (EContact*)eab_model_contact_at (priv->model, n2);

		if (contact1 && contact2) {
			const char *file_as1, *file_as2;
			file_as1 = e_contact_get_const (contact1, E_CONTACT_FILE_AS);
			file_as2 = e_contact_get_const (contact2, E_CONTACT_FILE_AS);
			if (file_as1 && file_as2)
				return g_utf8_collate(file_as1, file_as2);
			if (file_as1)
				return -1;
			if (file_as2)
				return 1;
			return strcmp(e_contact_get_const (contact1, E_CONTACT_UID),
				      e_contact_get_const (contact2, E_CONTACT_UID));
		}
		if (contact1)
			return -1;
		if (contact2)
			return 1;
		return 0;
	}
}

static int
adapter_drag_begin (EMinicard *card, GdkEvent *event, EAddressbookReflowAdapter *adapter)
{
	gint ret_val = 0;

	g_signal_emit (adapter,
		       e_addressbook_reflow_adapter_signals[DRAG_BEGIN], 0,
		       event, &ret_val);

	return ret_val;
}

static GnomeCanvasItem *
addressbook_incarnate (EReflowModel *erm, int i, GnomeCanvasGroup *parent)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	GnomeCanvasItem *item;

	item = gnome_canvas_item_new(parent,
				     e_minicard_get_type(),
				     "contact", eab_model_contact_at (priv->model, i),
				     "editable", eab_model_editable (priv->model),
				     NULL);

#if 0
	g_signal_connect (item, "selected",
			  G_CALLBACK(card_selected), 0, emvm);
#endif

	g_signal_connect (item, "drag_begin",
			  G_CALLBACK(adapter_drag_begin), adapter);
	
	return item;
}

static void
addressbook_reincarnate (EReflowModel *erm, int i, GnomeCanvasItem *item)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	gnome_canvas_item_set(item,
			      "contact", eab_model_contact_at (priv->model, i),
			      NULL);
}

static void
create_contact (EABModel *model,
		gint index, gint count,
		EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_items_inserted (E_REFLOW_MODEL (adapter),
				       index,
				       count);
}

static void
remove_contact (EABModel *model,
		gint index,
		EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_item_removed (E_REFLOW_MODEL (adapter), index);
}

static void
modify_contact (EABModel *model,
		gint index,
		EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_item_changed (E_REFLOW_MODEL (adapter), index);
}

static void
model_changed (EABModel *model,
	       EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
}

static void
search_started (EABModel *model,
		EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->loading = TRUE;
}

static void
search_result (EABModel *model,
	       EBookViewStatus status,
	       EAddressbookReflowAdapter *adapter)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->loading = FALSE;

	e_reflow_model_comparison_changed (E_REFLOW_MODEL (adapter));
}

static void
addressbook_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(object);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (prop_id) {
	case PROP_BOOK:
		g_object_set (priv->model,
			      "book", g_value_get_object (value),
			      NULL);
		break;
	case PROP_QUERY:
		g_object_set (priv->model,
			      "query", g_value_get_string (value),
			      NULL);
		break;
	case PROP_EDITABLE:
		g_object_set (priv->model,
			      "editable", g_value_get_boolean (value),
			      NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
addressbook_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(object);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (prop_id) {
	case PROP_BOOK: {
		g_object_get_property (G_OBJECT (priv->model),
				       "book", value);
		break;
	}
	case PROP_QUERY: {
		g_object_get_property (G_OBJECT (priv->model),
				       "query", value);
		break;
	}
	case PROP_EDITABLE: {
		g_object_get_property (G_OBJECT (priv->model),
				       "editable", value);
		break;
	}
	case PROP_MODEL:
		g_value_set_object (value, priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_addressbook_reflow_adapter_class_init (GObjectClass *object_class)
{
	EReflowModelClass *model_class = (EReflowModelClass *) object_class;

	parent_class = g_type_class_peek_parent (object_class);

	object_class->set_property = addressbook_set_property;
	object_class->get_property = addressbook_get_property;
	object_class->dispose = addressbook_dispose;

	g_object_class_install_property (object_class, PROP_BOOK, 
					 g_param_spec_object ("book",
							      _("Book"),
							      /*_( */"XXX blurb" /*)*/,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_string ("query",
							      _("Query"),
							      /*_( */"XXX blurb" /*)*/,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model",
							       _("Model"),
							      /*_( */"XXX blurb" /*)*/,
							      EAB_TYPE_MODEL,
							      G_PARAM_READABLE));

	e_addressbook_reflow_adapter_signals [DRAG_BEGIN] =
		g_signal_new ("drag_begin",
			      G_OBJECT_CLASS_TYPE(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookReflowAdapterClass, drag_begin),
			      NULL, NULL,
			      eab_marshal_INT__POINTER,
			      G_TYPE_INT, 1, G_TYPE_POINTER);

	model_class->set_width = addressbook_set_width;
	model_class->count = addressbook_count;
	model_class->height = addressbook_height;
	model_class->compare = addressbook_compare;
	model_class->incarnate = addressbook_incarnate;
	model_class->reincarnate = addressbook_reincarnate;
}

static void
e_addressbook_reflow_adapter_init (GtkObject *object)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(object);
	EAddressbookReflowAdapterPrivate *priv;

	priv = adapter->priv = g_new0 (EAddressbookReflowAdapterPrivate, 1);

	priv->loading = FALSE;
	priv->create_contact_id = 0;
	priv->remove_contact_id = 0;
	priv->modify_contact_id = 0;
	priv->model_changed_id = 0;
	priv->search_started_id = 0;
	priv->search_result_id = 0;
}

GType
e_addressbook_reflow_adapter_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (EAddressbookReflowAdapterClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_addressbook_reflow_adapter_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EAddressbookReflowAdapter),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_addressbook_reflow_adapter_init,
		};

		type = g_type_register_static (PARENT_TYPE, "EAddressbookReflowAdapter", &info, 0);
	}

	return type;
}

void
e_addressbook_reflow_adapter_construct (EAddressbookReflowAdapter *adapter,
					EABModel *model)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_contact_id = g_signal_connect(priv->model,
						"contact_added",
						G_CALLBACK(create_contact),
						adapter);
	priv->remove_contact_id = g_signal_connect(priv->model,
						"contact_removed",
						G_CALLBACK(remove_contact),
						adapter);
	priv->modify_contact_id = g_signal_connect(priv->model,
						"contact_changed",
						G_CALLBACK(modify_contact),
						adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);
	priv->search_started_id = g_signal_connect(priv->model,
						   "search_started",
						   G_CALLBACK(search_started),
						   adapter);
	priv->search_result_id = g_signal_connect(priv->model,
						  "search_result",
						  G_CALLBACK(search_result),
						  adapter);
}

EReflowModel *
e_addressbook_reflow_adapter_new (EABModel *model)
{
	EAddressbookReflowAdapter *et;

	et = g_object_new (E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, NULL);

	e_addressbook_reflow_adapter_construct (et, model);

	return E_REFLOW_MODEL(et);
}


EContact *
e_addressbook_reflow_adapter_get_contact (EAddressbookReflowAdapter *adapter,
					  int index)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	return eab_model_get_contact (priv->model, index);
}
