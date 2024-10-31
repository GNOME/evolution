/* Taken from libgweather 3, due to it being gone in the libgweather 4
 *
 * SPDX-FileCopyrightText: (C) 2008 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <string.h>
#include <geocode-glib/geocode-glib.h>
#include <gio/gio.h>

#include <glib/gi18n-lib.h>

#include "e-weather-location-entry.h"

/*
 * SECTION:gweatherlocationentry
 * @Title: EWeatherLocationEntry
 *
 * A subclass of #GtkSearchEntry that provides autocompletion on
 * #GWeatherLocation<!-- -->s
 */

struct _EWeatherLocationEntryPrivate {
	GWeatherLocation *location;
	GWeatherLocation *top;
	gboolean          show_named_timezones;
	gboolean          custom_text;
	GCancellable     *cancellable;
	GtkTreeModel     *model;
};

G_DEFINE_TYPE_WITH_PRIVATE (EWeatherLocationEntry, e_weather_location_entry, GTK_TYPE_SEARCH_ENTRY)

enum {
    PROP_0,
    PROP_TOP,
    PROP_SHOW_NAMED_TIMEZONES,
    PROP_LOCATION,
    LAST_PROP
};

static void set_property (GObject *object, guint prop_id,
			  const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
			  GValue *value, GParamSpec *pspec);

static void set_location_internal (EWeatherLocationEntry *entry,
				   GtkTreeModel          *model,
				   GtkTreeIter           *iter,
				   GWeatherLocation      *loc);
static void
fill_location_entry_model (GtkListStore *store, GWeatherLocation *loc,
			   const char *parent_display_name,
			   const char *parent_sort_local_name,
			   const char *parent_compare_local_name,
			   const char *parent_compare_english_name,
			   gboolean show_named_timezones);

enum LOC
{
	LOC_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
	LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION,
	LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME,
	LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME,
	LOC_E_WEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME,
	LOC_E_WEATHER_LOCATION_ENTRY_NUM_COLUMNS
};

enum PLACE
{
	PLACE_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME = 0,
	PLACE_E_WEATHER_LOCATION_ENTRY_COL_PLACE,
	PLACE_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME,
	PLACE_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME
};

static gboolean matcher (GtkEntryCompletion *completion, const char *key,
			 GtkTreeIter *iter, gpointer user_data);
static gboolean match_selected (GtkEntryCompletion *completion,
				GtkTreeModel       *model,
				GtkTreeIter        *iter,
				gpointer            entry);
static void entry_changed (EWeatherLocationEntry *entry);
static void _no_matches (GtkEntryCompletion *completion, EWeatherLocationEntry *entry);

static void
e_weather_location_entry_init (EWeatherLocationEntry *entry)
{
	GtkEntryCompletion *completion;
	EWeatherLocationEntryPrivate *priv;

	priv = entry->priv = e_weather_location_entry_get_instance_private (entry);

	completion = gtk_entry_completion_new ();

	gtk_entry_completion_set_popup_set_width (completion, FALSE);
	gtk_entry_completion_set_text_column (completion, LOC_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME);
	gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
	gtk_entry_completion_set_inline_completion (completion, TRUE);

	g_signal_connect (completion, "match-selected",
			  G_CALLBACK (match_selected), entry);

	g_signal_connect (completion, "no-matches",
			  G_CALLBACK (_no_matches), entry);

	gtk_entry_set_completion (GTK_ENTRY (entry), completion);
	g_object_unref (completion);

	priv->custom_text = FALSE;
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed), NULL);
}

static void
finalize (GObject *object)
{
	EWeatherLocationEntry *entry;
	EWeatherLocationEntryPrivate *priv;

	entry = E_WEATHER_LOCATION_ENTRY (object);
	priv = entry->priv;

	if (priv->location)
		g_object_unref (priv->location);
	if (priv->top)
		g_object_unref (priv->top);
	if (priv->model)
		g_object_unref (priv->model);

	G_OBJECT_CLASS (e_weather_location_entry_parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
	EWeatherLocationEntry *entry;
	EWeatherLocationEntryPrivate *priv;

	entry = E_WEATHER_LOCATION_ENTRY (object);
	priv = entry->priv;

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	G_OBJECT_CLASS (e_weather_location_entry_parent_class)->dispose (object);
}

static gint
tree_compare_local_name (GtkTreeModel *model,
			 GtkTreeIter  *a,
			 GtkTreeIter  *b,
			 gpointer      user_data)
{
	gchar *name_a = NULL, *name_b = NULL;
	gint res;

	gtk_tree_model_get (model, a,
			    LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, &name_a,
			    -1);
	gtk_tree_model_get (model, b,
			    LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, &name_b,
			    -1);

	res = g_utf8_collate (name_a, name_b);

	g_free (name_a);
	g_free (name_b);

	return res;
}


static void
constructed (GObject *object)
{
	EWeatherLocationEntry *entry;
	GtkListStore *store = NULL;
	GtkEntryCompletion *completion;

	entry = E_WEATHER_LOCATION_ENTRY (object);

	if (!entry->priv->top)
		entry->priv->top = gweather_location_get_world ();

	store = gtk_list_store_new (5, G_TYPE_STRING, GWEATHER_TYPE_LOCATION, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store), tree_compare_local_name, NULL, NULL);
	fill_location_entry_model (store, entry->priv->top, NULL, NULL, NULL, NULL, entry->priv->show_named_timezones);

	entry->priv->model = GTK_TREE_MODEL (store);
	completion = gtk_entry_get_completion (GTK_ENTRY (entry));
	gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));

	G_OBJECT_CLASS (e_weather_location_entry_parent_class)->constructed (object);
}

static void
e_weather_location_entry_class_init (EWeatherLocationEntryClass *location_entry_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (location_entry_class);

	object_class->constructed = constructed;
	object_class->finalize = finalize;
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->dispose = dispose;

	g_object_class_install_property (
		object_class, PROP_TOP,
		g_param_spec_object ("top",
				     "Top Location",
				     "The GWeatherLocation whose children will be used to fill in the entry",
				     GWEATHER_TYPE_LOCATION,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class, PROP_SHOW_NAMED_TIMEZONES,
		g_param_spec_boolean ("show-named-timezones",
				      "Show named timezones",
				      "Whether UTC and other named timezones are shown in the list of locations",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class, PROP_LOCATION,
		g_param_spec_object ("location",
				     "Location",
				     "The selected GWeatherLocation",
				     GWEATHER_TYPE_LOCATION,
				     G_PARAM_READWRITE));
}

static void
set_property (GObject *object,
	      guint prop_id,
	      const GValue *value,
	      GParamSpec *pspec)
{
	EWeatherLocationEntry *entry = E_WEATHER_LOCATION_ENTRY (object);

	switch (prop_id) {
	case PROP_TOP:
		entry->priv->top = g_value_dup_object (value);
		break;
	case PROP_SHOW_NAMED_TIMEZONES:
		entry->priv->show_named_timezones = g_value_get_boolean (value);
		break;
	case PROP_LOCATION:
		e_weather_location_entry_set_location (E_WEATHER_LOCATION_ENTRY (object),
						       g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object,
	      guint prop_id,
	      GValue *value,
	      GParamSpec *pspec)
{
	EWeatherLocationEntry *entry = E_WEATHER_LOCATION_ENTRY (object);

	switch (prop_id) {
	case PROP_SHOW_NAMED_TIMEZONES:
		g_value_set_boolean (value, entry->priv->show_named_timezones);
		break;
	case PROP_LOCATION:
		g_value_set_object (value, entry->priv->location);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
entry_changed (EWeatherLocationEntry *entry)
{
	GtkEntryCompletion *completion;
	const gchar *text;

	completion = gtk_entry_get_completion (GTK_ENTRY (entry));

	if (entry->priv->cancellable) {
		g_cancellable_cancel (entry->priv->cancellable);
		g_object_unref (entry->priv->cancellable);
		entry->priv->cancellable = NULL;
		gtk_entry_completion_delete_action (completion, 0);
	}

	gtk_entry_completion_set_match_func (gtk_entry_get_completion (GTK_ENTRY (entry)), matcher, NULL, NULL);
	gtk_entry_completion_set_model (gtk_entry_get_completion (GTK_ENTRY (entry)), entry->priv->model);

	text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (text && *text)
		entry->priv->custom_text = TRUE;
	else
		set_location_internal (entry, NULL, NULL, NULL);
}

static void
set_location_internal (EWeatherLocationEntry *entry,
		       GtkTreeModel          *model,
		       GtkTreeIter           *iter,
		       GWeatherLocation      *loc)
{
	EWeatherLocationEntryPrivate *priv;
	gchar *name;

	priv = entry->priv;

	if (priv->location)
		g_object_unref (priv->location);

	g_return_if_fail (iter == NULL || loc == NULL);

	if (iter) {
		gtk_tree_model_get (model, iter,
				    LOC_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, &name,
				    LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION, &priv->location,
				    -1);
		gtk_entry_set_text (GTK_ENTRY (entry), name);
		priv->custom_text = FALSE;
		g_free (name);
	} else if (loc) {
		priv->location = g_object_ref (loc);
		gtk_entry_set_text (GTK_ENTRY (entry), gweather_location_get_name (loc));
		priv->custom_text = FALSE;
	} else {
		priv->location = NULL;
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		priv->custom_text = TRUE;
	}

	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
	g_object_notify (G_OBJECT (entry), "location");
}

/*
 * e_weather_location_entry_set_location:
 * @entry: an #EWeatherLocationEntry
 * @loc: (allow-none): a #GWeatherLocation in @entry, or %NULL to
 * clear @entry
 *
 * Sets @entry's location to @loc, and updates the text of the
 * entry accordingly.
 * Note that if the database contains a location that compares
 * equal to @loc, that will be chosen in place of @loc.
 */
void
e_weather_location_entry_set_location (EWeatherLocationEntry *entry,
				       GWeatherLocation *loc)
{
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GWeatherLocation *cmploc;

	g_return_if_fail (E_WEATHER_IS_LOCATION_ENTRY (entry));

	completion = gtk_entry_get_completion (GTK_ENTRY (entry));
	model = gtk_entry_completion_get_model (completion);

	if (loc == NULL) {
		set_location_internal (entry, model, NULL, NULL);
		return;
	}

	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_tree_model_get (model, &iter,
				    LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
				    -1);
		if (gweather_location_equal (loc, cmploc)) {
			set_location_internal (entry, model, &iter, NULL);
			g_object_unref (cmploc);
			return;
		}

		g_object_unref (cmploc);
	} while (gtk_tree_model_iter_next (model, &iter));

	set_location_internal (entry, model, NULL, loc);
}

/*
 * e_weather_location_entry_get_location:
 * @entry: an #EWeatherLocationEntry
 *
 * Gets the location that was set by a previous call to
 * e_weather_location_entry_set_location() or was selected by the user.
 *
 * Return value: (transfer full) (allow-none): the selected location
 * (which you must unref when you are done with it), or %NULL if no
 * location is selected.
 **/
GWeatherLocation *
e_weather_location_entry_get_location (EWeatherLocationEntry *entry)
{
	g_return_val_if_fail (E_WEATHER_IS_LOCATION_ENTRY (entry), NULL);

	if (entry->priv->location)
		return g_object_ref (entry->priv->location);
	else
		return NULL;
}

/*
 * e_weather_location_entry_has_custom_text:
 * @entry: an #EWeatherLocationEntry
 *
 * Checks whether or not @entry's text has been modified by the user.
 * Note that this does not mean that no location is associated with @entry.
 * e_weather_location_entry_get_location() should be used for this.
 *
 * Return value: %TRUE if @entry's text was modified by the user, or %FALSE if
 * it's set to the default text of a location.
 **/
gboolean
e_weather_location_entry_has_custom_text (EWeatherLocationEntry *entry)
{
	g_return_val_if_fail (E_WEATHER_IS_LOCATION_ENTRY (entry), FALSE);

	return entry->priv->custom_text;
}

/*
 * e_weather_location_entry_set_city:
 * @entry: an #EWeatherLocationEntry
 * @city_name: (allow-none): the city name, or %NULL
 * @code: the METAR station code
 *
 * Sets @entry's location to a city with the given @code, and given
 * @city_name, if non-%NULL. If there is no matching city, sets
 * @entry's location to %NULL.
 *
 * Return value: %TRUE if @entry's location could be set to a matching city,
 * %FALSE otherwise.
 **/
gboolean
e_weather_location_entry_set_city (EWeatherLocationEntry *entry,
				   const gchar *city_name,
				   const gchar *code)
{
	GtkEntryCompletion *completion;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GWeatherLocation *cmploc;
	const gchar *cmpcode;
	gchar *cmpname;

	g_return_val_if_fail (E_WEATHER_IS_LOCATION_ENTRY (entry), FALSE);
	g_return_val_if_fail (code != NULL, FALSE);

	completion = gtk_entry_get_completion (GTK_ENTRY (entry));
	model = gtk_entry_completion_get_model (completion);

	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_tree_model_get (model, &iter,
				    LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION, &cmploc,
				    -1);

		cmpcode = gweather_location_get_code (cmploc);
		if (!cmpcode || strcmp (cmpcode, code) != 0) {
			g_object_unref (cmploc);
			continue;
		}

		if (city_name) {
			cmpname = gweather_location_get_city_name (cmploc);
			if (!cmpname || strcmp (cmpname, city_name) != 0) {
				g_object_unref (cmploc);
				g_free (cmpname);
				continue;
			}
			g_free (cmpname);
		}

		set_location_internal (entry, model, &iter, NULL);
		g_object_unref (cmploc);
		return TRUE;
	} while (gtk_tree_model_iter_next (model, &iter));

	set_location_internal (entry, model, NULL, NULL);

	return FALSE;
}

#if !GWEATHER_CHECK_VERSION(3, 39, 0)
#define gweather_location_get_english_sort_name gweather_location_get_sort_name
#endif

static void
fill_location_entry_model (GtkListStore *store,
			   GWeatherLocation *loc,
			   const gchar *parent_display_name,
			   const gchar *parent_sort_local_name,
			   const gchar *parent_compare_local_name,
			   const gchar *parent_compare_english_name,
			   gboolean show_named_timezones)
{
	GWeatherLocation *child = NULL;
	gchar *display_name, *local_sort_name, *local_compare_name, *english_compare_name;
	#if !GWEATHER_CHECK_VERSION(3, 39, 0)
	GWeatherLocation **children = gweather_location_get_children (loc);
	gint ii;
	#endif

	switch (gweather_location_get_level (loc)) {
	case GWEATHER_LOCATION_WORLD:
	case GWEATHER_LOCATION_REGION:
		/* Ignore these levels of hierarchy; just recurse, passing on
		 * the names from the parent node.
		 */
		#if GWEATHER_CHECK_VERSION(3, 39, 0)
		while ((child = gweather_location_next_child (loc, child)))
		#else
		for (ii = 0; children[ii]; ii++) {
			child = children[ii];
		#endif
			fill_location_entry_model (store, child,
						   parent_display_name,
						   parent_sort_local_name,
						   parent_compare_local_name,
						   parent_compare_english_name,
						   show_named_timezones);
		#if !GWEATHER_CHECK_VERSION(3, 39, 0)
			child = NULL; /* Do not unref it at the end of this function */
		}
		#endif
		break;

	case GWEATHER_LOCATION_COUNTRY:
		/* Recurse, initializing the names to the country name */
		#if GWEATHER_CHECK_VERSION(3, 39, 0)
		while ((child = gweather_location_next_child (loc, child)))
		#else
		for (ii = 0; children[ii]; ii++) {
			child = children[ii];
		#endif
			fill_location_entry_model (store, child,
						   gweather_location_get_name (loc),
						   gweather_location_get_sort_name (loc),
						   gweather_location_get_sort_name (loc),
						   gweather_location_get_english_sort_name (loc),
						   show_named_timezones);
		#if !GWEATHER_CHECK_VERSION(3, 39, 0)
			child = NULL; /* Do not unref it at the end of this function */
		}
		#endif
		break;

	case GWEATHER_LOCATION_ADM1:
		/* Recurse, adding the ADM1 name to the country name */
		/* Translators: this is the name of a location followed by a region, for example:
		 * 'London, United Kingdom'
		 * You shouldn't need to translate this string unless the language has a different comma.
		 */
		display_name = g_strdup_printf (_("%s, %s"), gweather_location_get_name (loc), parent_display_name);
		local_sort_name = g_strdup_printf ("%s, %s", parent_sort_local_name, gweather_location_get_sort_name (loc));
		local_compare_name = g_strdup_printf ("%s, %s", gweather_location_get_sort_name (loc), parent_compare_local_name);
		english_compare_name = g_strdup_printf ("%s, %s", gweather_location_get_english_sort_name (loc), parent_compare_english_name);

		#if GWEATHER_CHECK_VERSION(3, 39, 0)
		while ((child = gweather_location_next_child (loc, child)))
		#else
		for (ii = 0; children[ii]; ii++) {
			child = children[ii];
		#endif
			fill_location_entry_model (store, child,
						   display_name, local_sort_name, local_compare_name, english_compare_name,
						   show_named_timezones);
		#if !GWEATHER_CHECK_VERSION(3, 39, 0)
			child = NULL; /* Do not unref it at the end of this function */
		}
		#endif

		g_free (display_name);
		g_free (local_sort_name);
		g_free (local_compare_name);
		g_free (english_compare_name);
		break;

	case GWEATHER_LOCATION_CITY:
		/* If there are multiple (<location>) children, we use the one
		 * closest to the city center.
		 *
		 * Locations are already sorted by increasing distance from
		 * the city.
		 */
	case GWEATHER_LOCATION_WEATHER_STATION:
		/* <location> with no parent <city> */
		/* Translators: this is the name of a location followed by a region, for example:
		 * 'London, United Kingdom'
		 * You shouldn't need to translate this string unless the language has a different comma.
		 */
		display_name = g_strdup_printf (_("%s, %s"),
			gweather_location_get_name (loc), parent_display_name);
		local_sort_name = g_strdup_printf ("%s, %s",
			parent_sort_local_name, gweather_location_get_sort_name (loc));
		local_compare_name = g_strdup_printf ("%s, %s",
			gweather_location_get_sort_name (loc), parent_compare_local_name);
		english_compare_name = g_strdup_printf ("%s, %s",
			gweather_location_get_english_sort_name (loc), parent_compare_english_name);

		gtk_list_store_insert_with_values (store, NULL, -1,
			LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION, loc,
			LOC_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
			LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, local_sort_name,
			LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, local_compare_name,
			LOC_E_WEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, english_compare_name,
			-1);

		g_free (display_name);
		g_free (local_sort_name);
		g_free (local_compare_name);
		g_free (english_compare_name);
		break;

	case GWEATHER_LOCATION_NAMED_TIMEZONE:
		if (show_named_timezones) {
			gtk_list_store_insert_with_values (store, NULL, -1,
				LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCATION, loc,
				LOC_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, gweather_location_get_name (loc),
				LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, gweather_location_get_sort_name (loc),
				LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, gweather_location_get_sort_name (loc),
				LOC_E_WEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, gweather_location_get_english_sort_name (loc),
				-1);
		}
		break;

	case GWEATHER_LOCATION_DETACHED:
		g_warn_if_reached ();
		break;
	}

	g_clear_object (&child);
}

static gchar *
find_word (const gchar *full_name,
	   const gchar *word,
	   gint word_len,
	   gboolean whole_word,
	   gboolean is_first_word)
{
	gchar *p;

	if (word == NULL || *word == '\0')
		return NULL;

	p = (gchar *) full_name - 1;
	while ((p = strchr (p + 1, *word))) {
		if (strncmp (p, word, word_len) != 0)
			continue;

		if (p > (gchar *) full_name) {
			gchar *prev = g_utf8_prev_char (p);

			/* Make sure p points to the start of a word */
			if (g_unichar_isalpha (g_utf8_get_char (prev)))
				continue;

			/* If we're matching the first word of the key, it has to
			 * match the first word of the location, city, state, or
			 * country, or the abbreviation (in parenthesis).
			 * Eg, it either matches the start of the string
			 * (which we already know it doesn't at this point) or
			 * it is preceded by the string ", " or "(" (which isn't actually
			 * a perfect test. FIXME)
			 */
			if (is_first_word) {
				if (prev == (gchar *) full_name ||
				    ((prev - 1 <= full_name && strncmp (prev - 1, ", ", 2) != 0) && *prev != '('))
					continue;
			}
		}

		if (whole_word && g_unichar_isalpha (g_utf8_get_char (p + word_len)))
			continue;

		return p;
	}
	return NULL;
}

static gboolean
match_compare_name (const gchar *key,
		    const gchar *name)
{
	gboolean is_first_word = TRUE;
	size_t len;

	/* Ignore whitespace before the string */
	key += strspn (key, " ");

	/* All but the last word in KEY must match a full word from NAME,
	 * in order (but possibly skipping some words from NAME).
	 */
	len = strcspn (key, " ");
	while (key[len]) {
		name = find_word (name, key, len, TRUE, is_first_word);
		if (!name)
			return FALSE;

		key += len;
		while (*key && !g_unichar_isalpha (g_utf8_get_char (key)))
			key = g_utf8_next_char (key);
		while (*name && !g_unichar_isalpha (g_utf8_get_char (name)))
			name = g_utf8_next_char (name);

		len = strcspn (key, " ");
		is_first_word = FALSE;
	}

	/* The last word in KEY must match a prefix of a following word in NAME */
	if (len == 0) {
		return TRUE;
	} else {
		/* if we get here, key[len] == 0, so... */
		g_warn_if_fail (len == strlen (key));
		return find_word (name, key, len, FALSE, is_first_word) != NULL;
	}
}

static gboolean
matcher (GtkEntryCompletion *completion,
	 const gchar *key,
	 GtkTreeIter *iter,
	 gpointer user_data)
{
	gchar *local_compare_name, *english_compare_name;
	gboolean match;

	gtk_tree_model_get (gtk_entry_completion_get_model (completion), iter,
		LOC_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, &local_compare_name,
		LOC_E_WEATHER_LOCATION_ENTRY_COL_ENGLISH_COMPARE_NAME, &english_compare_name,
		-1);

	match = match_compare_name (key, local_compare_name) ||
		match_compare_name (key, english_compare_name) ||
		g_ascii_strcasecmp (key, english_compare_name) == 0;

	g_free (local_compare_name);
	g_free (english_compare_name);

	return match;
}

static gboolean
match_selected (GtkEntryCompletion *completion,
		GtkTreeModel *model,
		GtkTreeIter *iter,
		gpointer entry)
{
	EWeatherLocationEntryPrivate *priv;

	priv = ((EWeatherLocationEntry *)entry)->priv;

	if (model != priv->model) {
		GeocodePlace *place;
		gchar *display_name;
		GeocodeLocation *loc;
		GWeatherLocation *location;
		GWeatherLocation *scope = NULL;
		const gchar *country_code;

		gtk_tree_model_get (model, iter,
			PLACE_E_WEATHER_LOCATION_ENTRY_COL_PLACE, &place,
			PLACE_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, &display_name,
			-1);

		country_code = geocode_place_get_country_code (place);
		if (country_code != NULL && gweather_location_get_level (priv->top) == GWEATHER_LOCATION_WORLD)
			scope = gweather_location_find_by_country_code (priv->top, country_code);
		if (!scope)
			scope = priv->top;

		loc = geocode_place_get_location (place);
		location = gweather_location_find_nearest_city (scope, geocode_location_get_latitude (loc), geocode_location_get_longitude (loc));

		location = gweather_location_new_detached (display_name, NULL,
			geocode_location_get_latitude (loc),
			geocode_location_get_longitude (loc));

		set_location_internal (entry, model, NULL, location);

		g_object_unref (place);
		g_free (display_name);
	} else {
		set_location_internal (entry, model, iter, NULL);
	}

	return TRUE;
}

static gboolean
new_matcher (GtkEntryCompletion *completion,
	     const gchar *key,
             GtkTreeIter *iter,
	     gpointer user_data)
{
	return TRUE;
}

static void
fill_store (gpointer data,
	    gpointer user_data)
{
	GeocodePlace *place = GEOCODE_PLACE (data);
	GeocodeLocation *loc = geocode_place_get_location (place);
	const gchar *display_name;
	gchar *normalized;
	gchar *compare_name;

	display_name = geocode_location_get_description (loc);
	normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_ALL);
	compare_name = g_utf8_casefold (normalized, -1);

	gtk_list_store_insert_with_values (user_data, NULL, -1,
		PLACE_E_WEATHER_LOCATION_ENTRY_COL_PLACE, place,
		PLACE_E_WEATHER_LOCATION_ENTRY_COL_DISPLAY_NAME, display_name,
		PLACE_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_SORT_NAME, compare_name,
		PLACE_E_WEATHER_LOCATION_ENTRY_COL_LOCAL_COMPARE_NAME, compare_name,
		-1);

	g_free (normalized);
	g_free (compare_name);
}

static void
_got_places (GObject *source_object,
             GAsyncResult *result,
             gpointer user_data)
{
	GList *places;
	EWeatherLocationEntry *self = user_data;
	GError *error = NULL;
	GtkListStore *store = NULL;
	GtkEntryCompletion *completion;

	places = geocode_forward_search_finish (GEOCODE_FORWARD (source_object), result, &error);
	if (places == NULL) {
		/* return without touching anything if cancelled (the entry might have been disposed) */
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			g_clear_error (&error);
			return;
		}

		g_clear_error (&error);
		completion = gtk_entry_get_completion (user_data);
		gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
		gtk_entry_completion_set_model (completion, self->priv->model);
	} else {
		completion = gtk_entry_get_completion (user_data);
		store = gtk_list_store_new (5, G_TYPE_STRING, GEOCODE_TYPE_PLACE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store), tree_compare_local_name, NULL, NULL);
		g_list_foreach (places, fill_store, store);
		g_list_free (places);
		gtk_entry_completion_set_match_func (completion, new_matcher, NULL, NULL);
		gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
		g_object_unref (store);
	}

	gtk_entry_completion_delete_action (completion, 0);
	g_clear_object (&self->priv->cancellable);
}

static void
_no_matches (GtkEntryCompletion *completion,
	     EWeatherLocationEntry *entry)
{
	const gchar *key = gtk_entry_get_text (GTK_ENTRY (entry));
	GeocodeForward *forward;

	if (entry->priv->cancellable) {
		g_cancellable_cancel (entry->priv->cancellable);
		g_object_unref (entry->priv->cancellable);
		entry->priv->cancellable = NULL;
	} else {
		gtk_entry_completion_insert_action_text (completion, 0, _("Loading…"));
	}

	entry->priv->cancellable = g_cancellable_new ();

	forward = geocode_forward_new_for_string(key);
	geocode_forward_search_async (forward, entry->priv->cancellable, _got_places, entry);
}

/*
 * e_weather_location_entry_new:
 * @top: the top-level location for the entry.
 *
 * Creates a new #EWeatherLocationEntry.
 *
 * @top will normally be the location returned from
 * gweather_location_get_world(), but you can create an entry that
 * only accepts a smaller set of locations if you want.
 *
 * Return value: the new #EWeatherLocationEntry
 **/
GtkWidget *
e_weather_location_entry_new (GWeatherLocation *top)
{
	return g_object_new (E_WEATHER_TYPE_LOCATION_ENTRY,
			     "top", top,
			     NULL);
}
