
/* Copyright (C) 2004 Michael Zucchi */

/* This file is licensed under the GNU GPL v2 or later */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include "camel/camel-multipart.h"
#include "camel/camel-mime-part.h"
#include "mail/em-format-hook.h"
#include "mail/em-format.h"

#include <gconf/gconf-client.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include "mail/em-config.h"

void org_gnome_prefer_plain_multipart_alternative(void *ep, EMFormatHookTarget *t);
void org_gnome_prefer_plain_text_html(void *ep, EMFormatHookTarget *t);
GtkWidget *org_gnome_prefer_plain_config_mode(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

enum {
	EPP_NORMAL,
	EPP_PREFER,
	EPP_TEXT
};

static GConfClient *epp_gconf;
static int epp_mode;

void
org_gnome_prefer_plain_text_html(void *ep, EMFormatHookTarget *t)
{
	/* In text-only mode, all html output is suppressed */
	if (epp_mode != EPP_TEXT)
		t->item->handler.old->handler(t->format, t->stream, t->part, t->item->handler.old);
	else
		em_format_part_as(t->format, t->stream, t->part, NULL);
}

void
org_gnome_prefer_plain_multipart_alternative(void *ep, EMFormatHookTarget *t)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)t->part);
	CamelMimePart *part, *display_part = NULL;
	int i, nparts, partidlen, displayid = 0;

	if (epp_mode == EPP_NORMAL) {
		t->item->handler.old->handler(t->format, t->stream, t->part, t->item->handler.old);
		return;
	} else if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(t->format, t->stream, t->part);
		return;
	}
	
	nparts = camel_multipart_get_number(mp);	
	for (i=0; i<nparts; i++) {
		part = camel_multipart_get_part(mp, i);
		if (camel_content_type_is(camel_mime_part_get_content_type(part), "text", "plain")) {
			displayid = i;
			display_part = part;
			break;
		}
	}

	/* this part-id stuff is poking private data, needs api */
	partidlen = t->format->part_id->len;

	/* if we found a text part, show it */
	if (display_part) {
		g_string_append_printf(t->format->part_id, ".alternative.%d", displayid);
		em_format_part_as(t->format, t->stream, display_part, "text/plain");
		g_string_truncate(t->format->part_id, partidlen);
	}

	/* all other parts are attachments */
	for (i=0;i<nparts; i++) {
		part = camel_multipart_get_part(mp, i);
		if (part != display_part) {
			g_string_append_printf(t->format->part_id, ".alternative.%d", i);

			em_format_part_as(t->format, t->stream, t->part, NULL);

			g_string_truncate(t->format->part_id, partidlen);
		}
	}

	g_string_truncate(t->format->part_id, partidlen);
}

static struct {
	const char *label;
	const char *key;
} epp_options[] = {
	{ N_("Show HTML if present"), "normal" },
	{ N_("Prefer PLAIN"), "prefer_plain" },
	{ N_("Only ever show PLAIN"), "only_plain" },
};

static void
epp_mode_changed(GtkComboBox *dropdown, void *dummy)
{
	epp_mode = gtk_combo_box_get_active(dropdown);
	if (epp_mode > 2)
		epp_mode = 0;

	gconf_client_set_string(epp_gconf, "/apps/evolution/eplugin/prefer_plain/mode", epp_options[epp_mode].key, NULL);
}

GtkWidget *
org_gnome_prefer_plain_config_mode(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	/*EMConfigTargetPrefs *ep = (EMConfigTargetPrefs *)data->target;*/
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkWidget *w;
	int i;
	GtkTreeIter iter;

	if (data->old)
		return data->old;

	dropdown = (GtkComboBox *)gtk_combo_box_new();
	cell = gtk_cell_renderer_text_new();
	store = gtk_list_store_new(1, G_TYPE_STRING);
	for (i=0;i<sizeof(epp_options)/sizeof(epp_options[0]);i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _(epp_options[i].label), -1);
	}

	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	/*gtk_combo_box_set_active(dropdown, -1);*/
	gtk_combo_box_set_active(dropdown, epp_mode);
	g_signal_connect(dropdown, "changed", G_CALLBACK(epp_mode_changed), NULL);
	gtk_widget_show((GtkWidget *)dropdown);

	w = gtk_label_new(_("HTML Mode"));
	gtk_widget_show(w);

	i = ((GtkTable *)data->parent)->nrows;
	gtk_table_attach((GtkTable *)data->parent, w, 0, 1, i, i+1, 0, 0, 0, 0);
	gtk_table_attach((GtkTable *)data->parent, (GtkWidget *)dropdown, 1, 2, i, i+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);

	/* since this isnt dynamic, we don't need to track each item */

	return (GtkWidget *)dropdown;
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	char *key;
	int i;

	if (enable) {
		epp_gconf = gconf_client_get_default();
		key = gconf_client_get_string(epp_gconf, "/apps/evolution/eplugin/prefer_plain/mode", NULL);
		if (key) {
			for (i=0;i<sizeof(epp_options)/sizeof(epp_options[0]);i++) {
				if (!strcmp(epp_options[i].key, key)) {
					epp_mode = i;
					break;
				}
			}
		} else {
			epp_mode = 0;
		}
	} else {
		if (epp_gconf) {
			g_object_unref(epp_gconf);
			epp_gconf = 0;
		}
	}

	return 0;
}
