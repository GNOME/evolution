/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include <config.h>

#include <libgnome/gnome-i18n.h>
#include "e-addressbook-marshal.h"
#include "e-addressbook-reflow-adapter.h"
#include "e-addressbook-model.h"
#include "e-addressbook-view.h"
#include "e-addressbook-util.h"

#include "e-minicard.h"
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-font.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include "e-contact-save-as.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/printing/e-contact-print-envelope.h"


struct _EAddressbookReflowAdapterPrivate {
	EAddressbookModel *model;
	
	int create_card_id, remove_card_id, modify_card_id, model_changed_id;
};

#define PARENT_TYPE e_reflow_model_get_type()
EReflowModel *parent_class;

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

	g_signal_handler_disconnect (priv->model,
				     priv->create_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->remove_card_id);
	g_signal_handler_disconnect (priv->model,
				     priv->modify_card_id);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;

	g_object_unref (priv->model);

	priv->model = NULL;
}


static int
count_lines (const gchar *text)
{
	int num_lines = 1;
	gunichar unival;

	for (text = e_unicode_get_utf8 (text, &unival); (unival && text); text = e_unicode_get_utf8 (text, &unival)) {
		if (unival == '\n') {
			num_lines ++;
		}
	}

	return num_lines;
}

static int
text_height (GnomeCanvas *canvas, const gchar *text)
{
	EFont *font = e_font_from_gdk_font (gtk_style_get_font (gtk_widget_get_style (GTK_WIDGET (canvas))));
	gint height = e_font_height (font) * count_lines (text) / canvas->pixels_per_unit;

	e_font_unref (font);
	return height;
}

static void
addressbook_finalize(GObject *object)
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

	return e_addressbook_model_card_count (priv->model);
}

/* This function returns the number of items in our EReflowModel. */
static int
addressbook_height (EReflowModel *erm, int i, GnomeCanvasGroup *parent)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	/* FIXME */
	ECardSimpleField field;
	int count = 0;
	int height;
	char *string;
	ECardSimple *simple = e_card_simple_new (e_addressbook_model_card_at (priv->model, i));

	string = e_card_simple_get(simple, E_CARD_SIMPLE_FIELD_FILE_AS);
	height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, string ? string : "") + 10.0;
	g_free(string);

	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING && count < 5; field++) {

		if (field == E_CARD_SIMPLE_FIELD_FAMILY_NAME)
			continue;

		string = e_card_simple_get(simple, field);
		if (string && *string) {
			int this_height;
			int field_text_height;

			this_height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, e_card_simple_get_name(simple, field));

			field_text_height = text_height (GNOME_CANVAS_ITEM (parent)->canvas, string);
			if (this_height < field_text_height)
				this_height = field_text_height;

			this_height += 3;

			height += this_height;
			count ++;
		}
		g_free (string);
	}
	height += 2;

	g_object_unref (simple);

	return height;
}

static int
addressbook_compare (EReflowModel *erm, int n1, int n2)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(erm);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;
	ECard *card1, *card2;
	
	card1 = e_addressbook_model_card_at (priv->model, n1);
	card2 = e_addressbook_model_card_at (priv->model, n2);

	if (card1 && card2) {
		char *file_as1, *file_as2;
		file_as1 = card1->file_as;
		file_as2 = card2->file_as;
		if (file_as1 && file_as2)
			return g_utf8_collate(file_as1, file_as2);
		if (file_as1)
			return -1;
		if (file_as2)
			return 1;
		return strcmp(e_card_get_id(card1), e_card_get_id(card2));
	}
	if (card1)
		return -1;
	if (card2)
		return 1;
	return 0;
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
				     "card", e_addressbook_model_card_at (priv->model, i),
				     "editable", e_addressbook_model_editable (priv->model),
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
			      "card", e_addressbook_model_card_at (priv->model, i),
			      NULL);
}



static void
create_card(EAddressbookModel *model,
	    gint index, gint count,
	    EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_items_inserted (E_REFLOW_MODEL (adapter),
				       index,
				       count);
}

static void
remove_card(EAddressbookModel *model,
	    gint index,
	    EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
}

static void
modify_card(EAddressbookModel *model,
	    gint index,
	    EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_item_changed (E_REFLOW_MODEL (adapter), index);
}

static void
model_changed(EAddressbookModel *model,
	      EAddressbookReflowAdapter *adapter)
{
	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
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
		EBook *book;
		g_object_get (GTK_OBJECT (priv->model),
			      "book", &book,
			      NULL);

		g_value_set_object (value, book);
		break;
	}
	case PROP_QUERY: {
		char *query;
		g_object_get (priv->model,
			      "query", &query,
			      NULL);
		g_value_set_string (value, query);
		break;
	}
	case PROP_EDITABLE: {
		gboolean editable;
		g_object_get (priv->model,
			      "editable", &editable,
			      NULL);
		g_value_set_boolean (value, editable);
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

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_property = addressbook_set_property;
	object_class->get_property = addressbook_get_property;
	object_class->finalize = addressbook_finalize;

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
							      E_TYPE_ADDRESSBOOK_MODEL,
							      G_PARAM_READABLE));

	e_addressbook_reflow_adapter_signals [DRAG_BEGIN] =
		g_signal_new ("drag_begin",
			      G_OBJECT_CLASS_TYPE(object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EAddressbookReflowAdapterClass, drag_begin),
			      NULL, NULL,
			      e_addressbook_marshal_INT__POINTER,
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

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;
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
					EAddressbookModel *model)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	g_object_ref (priv->model);

	priv->create_card_id = g_signal_connect(priv->model,
						"card_added",
						G_CALLBACK(create_card),
						adapter);
	priv->remove_card_id = g_signal_connect(priv->model,
						"card_removed",
						G_CALLBACK(remove_card),
						adapter);
	priv->modify_card_id = g_signal_connect(priv->model,
						"card_changed",
						G_CALLBACK(modify_card),
						adapter);
	priv->model_changed_id = g_signal_connect(priv->model,
						  "model_changed",
						  G_CALLBACK(model_changed),
						  adapter);
}

EReflowModel *
e_addressbook_reflow_adapter_new (EAddressbookModel *model)
{
	EAddressbookReflowAdapter *et;

	et = g_object_new (E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER, NULL);

	e_addressbook_reflow_adapter_construct (et, model);

	return E_REFLOW_MODEL(et);
}


ECard *
e_addressbook_reflow_adapter_get_card (EAddressbookReflowAdapter *adapter,
				       int index)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	return e_addressbook_model_get_card (priv->model, index);
}
