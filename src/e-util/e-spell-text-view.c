/*
 * e-spell-text-view.c
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

/* Just a proxy for GtkSpell Text View spell checker */

#include "evolution-config.h"

#include <gtk/gtk.h>

#ifdef HAVE_GSPELL
#include <gspell/gspell.h>
#endif

#include "e-misc-utils.h"
#include "e-spell-text-view.h"

/**
 * e_spell_text_view_is_supported:
 *
 * Returns whether evolution was compiled with GtkSpell3. If it returns
 * %FALSE, all the other e_spell_text_view_... functions do nothing.
 *
 * Returns: Whether evolution was compiled with GtkSpell3
 *
 * Since: 3.12
 **/
gboolean
e_spell_text_view_is_supported (void)
{
#ifdef HAVE_GSPELL
	return TRUE;
#else /* HAVE_GSPELL */
	return FALSE;
#endif /* HAVE_GSPELL */
}

/**
 * e_spell_text_view_attach:
 * @text_view: a #GtkTextView
 *
 * Attaches a spell checker into the @text_view, if spell-checking is
 * enabled in Evolution.
 *
 * Since: 3.12
 **/
void
e_spell_text_view_attach (GtkTextView *text_view)
{
#ifdef HAVE_GSPELL
	GspellTextView *spell_view;
	GspellTextBuffer *spell_buffer;
	GspellChecker *checker;
	const GspellLanguage *language = NULL;
	GtkTextBuffer *text_buffer;
	GSettings *settings;
	gchar **strv;

	g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* do nothing, if spell-checking is disabled */
	if (!g_settings_get_boolean (settings, "composer-inline-spelling")) {
		g_object_unref (settings);
		return;
	}

	strv = g_settings_get_strv (settings, "composer-spell-languages");
	g_object_unref (settings);

	if (strv) {
		gint ii;

		for (ii = 0; strv[ii] && !language; ii++) {
			language = gspell_language_lookup (strv[ii]);
		}
	}

	g_strfreev (strv);

	checker = gspell_checker_new (language);
	text_buffer = gtk_text_view_get_buffer (text_view);
	spell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (text_buffer);
	gspell_text_buffer_set_spell_checker (spell_buffer, checker);
	g_object_unref (checker);

	spell_view = gspell_text_view_get_from_gtk_text_view (text_view);
	gspell_text_view_set_inline_spell_checking (spell_view, TRUE);
	gspell_text_view_set_enable_language_menu (spell_view, TRUE);
#endif /* HAVE_GSPELL */
}

/**
 * e_spell_text_view_get_enabled:
 * @text_view: a #GtkTextView
 *
 * Returns: whether the inline spell checking is enabled for the @text_view.
 * This can be used only after calling e_spell_text_view_attach().
 *
 * Since: 3.44
 **/
gboolean
e_spell_text_view_get_enabled (GtkTextView *text_view)
{
#ifdef HAVE_GSPELL
	GspellTextView *spell_view;

	spell_view = gspell_text_view_get_from_gtk_text_view (text_view);

	return gspell_text_view_get_inline_spell_checking (spell_view);
#else /* HAVE_GSPELL */
	return FALSE;
#endif /* HAVE_GSPELL */
}

/**
 * e_spell_text_view_set_enabled:
 * @text_view: a #GtkTextView
 * @enabled: value to set
 *
 * Sets whether the inline spell checking is enabled for the @text_view.
 * This can be used only after calling e_spell_text_view_attach().
 *
 * Since: 3.44
 **/
void
e_spell_text_view_set_enabled (GtkTextView *text_view,
			       gboolean enabled)
{
#ifdef HAVE_GSPELL
	GspellTextView *spell_view;

	spell_view = gspell_text_view_get_from_gtk_text_view (text_view);

	gspell_text_view_set_inline_spell_checking (spell_view, enabled);
#endif /* HAVE_GSPELL */
}

/**
 * e_spell_text_view_set_languages:
 * @text_view: a #GtkTextView
 * @languages: (nullable): languages to set, or %NULL to unset any previous
 *
 * Sets @languages for inline spell checking for the @text_view.
 * This can be used only after calling e_spell_text_view_attach().
 *
 * Since: 3.44
 **/
void
e_spell_text_view_set_languages (GtkTextView *text_view,
				 const gchar **languages)
{
#ifdef HAVE_GSPELL
	GspellTextBuffer *spell_buffer;
	GspellChecker *checker = NULL;
	GtkTextBuffer *text_buffer;
	guint ii;

	for (ii = 0; !checker && languages && languages[ii]; ii++) {
		const GspellLanguage *language;

		language = gspell_language_lookup (languages[ii]);

		if (language)
			checker = gspell_checker_new (language);
	}

	text_buffer = gtk_text_view_get_buffer (text_view);
	spell_buffer = gspell_text_buffer_get_from_gtk_text_buffer (text_buffer);
	gspell_text_buffer_set_spell_checker (spell_buffer, checker);
	g_clear_object (&checker);
#endif /* HAVE_GSPELL */
}
