/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


#include <config.h>

#include "e-addressbook-reflow-adapter.h"
#include "e-addressbook-model.h"
#include "e-addressbook-view.h"
#include "e-addressbook-util.h"

#include <gal/util/e-i18n.h>

#include "e-minicard.h"
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-font.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/unicode/gunicode.h>
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
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_EDITABLE,
	ARG_MODEL,
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

	gtk_signal_disconnect(GTK_OBJECT (priv->model),
			      priv->create_card_id);
	gtk_signal_disconnect(GTK_OBJECT (priv->model),
			      priv->remove_card_id);
	gtk_signal_disconnect(GTK_OBJECT (priv->model),
			      priv->modify_card_id);
	gtk_signal_disconnect(GTK_OBJECT (priv->model),
			      priv->model_changed_id);

	priv->create_card_id = 0;
	priv->remove_card_id = 0;
	priv->modify_card_id = 0;
	priv->model_changed_id = 0;

	gtk_object_unref(GTK_OBJECT(priv->model));

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
	EFont *font = e_font_from_gdk_font (((GtkWidget *) canvas)->style->font);
	gint height = e_font_height (font) * count_lines (text) / canvas->pixels_per_unit;

	e_font_unref (font);
	return height;
}

static void
addressbook_finalize(GtkObject *object)
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

	gtk_object_unref (GTK_OBJECT (simple));

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

	gtk_signal_emit (GTK_OBJECT(adapter),
			 e_addressbook_reflow_adapter_signals[DRAG_BEGIN],
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
	gtk_signal_connect (GTK_OBJECT (item), "selected",
			    GTK_SIGNAL_FUNC(card_selected), emvm);
#endif

	gtk_signal_connect (GTK_OBJECT (item), "drag_begin",
			    GTK_SIGNAL_FUNC(adapter_drag_begin), adapter);

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
addressbook_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(o);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (arg_id){
	case ARG_BOOK:
		gtk_object_set (GTK_OBJECT (priv->model),
				"book", GTK_VALUE_OBJECT (*arg),
				NULL);
		break;
	case ARG_QUERY:
		gtk_object_set (GTK_OBJECT (priv->model),
				"query", GTK_VALUE_STRING (*arg),
				NULL);
		break;
	case ARG_EDITABLE:
		gtk_object_set (GTK_OBJECT (priv->model),
				"editable", GTK_VALUE_BOOL (*arg),
				NULL);
		break;
	}
}

static void
addressbook_get_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EAddressbookReflowAdapter *adapter = E_ADDRESSBOOK_REFLOW_ADAPTER(o);
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	switch (arg_id) {
	case ARG_BOOK: {
		EBook *book;
		gtk_object_get (GTK_OBJECT (priv->model),
				"book", &book,
				NULL);
		if (book)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(book);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	}
	case ARG_QUERY: {
		char *query;
		gtk_object_get (GTK_OBJECT (priv->model),
				"query", &query,
				NULL);
		GTK_VALUE_STRING (*arg) = query;
		break;
	}
	case ARG_EDITABLE: {
		gboolean editable;
		gtk_object_get (GTK_OBJECT (priv->model),
				"editable", &editable,
				NULL);
		GTK_VALUE_BOOL (*arg) = editable;
		break;
	}
	case ARG_MODEL:
		if (priv->model)
			GTK_VALUE_OBJECT (*arg) = GTK_OBJECT (priv->model);
		else
			GTK_VALUE_OBJECT (*arg) = NULL;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_addressbook_reflow_adapter_class_init (GtkObjectClass *object_class)
{
	EReflowModelClass *model_class = (EReflowModelClass *) object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->set_arg = addressbook_set_arg;
	object_class->get_arg = addressbook_get_arg;
	object_class->finalize = addressbook_finalize;

	gtk_object_add_arg_type ("EAddressbookReflowAdapter::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EAddressbookReflowAdapter::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EAddressbookReflowAdapter::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);
	gtk_object_add_arg_type ("EAddressbookReflowAdapter::model", E_ADDRESSBOOK_MODEL_TYPE,
				 GTK_ARG_READABLE, ARG_MODEL);

	e_addressbook_reflow_adapter_signals [DRAG_BEGIN] =
		gtk_signal_new ("drag_begin",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EAddressbookReflowAdapterClass, drag_begin),
				gtk_marshal_INT__POINTER,
				GTK_TYPE_INT, 1, GTK_TYPE_POINTER);


	gtk_object_class_add_signals (object_class, e_addressbook_reflow_adapter_signals, LAST_SIGNAL);

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

GtkType
e_addressbook_reflow_adapter_get_type (void)
{
	static GtkType type = 0;

	if (!type){
		GtkTypeInfo info = {
			"EAddressbookReflowAdapter",
			sizeof (EAddressbookReflowAdapter),
			sizeof (EAddressbookReflowAdapterClass),
			(GtkClassInitFunc) e_addressbook_reflow_adapter_class_init,
			(GtkObjectInitFunc) e_addressbook_reflow_adapter_init,
			NULL, /* reserved 1 */
			NULL, /* reserved 2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}

void
e_addressbook_reflow_adapter_construct (EAddressbookReflowAdapter *adapter,
					EAddressbookModel *model)
{
	EAddressbookReflowAdapterPrivate *priv = adapter->priv;

	priv->model = model;
	gtk_object_ref (GTK_OBJECT (priv->model));

	priv->create_card_id = gtk_signal_connect(GTK_OBJECT(priv->model),
						  "card_added",
						  GTK_SIGNAL_FUNC(create_card),
						  adapter);
	priv->remove_card_id = gtk_signal_connect(GTK_OBJECT(priv->model),
						  "card_removed",
						  GTK_SIGNAL_FUNC(remove_card),
						  adapter);
	priv->modify_card_id = gtk_signal_connect(GTK_OBJECT(priv->model),
						  "card_changed",
						  GTK_SIGNAL_FUNC(modify_card),
						  adapter);
	priv->model_changed_id = gtk_signal_connect(GTK_OBJECT(priv->model),
						    "model_changed",
						    GTK_SIGNAL_FUNC(model_changed),
						    adapter);
}

EReflowModel *
e_addressbook_reflow_adapter_new (EAddressbookModel *model)
{
	EAddressbookReflowAdapter *et;

	et = gtk_type_new (e_addressbook_reflow_adapter_get_type ());

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
