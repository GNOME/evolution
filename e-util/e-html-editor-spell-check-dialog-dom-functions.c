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

#include "e-dom-utils.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

static gboolean
select_next_word (WebKitDOMDOMSelection *selection)
{
	gulong anchor_offset, focus_offset;
	WebKitDOMNode *anchor, *focus;

	anchor = webkit_dom_dom_selection_get_anchor_node (selection);
	anchor_offset = webkit_dom_dom_selection_get_anchor_offset (selection);

	focus = webkit_dom_dom_selection_get_focus_node (selection);
	focus_offset = webkit_dom_dom_selection_get_focus_offset (selection);

	/* Jump _behind_ next word */
	webkit_dom_dom_selection_modify (selection, "move", "forward", "word");
	/* Jump before the word */
	webkit_dom_dom_selection_modify (selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (selection, "extend", "forward", "word");

	/* If the selection didn't change, then we have most probably
	 * reached the end of document - return FALSE */
	return !((anchor == webkit_dom_dom_selection_get_anchor_node (selection)) &&
		 (anchor_offset == webkit_dom_dom_selection_get_anchor_offset (selection)) &&
		 (focus == webkit_dom_dom_selection_get_focus_node (selection)) &&
		 (focus_offset == webkit_dom_dom_selection_get_focus_offset (selection)));
}

gchar *
e_html_editor_spell_check_dialog_next (WebKitDOMDocument *document,
                                       const gchar *word)
{
	gulong start_offset, end_offset;
	WebKitDOMDOMSelection *selection;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *start = NULL, *end = NULL;

	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	if (!word) {
		webkit_dom_dom_selection_modify (
			selection, "move", "left", "documentboundary");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (selection);
		end = webkit_dom_dom_selection_get_focus_node (selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (selection);
	}

#if 0 /* FIXME WK2 */
	while (select_next_word (selection)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		word = webkit_dom_range_get_text (range);

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
			selection, start, start_offset, end, end_offset, NULL);

	return FALSE;
}

static gboolean
select_previous_word (WebKitDOMDOMSelection *selection)
{
	WebKitDOMNode *old_anchor_node;
	WebKitDOMNode *new_anchor_node;
	gulong old_anchor_offset;
	gulong new_anchor_offset;

	old_anchor_node = webkit_dom_dom_selection_get_anchor_node (selection);
	old_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (selection);

	/* Jump on the beginning of current word */
	webkit_dom_dom_selection_modify (selection, "move", "backward", "word");
	/* Jump before previous word */
	webkit_dom_dom_selection_modify (selection, "move", "backward", "word");
	/* Select it */
	webkit_dom_dom_selection_modify (selection, "extend", "forward", "word");

	/* If the selection start didn't change, then we have most probably
	 * reached the beginnig of document. Return FALSE */

	new_anchor_node = webkit_dom_dom_selection_get_anchor_node (selection);
	new_anchor_offset = webkit_dom_dom_selection_get_anchor_offset (selection);

	return (new_anchor_node != old_anchor_node) ||
		(new_anchor_offset != old_anchor_offset);
}

gchar *
e_html_editor_spell_check_dialog_prev (WebKitDOMDocument *document,
                                       const gchar *word)
{
	gulong start_offset, end_offset;
	WebKitDOMDOMSelection *selection;
	WebKitDOMDOMWindow *window;
	WebKitDOMNode *start = NULL, *end = NULL;

	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	if (!word) {
		webkit_dom_dom_selection_modify (
			selection, "move", "right", "documentboundary");
		webkit_dom_dom_selection_modify (
			selection, "extend", "backward", "word");
	} else {
		/* Remember last selected word */
		start = webkit_dom_dom_selection_get_anchor_node (selection);
		end = webkit_dom_dom_selection_get_focus_node (selection);
		start_offset = webkit_dom_dom_selection_get_anchor_offset (selection);
		end_offset = webkit_dom_dom_selection_get_focus_offset (selection);
	}
#if 0 /* FIXME WK2 */
	while (select_previous_word (selection)) {
		WebKitDOMRange *range;
		WebKitSpellChecker *checker;
		gint loc, len;
		gchar *word;

		range = webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
		word = webkit_dom_range_get_text (range);

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
			selection, start, start_offset, end, end_offset, NULL);

	return FALSE;
}
