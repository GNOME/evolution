/*
 * e-html-editor-spell-check-dialog-dom-functions.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-html-editor-spell-check-dialog-dom-functions.h"

#include <web-extensions/e-dom-utils.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

static gboolean
select_next_word (WebKitDOMDOMSelection *dom_selection)
{
	gulong anchor_offset, focus_offset;
	WebKitDOMNode *anchor, *focus;

	anchor = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	focus = webkit_dom_dom_selection_get_focus_node (dom_selection);
	focus_offset = webkit_dom_dom_selection_get_focus_offset (dom_selection);

	/* Jump _behind_ next word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "forward", "word");
	/* Jump before the word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "forward", "word");

	/* If the selection didn't change, then we have most probably
	 * reached the end of document - return FALSE */
	return !((anchor == webkit_dom_dom_selection_get_anchor_node (dom_selection)) &&
		 (anchor_offset == webkit_dom_dom_selection_get_anchor_offset (dom_selection)) &&
		 (focus == webkit_dom_dom_selection_get_focus_node (dom_selection)) &&
		 (focus_offset == webkit_dom_dom_selection_get_focus_offset (dom_selection)));
}

gchar *
e_html_editor_spell_check_dialog_next (WebKitDOMDocument *document,
                                       const gchar *word)
{
	gulong start_offset = 0, end_offset = 0;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNode *start = NULL, *end = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!word) {
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "left", "documentboundary");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (dom_selection);
		end = webkit_dom_dom_selection_get_focus_node (dom_selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (dom_selection);
	}

#if 0 /* FIXME WK2 */
	while (select_next_word (dom_selection)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		word = webkit_dom_range_get_text (range);
		g_object_unref (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
			checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1)
			return word;

		g_free (word);
	}
#endif
	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the end of the document */
	if (start && end)
		webkit_dom_dom_selection_set_base_and_extent (
			dom_selection, start, start_offset, end, end_offset, NULL);

	g_object_unref (dom_selection);

	return FALSE;
}

static gboolean
select_previous_word (WebKitDOMDOMSelection *dom_selection)
{
	WebKitDOMNode *old_anchor_node;
	WebKitDOMNode *new_anchor_node;
	gulong old_anchor_offset;
	gulong new_anchor_offset;

	old_anchor_node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	old_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	/* Jump on the beginning of current word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Jump before previous word */
	webkit_dom_dom_selection_modify (dom_selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (dom_selection, "extend", "forward", "word");

	/* If the selection start didn't change, then we have most probably
	 * reached the beginnig of document. Return FALSE */

	new_anchor_node = webkit_dom_dom_selection_get_anchor_node (dom_selection);
	new_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);

	return (new_anchor_node != old_anchor_node) ||
		(new_anchor_offset != old_anchor_offset);
}

gchar *
e_html_editor_spell_check_dialog_prev (WebKitDOMDocument *document,
                                       const gchar *word)
{
	gulong start_offset = 0, end_offset = 0;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMNode *start = NULL, *end = NULL;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!word) {
		webkit_dom_dom_selection_modify (
			dom_selection, "move", "right", "documentboundary");
		webkit_dom_dom_selection_modify (
			dom_selection, "extend", "backward", "word");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (dom_selection);
		end = webkit_dom_dom_selection_get_focus_node (dom_selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (dom_selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (dom_selection);
	}
#if 0 /* FIXME WK2 */
	while (select_previous_word (dom_selection)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
		word = webkit_dom_range_get_text (range);
		g_object_unref (range);

		checker = WEBKIT_SPELL_CHECKER (webkit_get_text_checker ());
		webkit_spell_checker_check_spelling_of_string (
			checker, word, &loc, &len);

		/* Found misspelled word! */
		if (loc != -1) {
			html_editor_spell_check_dialog_set_word (dialog, word);
			g_free (word);
			return TRUE;
		}

		g_free (word);
	}
#endif
	/* Restore the selection to contain the last misspelled word. This is
	 * reached only when we reach the beginning of the document */
	if (start && end)
		webkit_dom_dom_selection_set_base_and_extent (
			dom_selection, start, start_offset, end, end_offset, NULL);

	return FALSE;
}
