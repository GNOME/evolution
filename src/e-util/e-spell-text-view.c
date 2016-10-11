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

#ifdef HAVE_GTKSPELL
#include <gtkspell/gtkspell.h>
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
#ifdef HAVE_GTKSPELL
	return TRUE;
#else /* HAVE_GTKSPELL */
	return FALSE;
#endif /* HAVE_GTKSPELL */
}

/**
 * e_spell_text_view_attach:
 * @text_view: a #GtkTextView
 *
 * Attaches a spell checker into the @text_view, if spell-checking is
 * enabled in Evolution.
 *
 * Returns: Whether successfully attached the spell checker
 *
 * Since: 3.12
 **/
gboolean
e_spell_text_view_attach (GtkTextView *text_view)
{
#ifdef HAVE_GTKSPELL
	GtkSpellChecker *spell;
	GSettings *settings;
	gchar **strv;
	gboolean success;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* do nothing, if spell-checking is disabled */
	if (!g_settings_get_boolean (settings, "composer-inline-spelling")) {
		g_object_unref (settings);
		return FALSE;
	}

	strv = g_settings_get_strv (settings, "composer-spell-languages");
	g_object_unref (settings);

	spell = gtk_spell_checker_new ();
	g_object_set (G_OBJECT (spell), "decode-language-codes", TRUE, NULL);
	if (strv)
		gtk_spell_checker_set_language (spell, strv[0], NULL);
	success = gtk_spell_checker_attach (spell, text_view);

	g_strfreev (strv);

	return success;
#else /* HAVE_GTKSPELL */
	return FALSE;
#endif /* HAVE_GTKSPELL */
}

/**
 * e_spell_text_view_recheck_all:
 * @text_view: a #GtkTextView with attached spell checker
 *
 * Checks whole content of the @text_view for spell-errors,
 * if it has previously attached spell-checker with
 * e_spell_text_view_attach().
 *
 * Since: 3.12
 **/
void
e_spell_text_view_recheck_all (GtkTextView *text_view)
{
#ifdef HAVE_GTKSPELL
	GtkSpellChecker *spell;

	spell = gtk_spell_checker_get_from_text_view (text_view);
	if (spell)
		gtk_spell_checker_recheck_all (spell);
#endif /* HAVE_GTKSPELL */
}
