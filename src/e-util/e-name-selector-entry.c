/* e-name-selector-entry.c - Single-line text entry widget for EDestinations.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Authors: Hans Petter Jansson <hpj@novell.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include "e-name-selector-entry.h"

struct _ENameSelectorEntryPrivate {
	EClientCache *client_cache;
	gint minimum_query_length;
	gboolean show_address;

	PangoAttrList *attr_list;
	EContactStore *contact_store;
	ETreeModelGenerator *email_generator;
	EDestinationStore *destination_store;
	GtkEntryCompletion *entry_completion;

	guint type_ahead_complete_cb_id;
	guint update_completions_cb_id;

	EDestination *popup_destination;

	gpointer	(*contact_editor_func)	(EBookClient *,
						 EContact *,
						 gboolean,
						 gboolean);
	gpointer	(*contact_list_editor_func)
						(EBookClient *,
						 EContact *,
						 gboolean,
						 gboolean);

	gboolean is_completing;

	/* For asynchronous operations. */
	GQueue cancellables;

	GHashTable *known_contacts; /* gchar * ~> 1 */

	gboolean block_entry_changed_signal;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_MINIMUM_QUERY_LENGTH,
	PROP_SHOW_ADDRESS
};

enum {
	UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
#define ENS_DEBUG(x)

G_DEFINE_TYPE_WITH_CODE (ENameSelectorEntry, e_name_selector_entry, GTK_TYPE_ENTRY,
	G_ADD_PRIVATE (ENameSelectorEntry)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

/* 1/3 of the second to wait until invoking autocomplete lookup */
#define AUTOCOMPLETE_TIMEOUT 333

/* 1/20 of a second to wait until show the completion results */
#define SHOW_RESULT_TIMEOUT 50

#define re_set_timeout(id,func,ptr,tout) G_STMT_START { \
	if (id) \
		g_source_remove (id); \
	id = e_named_timeout_add (tout, func, ptr); \
	} G_STMT_END

static void destination_row_inserted (ENameSelectorEntry *name_selector_entry, GtkTreePath *path, GtkTreeIter *iter);
static void destination_row_changed  (ENameSelectorEntry *name_selector_entry, GtkTreePath *path, GtkTreeIter *iter);
static void destination_row_deleted  (ENameSelectorEntry *name_selector_entry, GtkTreePath *path);

static void user_insert_text (ENameSelectorEntry *name_selector_entry, const gchar *in_new_text, gint in_new_text_length, gint *position, gpointer user_data);
static void user_delete_text (ENameSelectorEntry *name_selector_entry, gint start_pos, gint end_pos, gpointer user_data);

static void setup_default_contact_store (ENameSelectorEntry *name_selector_entry);
static void deep_free_list (GList *list);

static void
remove_completion_timeout_sources (ENameSelectorEntry *self)
{
	if (self->priv->type_ahead_complete_cb_id) {
		g_source_remove (self->priv->type_ahead_complete_cb_id);
		self->priv->type_ahead_complete_cb_id = 0;
	}

	if (self->priv->update_completions_cb_id) {
		g_source_remove (self->priv->update_completions_cb_id);
		self->priv->update_completions_cb_id = 0;
	}
}

static void
name_selector_entry_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			e_name_selector_entry_set_client_cache (
				E_NAME_SELECTOR_ENTRY (object),
				g_value_get_object (value));
			return;

		case PROP_MINIMUM_QUERY_LENGTH:
			e_name_selector_entry_set_minimum_query_length (
				E_NAME_SELECTOR_ENTRY (object),
				g_value_get_int (value));
			return;

		case PROP_SHOW_ADDRESS:
			e_name_selector_entry_set_show_address (
				E_NAME_SELECTOR_ENTRY (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
name_selector_entry_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_name_selector_entry_ref_client_cache (
				E_NAME_SELECTOR_ENTRY (object)));
			return;

		case PROP_MINIMUM_QUERY_LENGTH:
			g_value_set_int (
				value,
				e_name_selector_entry_get_minimum_query_length (
				E_NAME_SELECTOR_ENTRY (object)));
			return;

		case PROP_SHOW_ADDRESS:
			g_value_set_boolean (
				value,
				e_name_selector_entry_get_show_address (
				E_NAME_SELECTOR_ENTRY (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
name_selector_entry_dispose (GObject *object)
{
	ENameSelectorEntry *self = E_NAME_SELECTOR_ENTRY (object);

	remove_completion_timeout_sources (self);
	gtk_editable_set_position (GTK_EDITABLE (self), 0);

	g_clear_object (&self->priv->client_cache);
	g_clear_pointer (&self->priv->attr_list, pango_attr_list_unref);
	g_clear_object (&self->priv->entry_completion);
	g_clear_object (&self->priv->destination_store);
	g_clear_object (&self->priv->email_generator);
	g_clear_object (&self->priv->contact_store);
	g_clear_pointer (&self->priv->known_contacts, g_hash_table_destroy);

	/* Cancel any stuck book loading operations. */
	while (!g_queue_is_empty (&self->priv->cancellables)) {
		GCancellable *cancellable;

		cancellable = g_queue_pop_head (&self->priv->cancellables);
		g_cancellable_cancel (cancellable);
		g_object_unref (cancellable);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_name_selector_entry_parent_class)->dispose (object);
}

static void
name_selector_entry_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_name_selector_entry_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
name_selector_entry_realize (GtkWidget *widget)
{
	ENameSelectorEntry *self = E_NAME_SELECTOR_ENTRY (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_name_selector_entry_parent_class)->realize (widget);

	if (self->priv->contact_store == NULL)
		setup_default_contact_store (self);
}

static void
name_selector_entry_drag_data_received (GtkWidget *widget,
                                        GdkDragContext *context,
                                        gint x,
                                        gint y,
                                        GtkSelectionData *selection_data,
                                        guint info,
                                        guint time)
{
	CamelInternetAddress *address;
	gint n_addresses = 0;
	gchar *text;

	address = camel_internet_address_new ();
	text = (gchar *) gtk_selection_data_get_text (selection_data);

	/* See if Camel can parse a valid email address from the text. */
	if (text != NULL && *text != '\0') {
		camel_url_decode (text);
		if (g_ascii_strncasecmp (text, "mailto:", 7) == 0)
			n_addresses = camel_address_decode (
				CAMEL_ADDRESS (address), text + 7);
		else
			n_addresses = camel_address_decode (
				CAMEL_ADDRESS (address), text);
	}

	if (n_addresses > 0) {
		GtkEditable *editable;
		GdkDragAction action;
		gboolean delete;
		gint position;

		editable = GTK_EDITABLE (widget);
		gtk_editable_set_position (editable, -1);
		position = gtk_editable_get_position (editable);

		g_free (text);

		text = camel_address_format (CAMEL_ADDRESS (address));
		gtk_editable_insert_text (editable, text, -1, &position);

		action = gdk_drag_context_get_selected_action (context);
		delete = (action == GDK_ACTION_MOVE);
		gtk_drag_finish (context, TRUE, delete, time);
	}

	g_object_unref (address);
	g_free (text);

	if (n_addresses <= 0)
		/* Chain up to parent's drag_data_received() method. */
		GTK_WIDGET_CLASS (e_name_selector_entry_parent_class)->
			drag_data_received (
				widget, context, x, y,
				selection_data, info, time);
}

static void
e_name_selector_entry_class_init (ENameSelectorEntryClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = name_selector_entry_set_property;
	object_class->get_property = name_selector_entry_get_property;
	object_class->dispose = name_selector_entry_dispose;
	object_class->constructed = name_selector_entry_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = name_selector_entry_realize;
	widget_class->drag_data_received = name_selector_entry_drag_data_received;

	/**
	 * ENameSelectorEntry:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MINIMUM_QUERY_LENGTH,
		g_param_spec_int (
			"minimum-query-length",
			"Minimum Query Length",
			NULL,
			1, G_MAXINT,
			3,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_ADDRESS,
		g_param_spec_boolean (
			"show-address",
			"Show Address",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[UPDATED] = g_signal_new (
		"updated",
		E_TYPE_NAME_SELECTOR_ENTRY,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ENameSelectorEntryClass, updated),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static gchar *
describe_contact (EContact *contact)
{
	GString *description;
	const gchar *str;
	GList *emails, *link;

	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	emails = e_contact_get (contact, E_CONTACT_EMAIL);
	/* Cannot merge one contact with multiple addresses with another contact */
	if (!e_contact_get (contact, E_CONTACT_IS_LIST) && emails && emails->next) {
		deep_free_list (emails);
		return NULL;
	}

	description = g_string_new ("");

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		g_string_append (description, "list\n");
	} else {
		g_string_append (description, "indv\n");
	}

	str = e_contact_get_const (contact, E_CONTACT_FILE_AS);
	g_string_append (description, str ? str : "");
	g_string_append_c (description, '\n');

	str = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
	g_string_append (description, str ? str : "");
	g_string_append_c (description, '\n');

	emails = g_list_sort (emails, (GCompareFunc) g_ascii_strcasecmp);
	for (link = emails; link; link = g_list_next (link)) {
		str = link->data;

		g_string_append (description, str ? str : "");
		g_string_append_c (description, '\n');
	}

	deep_free_list (emails);

	return g_string_free (description, FALSE);
}

static gchar *
describe_contact_source (EBookClient *client,
			 EContact *contact)
{
	return g_strdup_printf ("%p\n%s", client, (const gchar *) e_contact_get_const (contact, E_CONTACT_UID));
}

static gboolean
is_duplicate_contact_and_remember (ENameSelectorEntry *nsentry,
				   EBookClient *client,
				   EContact *contact)
{
	gchar *description;
	gchar *source_ident, *known_source_ident;

	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (nsentry), FALSE);
	g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);

	description = describe_contact (contact);
	if (!description) {
		/* Might be a contact with multiple addresses */
		return FALSE;
	}

	source_ident = describe_contact_source (client, contact);
	known_source_ident = g_hash_table_lookup (nsentry->priv->known_contacts, description);

	/* It's known, from the same book with the same UID */
	if (g_strcmp0 (known_source_ident, source_ident) == 0) {
		g_free (description);
		g_free (source_ident);
		return FALSE;
	}

	/* It's known, but from a different book or with a different UID - then skip it */
	if (known_source_ident) {
		g_free (description);
		g_free (source_ident);
		return TRUE;
	}

	g_hash_table_insert (nsentry->priv->known_contacts, description, source_ident);

	return FALSE;
}

/* Remove unquoted commas and control characters from string */
static gchar *
sanitize_string (const gchar *string)
{
	GString     *gstring;
	gboolean     quoted = FALSE;
	const gchar *p;

	gstring = g_string_new ("");

	if (!string)
		return g_string_free (gstring, FALSE);

	for (p = string; *p; p = g_utf8_next_char (p)) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"')
			quoted = ~quoted;
		else if (c == ',' && !quoted)
			continue;
		else if (c == '\t' || c == '\n')
			continue;

		g_string_append_unichar (gstring, c);
	}

	return g_string_free (gstring, FALSE);
}

/* Called for each list store entry whenever the user types (but not on cut/paste) */
static gboolean
completion_match_cb (GtkEntryCompletion *completion,
                     const gchar *key,
                     GtkTreeIter *iter,
                     gpointer user_data)
{
	ENS_DEBUG (g_print ("completion_match_cb, key=%s\n", key));

	return TRUE;
}

/* Gets context of n_unichars total (n_unicars / 2, before and after position)
 * and places them in array. If any positions would be outside the string, the
 * corresponding unichars are set to zero. */
static void
get_utf8_string_context (const gchar *string,
                         gint position,
                         gunichar *unichars,
                         gint n_unichars)
{
	gchar *p = NULL;
	gint   len;
	gint   gap;
	gint   i;

	/* n_unichars must be even */
	g_return_if_fail (n_unichars % 2 == 0);

	len = g_utf8_strlen (string, -1);
	gap = n_unichars / 2;

	for (i = 0; i < n_unichars; i++) {
		gint char_pos = position - gap + i;

		if (char_pos < 0 || char_pos >= len) {
			unichars[i] = '\0';
			continue;
		}

		if (p)
			p = g_utf8_next_char (p);
		else
			p = g_utf8_offset_to_pointer (string, char_pos);

		unichars[i] = g_utf8_get_char (p);
	}
}

static gboolean
get_range_at_position (const gchar *string,
                       gint pos,
                       gint *start_pos,
                       gint *end_pos)
{
	const gchar *p;
	gboolean     quoted = FALSE;
	gint         local_start_pos = 0;
	gint         local_end_pos = 0;
	gint         i;

	if (!string || !*string)
		return FALSE;

	for (p = string, i = 0; *p; p = g_utf8_next_char (p), i++) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"') {
			quoted = ~quoted;
		} else if (c == ',' && !quoted) {
			if (i < pos) {
				/* Start right after comma */
				local_start_pos = i + 1;
			} else {
				/* Stop right before comma */
				local_end_pos = i;
				break;
			}
		} else if (c == ' ' && local_start_pos == i) {
			/* Adjust start to skip space after first comma */
			local_start_pos++;
		}
	}

	/* If we didn't hit a comma, we must've hit NULL, and ours was the last element. */
	if (!local_end_pos)
		local_end_pos = i;

	if (start_pos)
		*start_pos = local_start_pos;
	if (end_pos)
		*end_pos   = local_end_pos;

	return TRUE;
}

static gboolean
is_quoted_at (const gchar *string,
              gint pos)
{
	const gchar *p;
	gboolean     quoted = FALSE;
	gint         i;

	for (p = string, i = 0; *p && i < pos; p = g_utf8_next_char (p), i++) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"')
			quoted = !quoted;
	}

	return quoted;
}

static gint
get_index_at_position (const gchar *string,
                       gint pos)
{
	const gchar *p;
	gboolean     quoted = FALSE;
	gint         n = 0;
	gint         i;

	for (p = string, i = 0; *p && i < pos; p = g_utf8_next_char (p), i++) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"')
			quoted = !quoted;
		else if (c == ',' && !quoted)
			n++;
	}

	return n;
}

static gboolean
get_range_by_index (const gchar *string,
                    gint index,
                    gint *start_pos,
                    gint *end_pos)
{
	const gchar *p;
	gboolean     quoted = FALSE;
	gint         i;
	gint         n = 0;

	for (p = string, i = 0; *p && n < index; p = g_utf8_next_char (p), i++) {
		gunichar c = g_utf8_get_char (p);

		if (c == '"')
			quoted = ~quoted;
		if (c == ',' && !quoted)
			n++;
	}

	if (n < index)
		return FALSE;

	return get_range_at_position (string, i, start_pos, end_pos);
}

static gchar *
get_address_at_position (const gchar *string,
                         gint pos)
{
	gint         start_pos;
	gint         end_pos;
	const gchar *start_p;
	const gchar *end_p;

	if (!get_range_at_position (string, pos, &start_pos, &end_pos))
		return NULL;

	start_p = g_utf8_offset_to_pointer (string, start_pos);
	end_p = g_utf8_offset_to_pointer (string, end_pos);

	return g_strndup (start_p, end_p - start_p);
}

/* Finds the destination in model */
static EDestination *
find_destination_by_index (ENameSelectorEntry *name_selector_entry,
                           gint index)
{
	GtkTreePath  *path;
	GtkTreeIter   iter;

	path = gtk_tree_path_new_from_indices (index, -1);
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (name_selector_entry->priv->destination_store),
				      &iter, path)) {
		/* If we have zero destinations, getting a NULL destination at index 0
		 * is valid. */
		if (index > 0)
			g_warning ("ENameSelectorEntry is out of sync with model!");
		gtk_tree_path_free (path);
		return NULL;
	}
	gtk_tree_path_free (path);

	return e_destination_store_get_destination (name_selector_entry->priv->destination_store, &iter);
}

/* Finds the destination in model */
static EDestination *
find_destination_at_position (ENameSelectorEntry *name_selector_entry,
                              gint pos)
{
	const gchar  *text;
	gint          index;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	index = get_index_at_position (text, pos);

	return find_destination_by_index (name_selector_entry, index);
}

/* Builds destination from our text */
static EDestination *
build_destination_at_position (const gchar *string,
                               gint pos)
{
	EDestination *destination;
	gchar        *address;

	address = get_address_at_position (string, pos);
	if (!address)
		return NULL;

	destination = e_destination_new ();
	e_destination_set_raw (destination, address);

	g_free (address);
	return destination;
}

static gchar *
name_style_query (const gchar *field,
                  const gchar *value)
{
	gchar   *spaced_str;
	gchar   *comma_str;
	GString *out = g_string_new ("");
	gchar  **strv;

	spaced_str = sanitize_string (value);
	g_strstrip (spaced_str);

	strv = g_strsplit (spaced_str, " ", 0);

	if (strv[0] && strv[1]) {
		g_string_append (out, "(or ");
		comma_str = g_strjoinv (", ", strv);
	} else {
		comma_str = NULL;
	}

	g_string_append (out, " (contains ");
	e_sexp_encode_string (out, field);
	e_sexp_encode_string (out, spaced_str);
	g_string_append_c (out, ')');

	if (comma_str) {
		g_string_append (out, " (contains ");

		e_sexp_encode_string (out, field);
		g_strstrip (comma_str);
		e_sexp_encode_string (out, comma_str);
		g_string_append (out, "))");
	}

	g_free (spaced_str);
	g_free (comma_str);
	g_strfreev (strv);

	return g_string_free (out, FALSE);
}

static gchar *
escape_sexp_string (const gchar *string)
{
	GString *gstring;

	gstring = g_string_new ("");
	e_sexp_encode_string (gstring, string);

	return g_string_free (gstring, FALSE);
}

static void
set_completion_query (ENameSelectorEntry *name_selector_entry,
                      const gchar *cue_str)
{
	EBookQuery *book_query;
	gchar      *query_str;
	gchar      *encoded_cue_str;
	gchar      *full_name_query_str;
	gchar      *file_as_query_str;

	if (!name_selector_entry->priv->contact_store)
		return;

	if (!cue_str) {
		/* Clear the store */
		e_contact_store_set_query (name_selector_entry->priv->contact_store, NULL);
		return;
	}

	encoded_cue_str = escape_sexp_string (cue_str);
	full_name_query_str = name_style_query ("full_name", cue_str);
	file_as_query_str = name_style_query ("file_as",   cue_str);

	query_str = g_strdup_printf (
		"(or "
		" (contains \"nickname\"  %s) "
		" (contains \"email\"     %s) "
		" %s "
		" %s "
		")",
		encoded_cue_str, encoded_cue_str,
		full_name_query_str, file_as_query_str);

	g_free (file_as_query_str);
	g_free (full_name_query_str);
	g_free (encoded_cue_str);

	ENS_DEBUG (g_print ("%s\n", query_str));

	book_query = e_book_query_from_string (query_str);
	e_contact_store_set_query (name_selector_entry->priv->contact_store, book_query);
	e_book_query_unref (book_query);

	g_free (query_str);
}

static gchar *
get_entry_substring (ENameSelectorEntry *name_selector_entry,
                     gint range_start,
                     gint range_end)
{
	const gchar *entry_text;
	gchar       *p0, *p1;

	entry_text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));

	p0 = g_utf8_offset_to_pointer (entry_text, range_start);
	p1 = g_utf8_offset_to_pointer (entry_text, range_end);

	return g_strndup (p0, p1 - p0);
}

static gint
utf8_casefold_collate_len (const gchar *str1,
                           const gchar *str2,
                           gint len)
{
	gchar *s1 = g_utf8_casefold (str1, len);
	gchar *s2 = g_utf8_casefold (str2, len);
	gint rv;

	rv = g_utf8_collate (s1, s2);

	g_free (s1);
	g_free (s2);

	return rv;
}

static gchar *
build_textrep_for_contact (EContact *contact,
                           EContactField cue_field,
                           gint email_num)
{
	gchar *name = NULL;
	gchar *email = NULL;
	gchar *textrep;
	GList *l;

	switch (cue_field) {
		case E_CONTACT_FULL_NAME:
		case E_CONTACT_NICKNAME:
		case E_CONTACT_FILE_AS:
			name = e_contact_get (contact, cue_field);
			email = e_contact_get (contact, E_CONTACT_EMAIL_1);
			break;

		case E_CONTACT_EMAIL:
			name = NULL;
			l = e_contact_get (contact, cue_field);
			email = g_strdup (g_list_nth_data (l, email_num));
			g_list_free_full (l, g_free);
			break;

		default:
			g_return_val_if_reached (NULL);
			break;
	}

	if (email && *email) {
		if (name)
			textrep = g_strdup_printf ("%s <%s>", name, email);
		else
			textrep = g_strdup_printf ("%s", email);
	} else {
		textrep = NULL;
		g_warn_if_fail (email != NULL);
		if (email)
			g_warn_if_fail (*email != '\0');
	}

	g_free (name);
	g_free (email);

	return textrep;
}

static gboolean
contact_match_cue (ENameSelectorEntry *name_selector_entry,
                   EContact *contact,
                   const gchar *cue_str,
                   EContactField *matched_field,
                   gint *matched_field_rank,
                   gint *matched_email_num)
{
	EContactField  fields[] = { E_CONTACT_FULL_NAME, E_CONTACT_NICKNAME, E_CONTACT_FILE_AS,
				    E_CONTACT_EMAIL };
	gchar         *email;
	gboolean       result = FALSE;
	gint           cue_len;
	gint           i;

	g_return_val_if_fail (contact, FALSE);
	g_return_val_if_fail (cue_str, FALSE);

	if (g_utf8_strlen (cue_str, -1) < name_selector_entry->priv->minimum_query_length)
		return FALSE;

	cue_len = strlen (cue_str);

	/* Make sure contact has an email address */
	email = e_contact_get (contact, E_CONTACT_EMAIL_1);
	if (!email || !*email) {
		g_free (email);
		return FALSE;
	}
	g_free (email);

	for (i = 0; i < G_N_ELEMENTS (fields) && result == FALSE; i++) {
		gint   email_num;
		gchar *value;
		gchar *value_sane;
		GList *emails = NULL, *ll = NULL;

		/* Don't match e-mail addresses in contact lists */
		if (e_contact_get (contact, E_CONTACT_IS_LIST) &&
		    fields[i] == E_CONTACT_EMAIL)
			continue;

		if (fields[i] == E_CONTACT_EMAIL) {
			emails = e_contact_get (contact, fields[i]);
		} else {
			value = e_contact_get (contact, fields[i]);
			if (!value)
				continue;
			emails = g_list_append (emails, value);
		}

		for (ll = emails, email_num = 0; ll; ll = ll->next, email_num++) {
			value = ll->data;
			value_sane = sanitize_string (value);

			ENS_DEBUG (g_print ("Comparing '%s' to '%s'\n", value, cue_str));

			if (!utf8_casefold_collate_len (value_sane, cue_str, cue_len)) {
				if (matched_field)
					*matched_field = fields[i];
				if (matched_field_rank)
					*matched_field_rank = i;
				if (matched_email_num)
					*matched_email_num = email_num;

				result = TRUE;
				g_free (value_sane);
				break;
			}
			g_free (value_sane);
		}
		g_list_free_full (emails, g_free);
	}

	return result;
}

static gboolean
find_existing_completion (ENameSelectorEntry *name_selector_entry,
                          const gchar *cue_str,
                          EContact **contact,
                          gchar **text,
                          EContactField *matched_field,
                          gint *matched_email_num,
                          EBookClient **book_client)
{
	GtkTreeIter    iter;
	EContact      *best_contact = NULL;
	gint           best_field_rank = G_MAXINT;
	EContactField  best_field = 0;
	gint           best_email_num = -1;
	EBookClient   *best_book_client = NULL;

	g_return_val_if_fail (cue_str, FALSE);

	if (!name_selector_entry->priv->contact_store)
		return FALSE;

	ENS_DEBUG (g_print ("Completing '%s'\n", cue_str));

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (name_selector_entry->priv->contact_store), &iter))
		return FALSE;

	do {
		EContact      *current_contact;
		gint           current_field_rank = best_field_rank;
		gint           current_email_num = best_email_num;
		EContactField  current_field = best_field;
		gboolean       matches;

		current_contact = e_contact_store_get_contact (name_selector_entry->priv->contact_store, &iter);
		if (!current_contact)
			continue;

		matches = contact_match_cue (name_selector_entry, current_contact, cue_str, &current_field, &current_field_rank, &current_email_num);
		if (matches && current_field_rank < best_field_rank) {
			best_contact = current_contact;
			best_field_rank = current_field_rank;
			best_field = current_field;
			best_book_client = e_contact_store_get_client (name_selector_entry->priv->contact_store, &iter);
			best_email_num = current_email_num;
		}

	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (name_selector_entry->priv->contact_store), &iter));

	if (!best_contact)
		return FALSE;

	if (contact)
		*contact = best_contact;
	if (text)
		*text = build_textrep_for_contact (best_contact, best_field, best_email_num);
	if (matched_field)
		*matched_field = best_field;
	if (book_client)
		*book_client = best_book_client;
	if (matched_email_num)
		*matched_email_num = best_email_num;
	return TRUE;
}

static void
generate_attribute_list (ENameSelectorEntry *name_selector_entry)
{
	PangoLayout    *layout;
	PangoAttrList  *attr_list;
	const gchar    *text;
	gint            i;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	layout = gtk_entry_get_layout (GTK_ENTRY (name_selector_entry));

	/* Set up the attribute list */

	attr_list = pango_attr_list_new ();

	if (name_selector_entry->priv->attr_list)
		pango_attr_list_unref (name_selector_entry->priv->attr_list);

	name_selector_entry->priv->attr_list = attr_list;

	/* Parse the entry's text and apply attributes to real contacts */

	for (i = 0; ; i++) {
		EDestination   *destination;
		PangoAttribute *attr;
		gint            start_pos;
		gint            end_pos;

		if (!get_range_by_index (text, i, &start_pos, &end_pos))
			break;

		destination = find_destination_at_position (name_selector_entry, start_pos);

		/* Destination will be NULL if we have no entries */
		if (!destination || !e_destination_get_contact (destination))
			continue;

		attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
		attr->start_index = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attr_list, attr);
	}

	pango_layout_set_attributes (layout, attr_list);
}

static gboolean
draw_event (ENameSelectorEntry *name_selector_entry)
{
	PangoLayout *layout;

	layout = gtk_entry_get_layout (GTK_ENTRY (name_selector_entry));
	pango_layout_set_attributes (layout, name_selector_entry->priv->attr_list);

	return FALSE;
}

static void
type_ahead_complete (ENameSelectorEntry *name_selector_entry)
{
	EContact      *contact;
	EBookClient   *book_client = NULL;
	EContactField  matched_field = E_CONTACT_FIELD_LAST;
	EDestination  *destination;
	gint           matched_email_num = -1;
	gint           cursor_pos;
	gint           range_start = 0;
	gint           range_end = 0;
	gint           pos = 0;
	gchar         *textrep;
	gint           textrep_len;
	gint           range_len;
	const gchar   *text;
	gchar         *cue_str;
	gchar         *temp_str;
	GtkEntryCompletion *completion;

	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (name_selector_entry));
	if (cursor_pos < 0)
		return;

	completion = gtk_entry_get_completion (GTK_ENTRY (name_selector_entry));
	if (completion)
		gtk_entry_completion_complete (completion);

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	get_range_at_position (text, cursor_pos, &range_start, &range_end);
	range_len = range_end - range_start;
	if (range_len < name_selector_entry->priv->minimum_query_length)
		return;

	destination = find_destination_at_position (name_selector_entry, cursor_pos);

	cue_str = get_entry_substring (name_selector_entry, range_start, range_end);
	if (!find_existing_completion (name_selector_entry, cue_str, &contact,
				       &textrep, &matched_field, &matched_email_num, &book_client)) {
		g_free (cue_str);
		return;
	}

	temp_str = sanitize_string (textrep);
	g_free (textrep);
	textrep = temp_str;

	textrep_len = g_utf8_strlen (textrep, -1);
	pos = range_start;

	g_signal_handlers_block_by_func (
		name_selector_entry,
		user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (
		name_selector_entry,
		user_delete_text, name_selector_entry);
	g_signal_handlers_block_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_changed, name_selector_entry);

	if (textrep_len > range_len) {
		gint i;

		/* keep character's case as user types */
		for (i = 0; textrep[i] && cue_str[i]; i++)
			textrep[i] = cue_str[i];

		gtk_editable_delete_text (
			GTK_EDITABLE (name_selector_entry),
			range_start, range_end);
		gtk_editable_insert_text (
			GTK_EDITABLE (name_selector_entry),
			textrep, -1, &pos);
		gtk_editable_select_region (
			GTK_EDITABLE (name_selector_entry),
			range_end, range_start + textrep_len);
		name_selector_entry->priv->is_completing = TRUE;
	}
	g_free (cue_str);

	if (contact && destination) {
		gint email_n = 0;

		if (matched_field == E_CONTACT_EMAIL)
			email_n = matched_email_num;
		e_destination_set_contact (destination, contact, email_n);
		if (book_client)
			e_destination_set_client (destination, book_client);
		generate_attribute_list (name_selector_entry);
	}

	g_signal_handlers_unblock_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_changed, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	g_free (textrep);
}

static void
clear_completion_model (ENameSelectorEntry *name_selector_entry)
{
	if (!name_selector_entry->priv->contact_store)
		return;

	e_contact_store_set_query (name_selector_entry->priv->contact_store, NULL);
	g_hash_table_remove_all (name_selector_entry->priv->known_contacts);
	name_selector_entry->priv->is_completing = FALSE;
}

static void
update_completion_model (ENameSelectorEntry *name_selector_entry)
{
	const gchar *text;
	gint         cursor_pos;
	gint         range_start = 0;
	gint         range_end = 0;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (name_selector_entry));

	if (cursor_pos >= 0)
		get_range_at_position (text, cursor_pos, &range_start, &range_end);

	if (range_end - range_start >= name_selector_entry->priv->minimum_query_length && cursor_pos == range_end) {
		gchar *cue_str;

		cue_str = get_entry_substring (name_selector_entry, range_start, range_end);
		set_completion_query (name_selector_entry, cue_str);
		g_free (cue_str);

		g_hash_table_remove_all (name_selector_entry->priv->known_contacts);
	} else {
		/* N/A; Clear completion model */
		clear_completion_model (name_selector_entry);
	}
}

static gboolean
type_ahead_complete_on_timeout_cb (gpointer user_data)
{
	ENameSelectorEntry *name_selector_entry;

	name_selector_entry = E_NAME_SELECTOR_ENTRY (user_data);
	type_ahead_complete (name_selector_entry);
	name_selector_entry->priv->type_ahead_complete_cb_id = 0;

	return FALSE;
}

static gboolean
update_completions_on_timeout_cb (gpointer user_data)
{
	ENameSelectorEntry *name_selector_entry;

	name_selector_entry = E_NAME_SELECTOR_ENTRY (user_data);
	update_completion_model (name_selector_entry);
	name_selector_entry->priv->update_completions_cb_id = 0;

	return FALSE;
}

static void
insert_destination_at_position (ENameSelectorEntry *name_selector_entry,
                                gint pos)
{
	EDestination *destination;
	const gchar  *text;
	gint          index;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	index = get_index_at_position (text, pos);

	destination = build_destination_at_position (text, pos);
	g_return_if_fail (destination);

	g_signal_handlers_block_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_inserted, name_selector_entry);
	e_destination_store_insert_destination (
		name_selector_entry->priv->destination_store,
						index, destination);
	g_signal_handlers_unblock_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_inserted, name_selector_entry);
	g_object_unref (destination);
}

static gboolean
modify_destination_at_position (ENameSelectorEntry *name_selector_entry,
                                gint pos)
{
	EDestination *destination;
	const gchar  *text;
	gchar        *raw_address;
	gboolean      rebuild_attributes = FALSE;

	destination = find_destination_at_position (name_selector_entry, pos);
	if (!destination)
		return FALSE;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	raw_address = get_address_at_position (text, pos);
	g_return_val_if_fail (raw_address, FALSE);

	if (e_destination_get_contact (destination))
		rebuild_attributes = TRUE;

	g_signal_handlers_block_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_changed, name_selector_entry);
	e_destination_set_raw (destination, raw_address);
	g_signal_handlers_unblock_by_func (
		name_selector_entry->priv->destination_store,
		destination_row_changed, name_selector_entry);

	g_free (raw_address);

	if (rebuild_attributes)
		generate_attribute_list (name_selector_entry);

	return TRUE;
}

static gchar *
get_destination_textrep (ENameSelectorEntry *name_selector_entry,
                         EDestination *destination)
{
	gboolean show_email = e_name_selector_entry_get_show_address (name_selector_entry);
	EContact *contact;

	g_return_val_if_fail (destination != NULL, NULL);

	contact = e_destination_get_contact (destination);

	if (!show_email) {
		if (contact && !e_contact_get (contact, E_CONTACT_IS_LIST)) {
			GList *email_list;

			email_list = e_contact_get (contact, E_CONTACT_EMAIL);
			show_email = g_list_length (email_list) > 1;
			deep_free_list (email_list);
		}
	}

	/* do not show emails for contact lists even user forces it */
	if (show_email && contact && e_contact_get (contact, E_CONTACT_IS_LIST))
		show_email = FALSE;

	return sanitize_string (e_destination_get_textrep (destination, show_email));
}

static void
sync_destination_at_position (ENameSelectorEntry *name_selector_entry,
                              gint range_pos,
                              gint *cursor_pos)
{
	EDestination *destination;
	const gchar  *text;
	gchar        *address;
	gint          address_len;
	gint          range_start, range_end;
	gint sel_start_pos = -1, sel_end_pos = -1;

	/* Get the destination we're looking at. Note that the entry may be empty, and so
	 * there may not be one. */
	destination = find_destination_at_position (name_selector_entry, range_pos);
	if (!destination)
		return;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	if (!get_range_at_position (text, range_pos, &range_start, &range_end)) {
		g_warning ("ENameSelectorEntry is out of sync with model!");
		return;
	}

	address = get_destination_textrep (name_selector_entry, destination);
	address_len = g_utf8_strlen (address, -1);

	if (cursor_pos) {
		/* Update cursor placement */
		if (*cursor_pos >= range_end)
			*cursor_pos += address_len - (range_end - range_start);
		else if (*cursor_pos > range_start)
			*cursor_pos = range_start + address_len;
	}

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	gtk_editable_get_selection_bounds (GTK_EDITABLE (name_selector_entry), &sel_start_pos, &sel_end_pos);

	gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), range_start, range_end);
	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), address, -1, &range_start);

	if (sel_start_pos >= 0 && sel_end_pos >= 0)
		gtk_editable_select_region (GTK_EDITABLE (name_selector_entry), sel_start_pos, sel_end_pos);

	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	generate_attribute_list (name_selector_entry);
	g_free (address);
}

static void
remove_destination_by_index (ENameSelectorEntry *name_selector_entry,
                             gint index)
{
	EDestination *destination;

	destination = find_destination_by_index (name_selector_entry, index);
	if (destination) {
		g_signal_handlers_block_by_func (
			name_selector_entry->priv->destination_store,
			destination_row_deleted, name_selector_entry);
		e_destination_store_remove_destination (
			name_selector_entry->priv->destination_store,
						destination);
		g_signal_handlers_unblock_by_func (
			name_selector_entry->priv->destination_store,
			destination_row_deleted, name_selector_entry);
	}
}

static void
post_insert_update (ENameSelectorEntry *name_selector_entry,
                    gint position)
{
	const gchar *text;
	glong length;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	length = g_utf8_strlen (text, -1);
	text = g_utf8_next_char (text);

	if (!*text)
		position = 0;

	/* Modified an existing destination or add a new. */
	if (!*text || !modify_destination_at_position (name_selector_entry, position)) {
		/* Create destination when it's the only character or when modify failed. */
		insert_destination_at_position (name_selector_entry, position);
	}

	/* If editing within the string, regenerate attributes. */
	if (position < length)
		generate_attribute_list (name_selector_entry);
}

/* Returns the number of characters inserted */
static gint
insert_unichar (ENameSelectorEntry *name_selector_entry,
		gint *pos,
		gunichar c,
		gboolean *inout_is_first_char)
{
	const gchar *text;
	gunichar     str_context[4];
	gchar        buf[7];
	gint         len;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	get_utf8_string_context (text, *pos, str_context, 4);

	/* Space is not allowed:
	 * - Before or after another space.
	 * - At start of string. */

	if (c == ' ' && *inout_is_first_char && str_context[0] && (str_context[1] == ' ' || str_context[1] == '\0' || str_context[2] == ' '))
		return 0;

	/* Comma is not allowed:
	 * - After another comma.
	 * - At start of string. */

	if (c == ',' && !is_quoted_at (text, *pos)) {
		gint         start_pos;
		gint         end_pos;
		gboolean     at_start = FALSE;
		gboolean     at_end = FALSE;

		if (str_context[1] == ',' || str_context[1] == '\0')
			return 0;

		/* We do this so we can avoid disturbing destinations with completed contacts
		 * either before or after the destination being inserted. */
		if (!get_range_at_position (text, *pos, &start_pos, &end_pos))
			return 0;
		if (*pos <= start_pos)
			at_start = TRUE;
		if (*pos >= end_pos)
			at_end = TRUE;

		/* Must insert comma first, so modify_destination_at_position can do its job
		 * correctly, splitting up the contact if necessary. */
		gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, pos);

		/* Update model */
		g_return_val_if_fail (*pos >= 2, 0);

		/* If we inserted the comma at the end of, or in the middle of, an existing
		 * address, add a new destination for what appears after comma. Else, we
		 * have to add a destination for what appears before comma (a blank one). */
		if (at_end) {
			/* End: Add last, sync first */
			insert_destination_at_position (name_selector_entry, *pos);
			sync_destination_at_position (name_selector_entry, *pos - 2, pos);
			/* Sync generates the attributes list */
		} else if (at_start) {
			/* Start: Add first */
			insert_destination_at_position (name_selector_entry, *pos - 2);
			generate_attribute_list (name_selector_entry);
		} else {
			/* Middle: */
			insert_destination_at_position (name_selector_entry, *pos);
			modify_destination_at_position (name_selector_entry, *pos - 2);
			generate_attribute_list (name_selector_entry);
		}

		*inout_is_first_char = TRUE;

		return 2;
	}

	/* Generic case. Allowed spaces also end up here. */

	*inout_is_first_char = FALSE;

	len = g_unichar_to_utf8 (c, buf);
	buf[len] = '\0';

	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), buf, -1, pos);

	post_insert_update (name_selector_entry, *pos);

	return 1;
}

static void
user_insert_text (ENameSelectorEntry *name_selector_entry,
                  const gchar *in_new_text,
                  gint in_new_text_length,
                  gint *position,
                  gpointer user_data)
{
	const gchar *text = in_new_text;
	gchar *tmp_text = NULL;
	gint text_length = in_new_text_length;
	gint chars_inserted = 0;
	gboolean fast_insert, has_focus;

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	/* these can be groups or Outlook-provided list of addresses with a ';' as
	   a delimiter, instead of ',', copied from the message body */
	if (g_utf8_strchr (text, text_length, ';') != NULL) {
		GString *prefer_text = NULL;
		GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
		const gchar *from = text;
		gboolean in_quote = FALSE;
		guint ii;

		if (text_length < 0)
			text_length = strlen (text);

		for (ii = 0; ii < text_length; ii++) {
			if (text[ii] == '\"') {
				in_quote = !in_quote;
			} else if (!in_quote && text[ii] == ';') {
				gchar *part = g_strstrip (g_strndup (from, text + ii - from));

				if (*part)
					g_ptr_array_add (parts, part);
				else
					g_free (part);

				from = text + ii + 1;
			}
		}

		if (from < text + text_length) {
			gchar *part = g_strstrip (g_strndup (from, text + text_length - from));

			if (*part)
				g_ptr_array_add (parts, part);
			else
				g_free (part);
		}

		for (ii = 0; ii < parts->len; ii++) {
			const gchar *addr = g_ptr_array_index (parts, ii);

			if (!strchr (addr, ':') && !strchr (addr, '\"')) {
				guint jj, has_addr_start = 0, has_addr_end = 0;
				gboolean in_addr = FALSE, broken = FALSE;

				for (jj = 0; addr[jj] && !broken; jj++) {
					gchar chr = addr[jj];

					if (chr == '<') {
						has_addr_start++;
						if (in_addr)
							broken = TRUE;
						else
							in_addr = TRUE;
					} else if (chr == '>') {
						if (in_addr)
							in_addr = FALSE;
						else
							broken = TRUE;
						has_addr_end++;
					}
				}

				broken = broken || in_addr || has_addr_start != 1 || has_addr_start != has_addr_end;

				if (broken) {
					if (!prefer_text) {
						prefer_text = g_string_new (addr);
					} else {
						g_string_append (prefer_text, ", ");
						g_string_append (prefer_text, addr);
					}
				} else {
					gchar *addr_start = strchr (addr, '<'), *name_end = addr_start;

					while (name_end > addr && g_ascii_isspace (name_end[-1]))
						name_end--;

					if (!prefer_text)
						prefer_text = g_string_new ("");
					else
						g_string_append (prefer_text, ", ");

					if (name_end > addr) {
						gboolean quote = g_utf8_strchr (addr, name_end - addr, ',') != NULL;

						if (quote)
							g_string_append_c (prefer_text, '\"');

						g_string_append_len (prefer_text, addr, name_end - addr);

						if (quote)
							g_string_append_c (prefer_text, '\"');

						g_string_append_c (prefer_text, ' ');
					}

					g_string_append (prefer_text, addr_start);
				}
			} else {
				if (!prefer_text) {
					prefer_text = g_string_new (addr);
				} else {
					g_string_append (prefer_text, ", ");
					g_string_append (prefer_text, addr);
				}
			}
		}

		g_ptr_array_free (parts, TRUE);

		if (prefer_text) {
			text_length = prefer_text->len;
			tmp_text = g_string_free (prefer_text, FALSE);
			text = tmp_text;
		}
	}

	fast_insert =
		(g_utf8_strchr (text, text_length, ' ') == NULL) &&
		(g_utf8_strchr (text, text_length, ',') == NULL) &&
		(g_utf8_strchr (text, text_length, '\t') == NULL) &&
		(g_utf8_strchr (text, text_length, '\n') == NULL);

	has_focus = gtk_widget_has_focus (GTK_WIDGET (name_selector_entry));

	if (!has_focus && *position && *position == gtk_entry_get_text_length (GTK_ENTRY (name_selector_entry))) {
		gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", 2, position);
		insert_destination_at_position (name_selector_entry, *position);
	}

	/* If the text to insert does not contain spaces or commas,
	 * insert all of it at once.  This avoids confusing on-going
	 * input method behavior. */
	if (fast_insert) {
		gint old_position = *position;

		gtk_editable_insert_text (
			GTK_EDITABLE (name_selector_entry),
			text, text_length, position);

		chars_inserted = *position - old_position;
		if (chars_inserted > 0)
			post_insert_update (name_selector_entry, *position);

	/* Otherwise, apply some rules as to where spaces and commas
	 * can be inserted, and insert a trailing space after comma. */
	} else {
		const gchar *cp;
		gboolean last_was_comma = FALSE, is_first_char = TRUE;

		for (cp = text; *cp; cp = g_utf8_next_char (cp)) {
			gunichar uc = g_utf8_get_char (cp);

			if (uc == '\n' || uc == '\t') {
				if (last_was_comma)
					continue;
				last_was_comma = TRUE;
				uc = ',';
			} else if (uc == '\r') {
				continue;
			} else {
				last_was_comma = uc == ',';
			}

			if (insert_unichar (name_selector_entry, position, uc, &is_first_char))
				chars_inserted++;
		}
	}

	if (chars_inserted >= 1 && has_focus) {
		/* If the user inserted one character, kick off completion */
		re_set_timeout (
			name_selector_entry->priv->update_completions_cb_id,
			update_completions_on_timeout_cb,  name_selector_entry,
			AUTOCOMPLETE_TIMEOUT);
		re_set_timeout (
			name_selector_entry->priv->type_ahead_complete_cb_id,
			type_ahead_complete_on_timeout_cb, name_selector_entry,
			AUTOCOMPLETE_TIMEOUT);
	}

	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	g_signal_stop_emission_by_name (name_selector_entry, "insert_text");

	g_free (tmp_text);
}

static void
user_delete_text (ENameSelectorEntry *name_selector_entry,
                  gint start_pos,
                  gint end_pos,
                  gpointer user_data)
{
	const gchar *text;
	gint         index_start, index_end;
	gint	     selection_start, selection_end;
	gunichar     str_context[2], str_b_context[2];
	gint         len;
	gint         i;
	gboolean     del_comma = FALSE;

	if (start_pos == end_pos)
		return;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	len = g_utf8_strlen (text, -1);

	if (end_pos == -1)
		end_pos = len;

	gtk_editable_get_selection_bounds (
		GTK_EDITABLE (name_selector_entry),
		&selection_start, &selection_end);

	get_utf8_string_context (text, start_pos, str_context, 2);
	get_utf8_string_context (text, end_pos, str_b_context, 2);

	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	if (end_pos - start_pos == 1) {
		/* Might be backspace; update completion model so dropdown is accurate */
		re_set_timeout (
			name_selector_entry->priv->update_completions_cb_id,
			update_completions_on_timeout_cb, name_selector_entry,
			AUTOCOMPLETE_TIMEOUT);
	}

	index_start = get_index_at_position (text, start_pos);
	index_end = get_index_at_position (text, end_pos);

	g_signal_stop_emission_by_name (name_selector_entry, "delete_text");

	/* If the deletion touches more than one destination, the first one is changed
	 * and the rest are removed. If the last destination wasn't completely deleted,
	 * it becomes part of the first one, since the separator between them was
	 * removed.
	 *
	 * Here, we let the model know about removals. */
	for (i = index_end; i > index_start; i--) {
		EDestination *destination = find_destination_by_index (name_selector_entry, i);
		gint range_start, range_end;
		gchar *ttext;
		const gchar *email = NULL;
		gboolean sel = FALSE;

		if (destination)
			email = e_destination_get_textrep (destination, TRUE);

		if (!email || !*email)
			continue;

		if (!get_range_by_index (text, i, &range_start, &range_end)) {
			g_warning ("ENameSelectorEntry is out of sync with model!");
			return;
		}

		if ((selection_start < range_start && selection_end > range_start) ||
		    (selection_end > range_start && selection_end < range_end))
			sel = TRUE;

		if (!sel) {
			g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
			g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

			gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), range_start, range_end);

			ttext = sanitize_string (email);
			gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ttext, -1, &range_start);
			g_free (ttext);

			g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
			g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

		}

		remove_destination_by_index (name_selector_entry, i);
	}

	/* Do the actual deletion */

	if (end_pos == start_pos +1 && index_end == index_start + 1) {
		/* We could be just deleting the empty text */
		gchar *c;

		/* Get the actual deleted text */
		c = gtk_editable_get_chars (GTK_EDITABLE (name_selector_entry), start_pos, start_pos + 1);

		if (c && c[0] == ',' && !is_quoted_at (text, start_pos)) {
			/* If we are at the beginning or removing junk space, let us ignore it */
			del_comma = TRUE;
		}
		g_free (c);
	}

	if (del_comma) {
		gint range_start=-1, range_end;
		EDestination *dest = find_destination_by_index (name_selector_entry, index_end);
		/* If we have deleted the last comma, let us autocomplete normally
		 */

		if (dest && len - end_pos != 0) {

			EDestination *destination1 = find_destination_by_index (name_selector_entry, index_start);
			gchar *ttext;
			const gchar *email = NULL;

			if (destination1)
				email = e_destination_get_textrep (destination1, TRUE);

			if (email && *email) {

				if (!get_range_by_index (text, i, &range_start, &range_end)) {
					g_warning ("ENameSelectorEntry is out of sync with model!");
					return;
				}

				g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
				g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

				gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), range_start, range_end);

				ttext = sanitize_string (email);
				gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ttext, -1, &range_start);
				g_free (ttext);

				g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
				g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);
			}

			if (range_start != -1) {
				start_pos = range_start;
				end_pos = start_pos + 1;
				gtk_editable_set_position (GTK_EDITABLE (name_selector_entry),start_pos);
			}
		}
	}
	gtk_editable_delete_text (
		GTK_EDITABLE (name_selector_entry),
		start_pos, end_pos);

	/*If the user is deleting a '"' new destinations have to be created for ',' between the quoted text
	 Like "fd,ty,uy" is a one entity, but if you remove the quotes it has to be broken doan into 3 seperate
	 addresses.
	*/

	if (str_b_context[1] == '"') {
		const gchar *p;
		gint j;
		for (p = text + (end_pos - 1), j = end_pos - 1; *p && *p != '"' ; p = g_utf8_next_char (p), j++) {
			gunichar c = g_utf8_get_char (p);
			if (c == ',') {
				insert_destination_at_position (name_selector_entry, j + 1);
			}
		}

	}

	/* Let model know about changes */
	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	if (!*text || strlen (text) <= 0) {
		/* If the entry was completely cleared, remove the initial destination too */
		remove_destination_by_index (name_selector_entry, 0);
		generate_attribute_list (name_selector_entry);
	} else {
		modify_destination_at_position (name_selector_entry, start_pos);
	}

	/* If editing within the string, we need to regenerate attributes */
	if (end_pos < len)
		generate_attribute_list (name_selector_entry);

	/* Prevent type-ahead completion */
	if (name_selector_entry->priv->type_ahead_complete_cb_id) {
		g_source_remove (name_selector_entry->priv->type_ahead_complete_cb_id);
		name_selector_entry->priv->type_ahead_complete_cb_id = 0;
	}

	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
}

static gboolean
completion_match_selected (ENameSelectorEntry *name_selector_entry,
                           ETreeModelGenerator *email_generator_model,
                           GtkTreeIter *generator_iter)
{
	EContact      *contact;
	EBookClient   *book_client;
	EDestination  *destination;
	gint           cursor_pos;
	GtkTreeIter    contact_iter;
	gint           email_n;

	if (!name_selector_entry->priv->contact_store)
		return FALSE;

	g_return_val_if_fail (name_selector_entry->priv->email_generator == email_generator_model, FALSE);

	e_tree_model_generator_convert_iter_to_child_iter (
		email_generator_model,
		&contact_iter, &email_n,
		generator_iter);

	contact = e_contact_store_get_contact (name_selector_entry->priv->contact_store, &contact_iter);
	book_client = e_contact_store_get_client (name_selector_entry->priv->contact_store, &contact_iter);
	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (name_selector_entry));

	/* Set the contact in the model's destination */

	destination = find_destination_at_position (name_selector_entry, cursor_pos);
	e_destination_set_contact (destination, contact, email_n);
	if (book_client)
		e_destination_set_client (destination, book_client);
	sync_destination_at_position (name_selector_entry, cursor_pos, &cursor_pos);

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, &cursor_pos);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	/*Add destination at end for next entry*/
	insert_destination_at_position (name_selector_entry, cursor_pos);
	/* Place cursor at end of address */

	gtk_editable_set_position (GTK_EDITABLE (name_selector_entry), cursor_pos);
	g_signal_emit (name_selector_entry, signals[UPDATED], 0, destination, NULL);
	return TRUE;
}

static void
entry_activate (ENameSelectorEntry *name_selector_entry)
{
	gint         cursor_pos;
	gint         range_start, range_end;
	EDestination  *destination;
	gint           range_len;
	const gchar   *text;
	gchar         *cue_str;

	cursor_pos = gtk_editable_get_position (GTK_EDITABLE (name_selector_entry));
	if (cursor_pos < 0)
		return;

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	if (!get_range_at_position (text, cursor_pos, &range_start, &range_end))
		return;

	range_len = range_end - range_start;
	if (range_len < name_selector_entry->priv->minimum_query_length)
		return;

	destination = find_destination_at_position (name_selector_entry, cursor_pos);
	if (!destination)
		return;

	cue_str = get_entry_substring (name_selector_entry, range_start, range_end);
#if 0
	if (!find_existing_completion (name_selector_entry, cue_str, &contact,
				       &textrep, &matched_field)) {
		g_free (cue_str);
		return;
	}
#endif
	g_free (cue_str);
	sync_destination_at_position (name_selector_entry, cursor_pos, &cursor_pos);

	/* Place cursor at end of address */
	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	get_range_at_position (text, cursor_pos, &range_start, &range_end);

	if (name_selector_entry->priv->is_completing) {
		gchar *str_context = NULL;

		str_context = gtk_editable_get_chars (GTK_EDITABLE (name_selector_entry), range_end, range_end + 1);

		if (str_context[0] != ',') {
			/* At the end*/
			gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, &range_end);
		} else {
			/* In the middle */
			gint newpos = strlen (text);

                        /* Doing this we can make sure that It wont ask for completion again. */
			gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, &newpos);
			g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);
			gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), newpos - 2, newpos);
			g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);

			/* Move it close to next destination*/
			range_end = range_end + 2;

		}
		g_free (str_context);
	}

	/* Set the position only if is completing or nothing is selected, because it also deselects any selection. */
	if (name_selector_entry->priv->is_completing || !gtk_editable_get_selection_bounds (GTK_EDITABLE (name_selector_entry), NULL, NULL))
		gtk_editable_set_position (GTK_EDITABLE (name_selector_entry), range_end);

	g_signal_emit (name_selector_entry, signals[UPDATED], 0, destination, NULL);

	if (name_selector_entry->priv->is_completing)
		clear_completion_model (name_selector_entry);
}

static void
update_text (ENameSelectorEntry *name_selector_entry,
             const gchar *text)
{
	gint start = -1, end = -1;

	gtk_editable_get_selection_bounds (GTK_EDITABLE (name_selector_entry), &start, &end);

	gtk_entry_set_text (GTK_ENTRY (name_selector_entry), text);

	if (start >= 0 && end >= 0)
		gtk_editable_select_region (GTK_EDITABLE (name_selector_entry), start, end);
}

static void
sanitize_entry (ENameSelectorEntry *name_selector_entry)
{
	gint n;
	GList *l, *known, *del = NULL;
	GString *str = g_string_new ("");

	g_signal_handlers_block_matched (name_selector_entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);
	g_signal_handlers_block_matched (name_selector_entry->priv->destination_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);

	known = e_destination_store_list_destinations (name_selector_entry->priv->destination_store);
	for (l = known, n = 0; l != NULL; l = l->next, n++) {
		EDestination *dest = l->data;

		if (!dest || !e_destination_get_address (dest))
			del = g_list_prepend (del, GINT_TO_POINTER (n));
		else {
			gchar *text;

			text = get_destination_textrep (name_selector_entry, dest);
			if (text) {
				if (str->str && str->str[0])
					g_string_append (str, ", ");

				g_string_append (str, text);
			}
			g_free (text);
		}
	}
	g_list_free (known);

	for (l = del; l != NULL; l = l->next) {
		e_destination_store_remove_destination_nth (name_selector_entry->priv->destination_store, GPOINTER_TO_INT (l->data));
	}
	g_list_free (del);

	update_text (name_selector_entry, str->str);

	g_string_free (str, TRUE);

	g_signal_handlers_unblock_matched (name_selector_entry->priv->destination_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);
	g_signal_handlers_unblock_matched (name_selector_entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);

	generate_attribute_list (name_selector_entry);
}

static void
maybe_block_entry_changed_cb (ENameSelectorEntry *name_selector_entry,
			      gpointer user_data)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));

	if (name_selector_entry->priv->block_entry_changed_signal)
		g_signal_stop_emission_by_name (name_selector_entry, "changed");
}

static gboolean
user_focus_in (ENameSelectorEntry *name_selector_entry,
               GdkEventFocus *event_focus)
{
	gint n;
	GList *l, *known;
	GString *str = g_string_new ("");
	gint sel_start_pos = -1, sel_end_pos = -1;

	/* To not send fake 'changed' signals, which can influence message composer */
	name_selector_entry->priv->block_entry_changed_signal = TRUE;
	g_signal_handlers_block_matched (name_selector_entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);
	g_signal_handlers_block_matched (name_selector_entry->priv->destination_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);

	known = e_destination_store_list_destinations (name_selector_entry->priv->destination_store);
	for (l = known, n = 0; l != NULL; l = l->next, n++) {
		EDestination *dest = l->data;

		if (dest) {
			gchar *text;

			text = get_destination_textrep (name_selector_entry, dest);
			if (text) {
				if (str->str && str->str[0])
					g_string_append (str, ", ");

				g_string_append (str, text);
			}
			g_free (text);
		}
	}
	g_list_free (known);

	if (str->len < 2 || (str->str && str->str[str->len - 1] != ' ' && str->str[str->len - 2] != ',')) {
		EDestination *dest_dummy = e_destination_new ();

		/* Add a blank destination */
		e_destination_store_append_destination (name_selector_entry->priv->destination_store, dest_dummy);
		if (str->str && str->str[0])
			g_string_append (str, ", ");

		g_clear_object (&dest_dummy);
	}

	gtk_editable_get_selection_bounds (GTK_EDITABLE (name_selector_entry), &sel_start_pos, &sel_end_pos);

	gtk_entry_set_text (GTK_ENTRY (name_selector_entry), str->str);

	if (sel_start_pos >= 0 && sel_end_pos >= 0)
		gtk_editable_select_region (GTK_EDITABLE (name_selector_entry), sel_start_pos, sel_end_pos);

	g_string_free (str, TRUE);

	g_signal_handlers_unblock_matched (name_selector_entry->priv->destination_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);
	g_signal_handlers_unblock_matched (name_selector_entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, name_selector_entry);
	name_selector_entry->priv->block_entry_changed_signal = FALSE;

	generate_attribute_list (name_selector_entry);

	return FALSE;
}

static gboolean
user_focus_out (ENameSelectorEntry *name_selector_entry,
                GdkEventFocus *event_focus)
{
	if (!event_focus->in) {
		entry_activate (name_selector_entry);
	}

	remove_completion_timeout_sources (name_selector_entry);
	clear_completion_model (name_selector_entry);

	if (!event_focus->in) {
		sanitize_entry (name_selector_entry);
	}

	return FALSE;
}

static gboolean
user_key_press_event_cb (ENameSelectorEntry *name_selector_entry,
			 GdkEventKey *event_key)
{
	gint end;

	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), FALSE);
	g_return_val_if_fail (event_key != NULL, FALSE);

	if ((event_key->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0 &&
	    event_key->keyval == GDK_KEY_comma &&
	    gtk_editable_get_selection_bounds (GTK_EDITABLE (name_selector_entry), NULL, &end)) {
		entry_activate (name_selector_entry);

		remove_completion_timeout_sources (name_selector_entry);
		clear_completion_model (name_selector_entry);

		sanitize_entry (name_selector_entry);

		gtk_editable_select_region (GTK_EDITABLE (name_selector_entry), end, end);
	}

	return FALSE;
}

static void
deep_free_list (GList *list)
{
	GList *l;

	for (l = list; l; l = g_list_next (l))
		g_free (l->data);

	g_list_free (list);
}

/* Given a widget, determines the height that text will normally be drawn. */
static guint
entry_height (GtkWidget *widget)
{
	PangoLayout *layout;
	gint bound;

	g_return_val_if_fail (widget != NULL, 0);

	layout = gtk_widget_create_pango_layout (widget, NULL);

	pango_layout_get_pixel_size (layout, NULL, &bound);

	return bound;
}

static void
contact_layout_pixbuffer (GtkCellLayout *cell_layout,
                          GtkCellRenderer *cell,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          ENameSelectorEntry *name_selector_entry)
{
	EContact      *contact;
	GtkTreeIter    generator_iter;
	GtkTreeIter    contact_store_iter;
	gint           email_n;
	EContactPhoto *photo;
	gboolean       iter_is_valid;
	GdkPixbuf *pixbuf = NULL;

	if (!name_selector_entry->priv->contact_store)
		return;

	gtk_tree_model_filter_convert_iter_to_child_iter (
		GTK_TREE_MODEL_FILTER (model),
		&generator_iter, iter);
	iter_is_valid = e_tree_model_generator_convert_iter_to_child_iter (
		name_selector_entry->priv->email_generator,
		&contact_store_iter, &email_n,
		&generator_iter);

	if (!iter_is_valid) {
		return;
	}

	contact = e_contact_store_get_contact (name_selector_entry->priv->contact_store, &contact_store_iter);
	if (!contact) {
		g_object_set (cell, "pixbuf", pixbuf, NULL);
		return;
	}

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		guint max_height = entry_height (GTK_WIDGET (name_selector_entry));
		GdkPixbufLoader *loader;

		loader = gdk_pixbuf_loader_new ();
		if (gdk_pixbuf_loader_write (loader, (guchar *) photo->data.inlined.data, photo->data.inlined.length, NULL) &&
		    gdk_pixbuf_loader_close (loader, NULL)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			if (pixbuf)
				g_object_ref (pixbuf);
		}
		g_object_unref (loader);

		if (pixbuf) {
			gint w, h;
			gdouble scale = 1.0;

			w = gdk_pixbuf_get_width (pixbuf);
			h = gdk_pixbuf_get_height (pixbuf);

			if (h > w)
				scale = max_height / (double) h;
			else
				scale = max_height / (double) w;

			if (scale < 1.0) {
				GdkPixbuf *tmp;

				tmp = gdk_pixbuf_scale_simple (pixbuf, w * scale, h * scale, GDK_INTERP_BILINEAR);
				g_object_unref (pixbuf);
				pixbuf = tmp;
			}

		}
	}

	e_contact_photo_free (photo);

	g_object_set (cell, "pixbuf", pixbuf, NULL);

	if (pixbuf)
		g_object_unref (pixbuf);
}

static void
contact_layout_formatter (GtkCellLayout *cell_layout,
                          GtkCellRenderer *cell,
                          GtkTreeModel *model,
                          GtkTreeIter *iter,
                          ENameSelectorEntry *name_selector_entry)
{
	EContact      *contact;
	GtkTreeIter    generator_iter;
	GtkTreeIter    contact_store_iter;
	GList         *email_list;
	gchar         *string;
	gchar         *file_as_str;
	gchar         *email_str;
	gint           email_n;
	gboolean       iter_is_valid;

	if (!name_selector_entry->priv->contact_store)
		return;

	gtk_tree_model_filter_convert_iter_to_child_iter (
		GTK_TREE_MODEL_FILTER (model),
		&generator_iter, iter);
	iter_is_valid = e_tree_model_generator_convert_iter_to_child_iter (
		name_selector_entry->priv->email_generator,
		&contact_store_iter, &email_n,
		&generator_iter);

	if (!iter_is_valid) {
		return;
	}

	contact = e_contact_store_get_contact (name_selector_entry->priv->contact_store, &contact_store_iter);
	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	email_str = g_list_nth_data (email_list, email_n);
	file_as_str = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		string = g_strdup_printf ("%s", file_as_str ? file_as_str : "?");
	} else {
		string = g_strdup_printf (
			"%s%s<%s>", file_as_str ? file_as_str : "",
			file_as_str ? " " : "",
			email_str ? email_str : "");
	}

	g_free (file_as_str);
	deep_free_list (email_list);

	g_object_set (cell, "text", string, NULL);
	g_free (string);
}

static gint
generate_contact_rows (EContactStore *contact_store,
                       GtkTreeIter *iter,
                       ENameSelectorEntry *name_selector_entry)
{
	EContact    *contact;
	const gchar *contact_uid;
	GList       *email_list;
	gint         n_rows;

	contact = e_contact_store_get_contact (contact_store, iter);
	g_return_val_if_fail (contact != NULL, 0);

	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);
	if (!contact_uid)
		return 0;  /* Can happen with broken databases */

	if (is_duplicate_contact_and_remember (name_selector_entry, e_contact_store_get_client (contact_store, iter), contact))
		return 0;

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		return 1;

	email_list = e_contact_get (contact, E_CONTACT_EMAIL);
	n_rows = g_list_length (email_list);
	deep_free_list (email_list);

	return n_rows;
}

static void
ensure_type_ahead_complete_on_timeout (ENameSelectorEntry *name_selector_entry)
{
	/* this is called whenever a new item is added to the model,
	 * thus, to not starve when there are many matches, do not
	 * postpone on each add, but show results as soon as possible */
	if (!name_selector_entry->priv->type_ahead_complete_cb_id) {
		re_set_timeout (
			name_selector_entry->priv->type_ahead_complete_cb_id,
			type_ahead_complete_on_timeout_cb, name_selector_entry,
			SHOW_RESULT_TIMEOUT);
	}
}

static void
setup_contact_store (ENameSelectorEntry *name_selector_entry)
{
	g_clear_object (&name_selector_entry->priv->email_generator);

	if (name_selector_entry->priv->contact_store) {
		name_selector_entry->priv->email_generator =
			e_tree_model_generator_new (
				GTK_TREE_MODEL (
				name_selector_entry->priv->contact_store));

		e_tree_model_generator_set_generate_func (
			name_selector_entry->priv->email_generator,
			(ETreeModelGeneratorGenerateFunc) generate_contact_rows,
			name_selector_entry, NULL);

		/* Assign the store to the entry completion */

		gtk_entry_completion_set_model (
			name_selector_entry->priv->entry_completion,
			GTK_TREE_MODEL (
			name_selector_entry->priv->email_generator));

		/* Set up callback for incoming matches */
		g_signal_connect_swapped (
			name_selector_entry->priv->contact_store, "row-inserted",
			G_CALLBACK (ensure_type_ahead_complete_on_timeout), name_selector_entry);
		g_signal_connect_swapped (
			name_selector_entry->priv->contact_store, "row-changed",
			G_CALLBACK (ensure_type_ahead_complete_on_timeout), name_selector_entry);
		g_signal_connect_swapped (
			name_selector_entry->priv->contact_store, "row-deleted",
			G_CALLBACK (ensure_type_ahead_complete_on_timeout), name_selector_entry);
	} else {
		/* Remove the store from the entry completion */

		gtk_entry_completion_set_model (name_selector_entry->priv->entry_completion, NULL);
	}
}

static void
name_selector_entry_get_client_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EContactStore *contact_store = user_data;
	EBookClient *book_client;
	EClient *client;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		goto exit;
	}

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	book_client = E_BOOK_CLIENT (client);

	g_return_if_fail (E_IS_BOOK_CLIENT (book_client));
	e_contact_store_add_client (contact_store, book_client);
	g_object_unref (book_client);

 exit:
	g_object_unref (contact_store);
}

static void
setup_default_contact_store (ENameSelectorEntry *name_selector_entry)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EContactStore *contact_store;
	GList *list, *iter;
	const gchar *extension_name;

	g_return_if_fail (name_selector_entry->priv->contact_store == NULL);

	/* Create a book for each completion source, and assign them to the contact store */

	contact_store = e_contact_store_new ();
	name_selector_entry->priv->contact_store = contact_store;

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	client_cache = e_name_selector_entry_ref_client_cache (name_selector_entry);
	registry = e_client_cache_ref_registry (client_cache);

	list = e_source_registry_list_enabled (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceAutocomplete *extension;
		GCancellable *cancellable;

		extension_name = E_SOURCE_EXTENSION_AUTOCOMPLETE;
		extension = e_source_get_extension (source, extension_name);

		/* Skip non-completion address books. */
		if (!e_source_autocomplete_get_include_me (extension))
			continue;

		cancellable = g_cancellable_new ();

		g_queue_push_tail (
			&name_selector_entry->priv->cancellables,
			cancellable);

		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
			cancellable,
			name_selector_entry_get_client_cb,
			g_object_ref (contact_store));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_object_unref (registry);
	g_object_unref (client_cache);

	setup_contact_store (name_selector_entry);
}

static void
destination_row_changed (ENameSelectorEntry *name_selector_entry,
                         GtkTreePath *path,
                         GtkTreeIter *iter)
{
	EDestination *destination;
	const gchar  *entry_text;
	gchar        *text;
	gint          range_start, range_end;
	gint          n;

	n = gtk_tree_path_get_indices (path)[0];
	destination = e_destination_store_get_destination (name_selector_entry->priv->destination_store, iter);

	if (!destination)
		return;

	g_return_if_fail (n >= 0);

	entry_text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	if (!get_range_by_index (entry_text, n, &range_start, &range_end)) {
		g_warning ("ENameSelectorEntry is out of sync with model!");
		return;
	}

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), range_start, range_end);

	text = get_destination_textrep (name_selector_entry, destination);
	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), text, -1, &range_start);
	g_free (text);

	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	clear_completion_model (name_selector_entry);
	generate_attribute_list (name_selector_entry);
}

static void
destination_row_inserted (ENameSelectorEntry *name_selector_entry,
                          GtkTreePath *path,
                          GtkTreeIter *iter)
{
	EDestination *destination;
	const gchar  *entry_text;
	gchar        *text;
	gboolean      comma_before = FALSE;
	gboolean      comma_after = FALSE;
	gint          range_start, range_end;
	gint          insert_pos;
	gint          n;

	n = gtk_tree_path_get_indices (path)[0];
	destination = e_destination_store_get_destination (name_selector_entry->priv->destination_store, iter);

	g_return_if_fail (n >= 0);
	g_return_if_fail (destination != NULL);

	entry_text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));

	if (get_range_by_index (entry_text, n, &range_start, &range_end) && range_start != range_end) {
		/* Another destination comes after us */
		insert_pos = range_start;
		comma_after = TRUE;
	} else if (n > 0 && get_range_by_index (entry_text, n - 1, &range_start, &range_end)) {
		/* Another destination comes before us */
		insert_pos = range_end;
		comma_before = TRUE;
	} else if (n == 0) {
		/* We're the sole destination */
		insert_pos = 0;
	} else {
		g_warning ("ENameSelectorEntry is out of sync with model!");
		return;
	}

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	if (comma_before)
		gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, &insert_pos);

	text = get_destination_textrep (name_selector_entry, destination);
	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), text, -1, &insert_pos);
	g_free (text);

	if (comma_after)
		gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), ", ", -1, &insert_pos);

	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);

	clear_completion_model (name_selector_entry);
	generate_attribute_list (name_selector_entry);
}

static void
destination_row_deleted (ENameSelectorEntry *name_selector_entry,
                         GtkTreePath *path)
{
	const gchar *text;
	gboolean     deleted_comma = FALSE;
	gint         range_start, range_end;
	gchar       *p0;
	gint         n;

	n = gtk_tree_path_get_indices (path)[0];
	g_return_if_fail (n >= 0);

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));

	if (!get_range_by_index (text, n, &range_start, &range_end)) {
		g_warning ("ENameSelectorEntry is out of sync with model!");
		return;
	}

	/* Expand range for deletion forwards */
	for (p0 = g_utf8_offset_to_pointer (text, range_end); *p0;
	     p0 = g_utf8_next_char (p0), range_end++) {
		gunichar c = g_utf8_get_char (p0);

		/* Gobble spaces directly after comma */
		if (c != ' ' && deleted_comma) {
			range_end--;
			break;
		}

		if (c == ',') {
			deleted_comma = TRUE;
			range_end++;
		}
	}

	/* Expand range for deletion backwards */
	for (p0 = g_utf8_offset_to_pointer (text, range_start); range_start > 0;
	     p0 = g_utf8_prev_char (p0), range_start--) {
		gunichar c = g_utf8_get_char (p0);

		if (c == ',') {
			if (!deleted_comma)
				break;

			range_start++;

			/* Leave a space in front; we deleted the comma and spaces before the
			 * following destination */
			p0 = g_utf8_next_char (p0);
			c = g_utf8_get_char (p0);
			if (c == ' ')
				range_start++;

			break;
		}
	}

	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), range_start, range_end);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	clear_completion_model (name_selector_entry);
	generate_attribute_list (name_selector_entry);
}

static void
setup_destination_store (ENameSelectorEntry *name_selector_entry)
{
	GtkTreeIter  iter;

	g_signal_connect_swapped (
		name_selector_entry->priv->destination_store, "row-changed",
		G_CALLBACK (destination_row_changed), name_selector_entry);
	g_signal_connect_swapped (
		name_selector_entry->priv->destination_store, "row-deleted",
		G_CALLBACK (destination_row_deleted), name_selector_entry);
	g_signal_connect_swapped (
		name_selector_entry->priv->destination_store, "row-inserted",
		G_CALLBACK (destination_row_inserted), name_selector_entry);

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (name_selector_entry->priv->destination_store), &iter))
		return;

	do {
		GtkTreePath *path;

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (name_selector_entry->priv->destination_store), &iter);
		g_return_if_fail (path);

		destination_row_inserted (name_selector_entry, path, &iter);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (name_selector_entry->priv->destination_store), &iter));
}

static gboolean
prepare_popup_destination (ENameSelectorEntry *name_selector_entry,
                           GdkEventButton *event_button)
{
	EDestination *destination;
	PangoLayout  *layout;
	gint          layout_offset_x;
	gint          layout_offset_y;
	gint          x, y;
	gint          index;

	if (event_button->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (event_button->button != 3)
		return FALSE;

	g_clear_object (&name_selector_entry->priv->popup_destination);

	gtk_entry_get_layout_offsets (
		GTK_ENTRY (name_selector_entry),
		&layout_offset_x, &layout_offset_y);
	x = (event_button->x + 0.5) - layout_offset_x;
	y = (event_button->y + 0.5) - layout_offset_y;

	if (x < 0 || y < 0)
		return FALSE;

	layout = gtk_entry_get_layout (GTK_ENTRY (name_selector_entry));
	if (!pango_layout_xy_to_index (layout, x * PANGO_SCALE, y * PANGO_SCALE, &index, NULL))
		return FALSE;

	index = gtk_entry_layout_index_to_text_index (GTK_ENTRY (name_selector_entry), index);
	destination = find_destination_at_position (name_selector_entry, index);
	/* FIXME: Add this to a private variable, in ENameSelectorEntry Class*/
	g_object_set_data ((GObject *) name_selector_entry, "index", GINT_TO_POINTER (index));

	if (!destination || !e_destination_get_contact (destination))
		return FALSE;

	/* TODO: Unref destination when we finalize */
	name_selector_entry->priv->popup_destination = g_object_ref (destination);
	return FALSE;
}

static EBookClient *
find_client_by_contact (GSList *clients,
                        const gchar *contact_uid,
                        const gchar *source_uid)
{
	GSList *l;

	if (source_uid && *source_uid) {
		/* this is much quicket than asking each client for an existence */
		for (l = clients; l; l = g_slist_next (l)) {
			EBookClient *client = l->data;
			ESource *source = e_client_get_source (E_CLIENT (client));

			if (!source)
				continue;

			if (g_strcmp0 (source_uid, e_source_get_uid (source)) == 0)
				return client;
		}
	}

	for (l = clients; l; l = g_slist_next (l)) {
		EBookClient *client = l->data;
		EContact *contact = NULL;
		gboolean  result;

		result = e_book_client_get_contact_sync (client, contact_uid, &contact, NULL, NULL);
		if (contact)
			g_object_unref (contact);

		if (result)
			return client;
	}

	return NULL;
}

static void
editor_closed_cb (GtkWidget *editor,
                  gpointer data)
{
	EContact *contact;
	gchar *contact_uid;
	EDestination *destination;
	GSList *clients;
	EBookClient *book_client;
	gint email_num;
	ENameSelectorEntry *name_selector_entry = E_NAME_SELECTOR_ENTRY (data);

	destination = name_selector_entry->priv->popup_destination;
	contact = e_destination_get_contact (destination);
	if (!contact) {
		g_object_unref (name_selector_entry);
		return;
	}

	contact_uid = e_contact_get (contact, E_CONTACT_UID);
	if (!contact_uid) {
		g_object_unref (contact);
		g_object_unref (name_selector_entry);
		return;
	}

	if (name_selector_entry->priv->contact_store) {
		clients = e_contact_store_get_clients (name_selector_entry->priv->contact_store);
		book_client = find_client_by_contact (clients, contact_uid, e_destination_get_source_uid (destination));
		g_slist_free (clients);
	} else {
		book_client = NULL;
	}

	if (book_client) {
		contact = NULL;

		g_warn_if_fail (e_book_client_get_contact_sync (book_client, contact_uid, &contact, NULL, NULL));
		email_num = e_destination_get_email_num (destination);
		e_destination_set_contact (destination, contact, email_num);
		e_destination_set_client (destination, book_client);
	} else {
		contact = NULL;
	}

	g_free (contact_uid);
	if (contact)
		g_object_unref (contact);
	g_object_unref (name_selector_entry);
}

/* To parse something like...
 * =?UTF-8?Q?=E0=A4=95=E0=A4=95=E0=A4=AC=E0=A5=82=E0=A5=8B=E0=A5=87?=\t\n=?UTF-8?Q?=E0=A4=B0?=\t\n<aa@aa.ccom>
 * and return the decoded representation of name & email parts.
 * */
static gboolean
eab_parse_qp_email (const gchar *string,
                    gchar **name,
                    gchar **email)
{
	struct _camel_header_address *address;
	gboolean res = FALSE;

	address = camel_header_address_decode (string, "UTF-8");

	if (!address)
		return FALSE;

        /* report success only when we have filled both name and email address */
	if (address->type == CAMEL_HEADER_ADDRESS_NAME && address->name && *address->name && address->v.addr && *address->v.addr) {
                *name = g_strdup (address->name);
                *email = g_strdup (address->v.addr);
		res = TRUE;
	}

	camel_header_address_unref (address);

	return res;
}

static void
popup_activate_inline_expand (ENameSelectorEntry *name_selector_entry,
                              GtkWidget *menu_item)
{
	const gchar *text;
	GString *sanitized_text = g_string_new ("");
	EDestination *destination = name_selector_entry->priv->popup_destination;
	gint position, start, end;
	const GList *dests;

	position = GPOINTER_TO_INT (g_object_get_data ((GObject *) name_selector_entry, "index"));

	for (dests = e_destination_list_get_dests (destination); dests; dests = dests->next) {
		const EDestination *dest = dests->data;
		gchar *sanitized;
		gchar *name = NULL, *email = NULL, *tofree = NULL;

		if (!dest)
			continue;

		text = e_destination_get_textrep (dest, TRUE);

		if (!text || !*text)
			continue;

		if (eab_parse_qp_email (text, &name, &email)) {
			tofree = g_strdup_printf ("%s <%s>", name, email);
			text = tofree;
			g_free (name);
			g_free (email);
		}

		sanitized = sanitize_string (text);
		g_free (tofree);
		if (!sanitized)
			continue;

		if (*sanitized) {
			if (*sanitized_text->str)
				g_string_append (sanitized_text, ", ");

			g_string_append (sanitized_text, sanitized);
		}

		g_free (sanitized);
	}

	text = gtk_entry_get_text (GTK_ENTRY (name_selector_entry));
	if (get_range_at_position (text, position, &start, &end))
		gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), start, end);
	gtk_editable_insert_text (GTK_EDITABLE (name_selector_entry), sanitized_text->str, -1, &start);
	g_string_free (sanitized_text, TRUE);

	clear_completion_model (name_selector_entry);
	generate_attribute_list (name_selector_entry);
}

static void
popup_activate_contact (ENameSelectorEntry *name_selector_entry,
                        GtkWidget *menu_item)
{
	EBookClient  *book_client;
	GSList       *clients;
	EDestination *destination;
	EContact     *contact;
	gchar        *contact_uid;

	destination = name_selector_entry->priv->popup_destination;
	if (!destination)
		return;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	contact_uid = e_contact_get (contact, E_CONTACT_UID);
	if (!contact_uid)
		return;

	if (name_selector_entry->priv->contact_store) {
		clients = e_contact_store_get_clients (name_selector_entry->priv->contact_store);
		book_client = find_client_by_contact (clients, contact_uid, e_destination_get_source_uid (destination));
		g_slist_free (clients);
		g_free (contact_uid);
	} else {
		book_client = NULL;
	}

	if (!book_client)
		return;

	if (e_destination_is_evolution_list (destination)) {
		GtkWidget *contact_list_editor;

		if (!name_selector_entry->priv->contact_list_editor_func)
			return;

		contact_list_editor = (*name_selector_entry->priv->contact_list_editor_func) (book_client, contact, FALSE, TRUE);
		g_object_ref (name_selector_entry);
		g_signal_connect (
			contact_list_editor, "editor_closed",
			G_CALLBACK (editor_closed_cb), name_selector_entry);
	} else {
		GtkWidget *contact_editor;

		if (!name_selector_entry->priv->contact_editor_func)
			return;

		contact_editor = (*name_selector_entry->priv->contact_editor_func) (book_client, contact, FALSE, TRUE);
		g_object_ref (name_selector_entry);
		g_signal_connect (
			contact_editor, "editor_closed",
			G_CALLBACK (editor_closed_cb), name_selector_entry);
	}
}

static void
popup_activate_email (ENameSelectorEntry *name_selector_entry,
                      GtkWidget *menu_item)
{
	EDestination *destination;
	EContact     *contact;
	gint          email_num;

	destination = name_selector_entry->priv->popup_destination;
	if (!destination)
		return;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	email_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "order"));
	e_destination_set_contact (destination, contact, email_num);
}

static void
popup_activate_list (EDestination *destination,
                     GtkWidget *item)
{
	gboolean status = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));

	e_destination_set_ignored (destination, !status);
}

static void
popup_activate_cut (ENameSelectorEntry *name_selector_entry,
                    GtkWidget *menu_item)
{
	EDestination *destination;
	const gchar *contact_email;
	gchar *pemail = NULL;
	GtkClipboard *clipboard;

	destination = name_selector_entry->priv->popup_destination;
	contact_email =e_destination_get_textrep (destination, TRUE);

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	pemail = g_strconcat (contact_email, ",", NULL);
	gtk_clipboard_set_text (clipboard, pemail, strlen (pemail));

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, pemail, strlen (pemail));

	gtk_editable_delete_text (GTK_EDITABLE (name_selector_entry), 0, 0);
	e_destination_store_remove_destination (name_selector_entry->priv->destination_store, destination);

	g_free (pemail);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);
}

static void
popup_activate_copy (ENameSelectorEntry *name_selector_entry,
                     GtkWidget *menu_item)
{
	EDestination *destination;
	const gchar *contact_email;
	gchar *pemail;
	GtkClipboard *clipboard;

	destination = name_selector_entry->priv->popup_destination;
	contact_email = e_destination_get_textrep (destination, TRUE);

	g_signal_handlers_block_by_func (name_selector_entry, user_insert_text, name_selector_entry);
	g_signal_handlers_block_by_func (name_selector_entry, user_delete_text, name_selector_entry);

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	pemail = g_strconcat (contact_email, ",", NULL);
	gtk_clipboard_set_text (clipboard, pemail, strlen (pemail));

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, pemail, strlen (pemail));
	g_free (pemail);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_delete_text, name_selector_entry);
	g_signal_handlers_unblock_by_func (name_selector_entry, user_insert_text, name_selector_entry);
}

static void
destination_set_list (GtkWidget *item,
                      EDestination *destination)
{
	EContact *contact;
	gboolean status = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	e_destination_set_ignored (destination, !status);
}

static void
destination_set_email (GtkWidget *item,
                       EDestination *destination)
{
	gint email_num;
	EContact *contact;

	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;
	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	email_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "order"));
	e_destination_set_contact (destination, contact, email_num);
}

static void
populate_popup (ENameSelectorEntry *name_selector_entry,
                GtkMenu *menu)
{
	EDestination *destination;
	EContact     *contact;
	GtkWidget    *menu_item;
	GList        *email_list = NULL;
	GList        *l;
	gint          i;
	gchar	     *edit_label;
	gchar	     *cut_label;
	gchar         *copy_label;
	gint	      email_num, len;
	GSList	     *group = NULL;
	gboolean      is_list;
	gboolean      show_menu = FALSE;

	destination = name_selector_entry->priv->popup_destination;
	if (!destination)
		return;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	/* Prepend the menu items, backwards */

	/* Separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	email_num = e_destination_get_email_num (destination);

	/* Addresses */
	is_list = e_contact_get (contact, E_CONTACT_IS_LIST) ? TRUE : FALSE;
	if (is_list) {
		const GList *dests = e_destination_list_get_dests (destination);
		GList *iter;
		gint length = g_list_length ((GList *) dests);

		for (iter = (GList *) dests; iter; iter = iter->next) {
			EDestination *dest = (EDestination *) iter->data;
			const gchar *email = e_destination_get_email (dest);

			if (!email || *email == '\0')
				continue;

			if (length > 1) {
				menu_item = gtk_check_menu_item_new_with_label (email);
				g_signal_connect (
					menu_item, "toggled",
					G_CALLBACK (destination_set_list), dest);
			} else {
				menu_item = gtk_menu_item_new_with_label (email);
			}

			gtk_widget_show (menu_item);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
			show_menu = TRUE;

			if (length > 1) {
				gtk_check_menu_item_set_active (
					GTK_CHECK_MENU_ITEM (menu_item),
					!e_destination_is_ignored (dest));
				g_signal_connect_swapped (
					menu_item, "activate",
					G_CALLBACK (popup_activate_list), dest);
			}
		}

	} else {
		email_list = e_contact_get (contact, E_CONTACT_EMAIL);
		len = g_list_length (email_list);

		for (l = email_list, i = 0; l; l = g_list_next (l), i++) {
			gchar *email = l->data;

			if (!email || *email == '\0')
				continue;

			if (len > 1) {
				menu_item = gtk_radio_menu_item_new_with_label (group, email);
				group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));
				g_signal_connect (menu_item, "toggled", G_CALLBACK (destination_set_email), destination);
			} else {
				menu_item = gtk_menu_item_new_with_label (email);
			}

			gtk_widget_show (menu_item);
			gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
			show_menu = TRUE;
			g_object_set_data (G_OBJECT (menu_item), "order", GINT_TO_POINTER (i));

			if (i == email_num && len > 1) {
				gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
				g_signal_connect_swapped (
					menu_item, "activate",
					G_CALLBACK (popup_activate_email),
					name_selector_entry);
			}
		}
	}

	/* Separator */

	if (show_menu) {
		menu_item = gtk_separator_menu_item_new ();
		gtk_widget_show (menu_item);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	}

	/* Expand a list inline */
	if (is_list) {
		/* To Translators: This would be similiar to "Expand MyList Inline" where MyList is a Contact List*/
		edit_label = g_strdup_printf (_("E_xpand %s Inline"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
		menu_item = gtk_menu_item_new_with_mnemonic (edit_label);
		g_free (edit_label);
		gtk_widget_show (menu_item);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
		g_signal_connect_swapped (
			menu_item, "activate", G_CALLBACK (popup_activate_inline_expand),
			name_selector_entry);

		/* Separator */
		menu_item = gtk_separator_menu_item_new ();
		gtk_widget_show (menu_item);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	}

	/* Copy Contact Item */
	copy_label = g_strdup_printf (_("Cop_y %s"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
	menu_item = gtk_menu_item_new_with_mnemonic (copy_label);
	g_free (copy_label);
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect_swapped (
		menu_item, "activate", G_CALLBACK (popup_activate_copy),
		name_selector_entry);

	/* Cut Contact Item */
	cut_label = g_strdup_printf (_("C_ut %s"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
	menu_item = gtk_menu_item_new_with_mnemonic (cut_label);
	g_free (cut_label);
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect_swapped (
		menu_item, "activate", G_CALLBACK (popup_activate_cut),
		name_selector_entry);

	if (show_menu) {
		menu_item = gtk_separator_menu_item_new ();
		gtk_widget_show (menu_item);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	}

	/* Edit Contact item */

	edit_label = g_strdup_printf (_("_Edit %s"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
	menu_item = gtk_menu_item_new_with_mnemonic (edit_label);
	g_free (edit_label);
	gtk_widget_show (menu_item);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect_swapped (
		menu_item, "activate", G_CALLBACK (popup_activate_contact),
		name_selector_entry);

	deep_free_list (email_list);
}

static gint
compare_gint_ptr_cb (gconstpointer a,
                     gconstpointer b)
{
	return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

static void
copy_or_cut_clipboard (ENameSelectorEntry *name_selector_entry,
                       gboolean is_cut)
{
	GtkClipboard *clipboard;
	GtkEditable *editable;
	const gchar *text, *cp;
	GHashTable *hash;
	GHashTableIter iter;
	gpointer key, value;
	GSList *sorted, *siter;
	GString *addresses;
	gint ii, start, end, ostart, oend;
	gunichar uc;

	editable = GTK_EDITABLE (name_selector_entry);
	text = gtk_entry_get_text (GTK_ENTRY (editable));

	if (!gtk_editable_get_selection_bounds (editable, &start, &end))
		return;

	g_return_if_fail (end > start);

	hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* convert from character indexes to pointer indexes */
	ostart = g_utf8_offset_to_pointer (text, start) - text;
	oend = g_utf8_offset_to_pointer (text, end) - text;

	ii = end;
	cp = g_utf8_offset_to_pointer (text, end);
	uc = g_utf8_get_char (cp);

	/* Exclude trailing whitespace and commas. */
	while (ii >= start && (uc == ',' || g_unichar_isspace (uc))) {
		cp = g_utf8_prev_char (cp);
		uc = g_utf8_get_char (cp);
		ii--;
	}

	/* Determine the index of each remaining character. */
	while (ii >= start) {
		gint index = get_index_at_position (text, ii--);
		g_hash_table_insert (hash, GINT_TO_POINTER (index), NULL);
	}

	sorted = NULL;
	g_hash_table_iter_init (&iter, hash);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		sorted = g_slist_prepend (sorted, key);
	}

	sorted = g_slist_sort (sorted, compare_gint_ptr_cb);
	addresses = g_string_new ("");

	for (siter = sorted; siter != NULL; siter = g_slist_next (siter)) {
		gint index = GPOINTER_TO_INT (siter->data);
		EDestination *dest;
		gint rstart, rend;

		if (!get_range_by_index (text, index, &rstart, &rend))
			continue;

		/* convert from character indexes to pointer indexes */
		rstart = g_utf8_offset_to_pointer (text, rstart) - text;
		rend = g_utf8_offset_to_pointer (text, rend) - text;

		if (rstart < ostart) {
			if (addresses->str && *addresses->str)
				g_string_append (addresses, ", ");

			g_string_append_len (addresses, text + ostart, MIN (oend - ostart, rend - ostart));
		} else if (rend > oend) {
			if (addresses->str && *addresses->str)
				g_string_append (addresses, ", ");

			g_string_append_len (addresses, text + rstart, oend - rstart);
		} else {
			/* the contact is whole selected */
			dest = find_destination_by_index (name_selector_entry, index);
			if (dest && e_destination_get_textrep (dest, TRUE)) {
				if (addresses->str && *addresses->str)
					g_string_append (addresses, ", ");

				g_string_append (addresses, e_destination_get_textrep (dest, TRUE));
			} else
				g_string_append_len (addresses, text + rstart, rend - rstart);
		}
	}

	g_slist_free (sorted);

	if (is_cut)
		gtk_editable_delete_text (editable, start, end);

	g_hash_table_unref (hash);

	clipboard = gtk_widget_get_clipboard (
		GTK_WIDGET (name_selector_entry), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, addresses->str, -1);

	g_string_free (addresses, TRUE);
}

static void
copy_clipboard (GtkEntry *entry,
                ENameSelectorEntry *name_selector_entry)
{
	copy_or_cut_clipboard (name_selector_entry, FALSE);
	g_signal_stop_emission_by_name (entry, "copy-clipboard");
}

static void
cut_clipboard (GtkEntry *entry,
               ENameSelectorEntry *name_selector_entry)
{
	copy_or_cut_clipboard (name_selector_entry, TRUE);
	g_signal_stop_emission_by_name (entry, "cut-clipboard");
}

static void
e_name_selector_entry_init (ENameSelectorEntry *name_selector_entry)
{
	GtkCellRenderer *renderer;

	name_selector_entry->priv = e_name_selector_entry_get_instance_private (name_selector_entry);

	g_queue_init (&name_selector_entry->priv->cancellables);

	name_selector_entry->priv->minimum_query_length = 3;
	name_selector_entry->priv->show_address = FALSE;
	name_selector_entry->priv->block_entry_changed_signal = FALSE;
	name_selector_entry->priv->known_contacts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	/* Edit signals */

	g_signal_connect (
		name_selector_entry, "changed",
		G_CALLBACK (maybe_block_entry_changed_cb), NULL);
	g_signal_connect (
		name_selector_entry, "insert-text",
		G_CALLBACK (user_insert_text), name_selector_entry);
	g_signal_connect (
		name_selector_entry, "delete-text",
		G_CALLBACK (user_delete_text), name_selector_entry);
	g_signal_connect (
		name_selector_entry, "focus-out-event",
		G_CALLBACK (user_focus_out), name_selector_entry);
	g_signal_connect_after (
		name_selector_entry, "focus-in-event",
		G_CALLBACK (user_focus_in), name_selector_entry);
	g_signal_connect (
		name_selector_entry, "key-press-event",
		G_CALLBACK (user_key_press_event_cb), name_selector_entry);

	/* Drawing */

	g_signal_connect (
		name_selector_entry, "draw",
		G_CALLBACK (draw_event), name_selector_entry);

	/* Activation: Complete current entry if possible */

	g_signal_connect (
		name_selector_entry, "activate",
		G_CALLBACK (entry_activate), name_selector_entry);

	/* Pop-up menu */

	g_signal_connect (
		name_selector_entry, "button-press-event",
		G_CALLBACK (prepare_popup_destination), name_selector_entry);
	g_signal_connect (
		name_selector_entry, "populate-popup",
		G_CALLBACK (populate_popup), name_selector_entry);

	/* Clipboard signals */
	g_signal_connect (
		name_selector_entry, "copy-clipboard",
		G_CALLBACK (copy_clipboard), name_selector_entry);
	g_signal_connect (
		name_selector_entry, "cut-clipboard",
		G_CALLBACK (cut_clipboard), name_selector_entry);

	/* Completion */

	name_selector_entry->priv->email_generator = NULL;

	name_selector_entry->priv->entry_completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_match_func (
		name_selector_entry->priv->entry_completion,
		(GtkEntryCompletionMatchFunc) completion_match_cb, NULL, NULL);
	g_signal_connect_swapped (
		name_selector_entry->priv->entry_completion, "match-selected",
		G_CALLBACK (completion_match_selected), name_selector_entry);

	gtk_entry_set_completion (
		GTK_ENTRY (name_selector_entry),
		name_selector_entry->priv->entry_completion);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (name_selector_entry->priv->entry_completion),
		renderer, FALSE);
	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (name_selector_entry->priv->entry_completion),
		GTK_CELL_RENDERER (renderer),
		(GtkCellLayoutDataFunc) contact_layout_pixbuffer,
		name_selector_entry, NULL);

	/* Completion list name renderer */
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (name_selector_entry->priv->entry_completion),
		renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (name_selector_entry->priv->entry_completion),
		GTK_CELL_RENDERER (renderer),
		(GtkCellLayoutDataFunc) contact_layout_formatter,
		name_selector_entry, NULL);

	/* Destination store */

	name_selector_entry->priv->destination_store = e_destination_store_new ();
	setup_destination_store (name_selector_entry);
	name_selector_entry->priv->is_completing = FALSE;
}

/**
 * e_name_selector_entry_new:
 * @client_cache: an #EClientCache
 *
 * Creates a new #ENameSelectorEntry.
 *
 * Returns: A new #ENameSelectorEntry.
 **/
GtkWidget *
e_name_selector_entry_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_NAME_SELECTOR_ENTRY,
		"client-cache", client_cache, NULL);
}

/**
 * e_name_selector_entry_ref_client_cache:
 * @name_selector_entry: an #ENameSelectorEntry
 *
 * Returns the #EClientCache passed to e_name_selector_entry_new().
 *
 * The returned #EClientCache is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClientCache
 *
 * Since: 3.8
 **/
EClientCache *
e_name_selector_entry_ref_client_cache (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (
		E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), NULL);

	if (name_selector_entry->priv->client_cache == NULL)
		return NULL;

	return g_object_ref (name_selector_entry->priv->client_cache);
}

/**
 * e_name_selector_entry_set_client_cache:
 * @name_selector_entry: an #ENameSelectorEntry
 * @client_cache: an #EClientCache
 *
 * Sets the #EClientCache used to query address books.
 *
 * This function is intended for cases where @name_selector_entry is
 * instantiated by a #GtkBuilder and has to be given an #EClientCache
 * after it is fully constructed.
 *
 * Since: 3.6
 **/
void
e_name_selector_entry_set_client_cache (ENameSelectorEntry *name_selector_entry,
                                        EClientCache *client_cache)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));

	if (client_cache == name_selector_entry->priv->client_cache)
		return;

	if (client_cache != NULL) {
		g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
		g_object_ref (client_cache);
	}

	if (name_selector_entry->priv->client_cache != NULL)
		g_object_unref (name_selector_entry->priv->client_cache);

	name_selector_entry->priv->client_cache = client_cache;

	g_object_notify (G_OBJECT (name_selector_entry), "client-cache");
}

/**
 * e_name_selector_entry_get_minimum_query_length:
 * @name_selector_entry: an #ENameSelectorEntry
 *
 * Returns: Minimum length of query before completion starts
 *
 * Since: 3.6
 **/
gint
e_name_selector_entry_get_minimum_query_length (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), -1);

	return name_selector_entry->priv->minimum_query_length;
}

/**
 * e_name_selector_entry_set_minimum_query_length:
 * @name_selector_entry: an #ENameSelectorEntry
 * @length: minimum query length
 *
 * Sets minimum length of query before completion starts.
 *
 * Since: 3.6
 **/
void
e_name_selector_entry_set_minimum_query_length (ENameSelectorEntry *name_selector_entry,
                                                gint length)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));
	g_return_if_fail (length > 0);

	if (name_selector_entry->priv->minimum_query_length == length)
		return;

	name_selector_entry->priv->minimum_query_length = length;

	g_object_notify (G_OBJECT (name_selector_entry), "minimum-query-length");
}

/**
 * e_name_selector_entry_get_show_address:
 * @name_selector_entry: an #ENameSelectorEntry
 *
 * Returns: Whether always show email address for an auto-completed contact.
 *
 * Since: 3.6
 **/
gboolean
e_name_selector_entry_get_show_address (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), FALSE);

	return name_selector_entry->priv->show_address;
}

/**
 * e_name_selector_entry_set_show_address:
 * @name_selector_entry: an #ENameSelectorEntry
 * @show: new value to set
 *
 * Sets whether always show email address for an auto-completed contact.
 *
 * Since: 3.6
 **/
void
e_name_selector_entry_set_show_address (ENameSelectorEntry *name_selector_entry,
                                        gboolean show)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));

	if ((name_selector_entry->priv->show_address ? 1 : 0) == (show ? 1 : 0))
		return;

	name_selector_entry->priv->show_address = show;

	sanitize_entry (name_selector_entry);

	g_object_notify (G_OBJECT (name_selector_entry), "show-address");
}

/**
 * e_name_selector_entry_peek_contact_store:
 * @name_selector_entry: an #ENameSelectorEntry
 *
 * Gets the #EContactStore being used by @name_selector_entry.
 *
 * Returns: An #EContactStore.
 **/
EContactStore *
e_name_selector_entry_peek_contact_store (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), NULL);

	return name_selector_entry->priv->contact_store;
}

/**
 * e_name_selector_entry_set_contact_store:
 * @name_selector_entry: an #ENameSelectorEntry
 * @contact_store: an #EContactStore to use
 *
 * Sets the #EContactStore being used by @name_selector_entry to @contact_store.
 **/
void
e_name_selector_entry_set_contact_store (ENameSelectorEntry *name_selector_entry,
                                         EContactStore *contact_store)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));
	g_return_if_fail (contact_store == NULL || E_IS_CONTACT_STORE (contact_store));

	if (contact_store == name_selector_entry->priv->contact_store)
		return;

	if (name_selector_entry->priv->contact_store)
		g_object_unref (name_selector_entry->priv->contact_store);
	name_selector_entry->priv->contact_store = contact_store;
	if (name_selector_entry->priv->contact_store)
		g_object_ref (name_selector_entry->priv->contact_store);

	setup_contact_store (name_selector_entry);
}

/**
 * e_name_selector_entry_peek_destination_store:
 * @name_selector_entry: an #ENameSelectorEntry
 *
 * Gets the #EDestinationStore being used to store @name_selector_entry's destinations.
 *
 * Returns: An #EDestinationStore.
 **/
EDestinationStore *
e_name_selector_entry_peek_destination_store (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), NULL);

	return name_selector_entry->priv->destination_store;
}

/**
 * e_name_selector_entry_set_destination_store:
 * @name_selector_entry: an #ENameSelectorEntry
 * @destination_store: an #EDestinationStore to use
 *
 * Sets @destination_store as the #EDestinationStore to be used to store
 * destinations for @name_selector_entry.
 **/
void
e_name_selector_entry_set_destination_store (ENameSelectorEntry *name_selector_entry,
                                             EDestinationStore *destination_store)
{
	g_return_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry));
	g_return_if_fail (E_IS_DESTINATION_STORE (destination_store));

	if (destination_store == name_selector_entry->priv->destination_store)
		return;

	g_object_unref (name_selector_entry->priv->destination_store);
	name_selector_entry->priv->destination_store = g_object_ref (destination_store);

	setup_destination_store (name_selector_entry);
}

/**
 * e_name_selector_entry_get_popup_destination:
 *
 * Since: 2.32
 **/
EDestination *
e_name_selector_entry_get_popup_destination (ENameSelectorEntry *name_selector_entry)
{
	g_return_val_if_fail (E_IS_NAME_SELECTOR_ENTRY (name_selector_entry), NULL);

	return name_selector_entry->priv->popup_destination;
}

/**
 * e_name_selector_entry_set_contact_editor_func:
 *
 * DO NOT USE.
 **/
void
e_name_selector_entry_set_contact_editor_func (ENameSelectorEntry *name_selector_entry,
                                               gpointer func)
{
	name_selector_entry->priv->contact_editor_func = func;
}

/**
 * e_name_selector_entry_set_contact_list_editor_func:
 *
 * DO NOT USE.
 **/
void
e_name_selector_entry_set_contact_list_editor_func (ENameSelectorEntry *name_selector_entry,
                                                    gpointer func)
{
	name_selector_entry->priv->contact_list_editor_func = func;
}
