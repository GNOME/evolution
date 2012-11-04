/*
 * e-editor-widget.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-editor-widget.h"
#include "e-editor.h"
#include "e-emoticon-chooser.h"

#include <e-util/e-util.h>
#include <e-util/e-marshal.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

struct _EEditorWidgetPrivate {
	gint changed		: 1;
	gint inline_spelling	: 1;
	gint magic_links	: 1;
	gint magic_smileys	: 1;
	gint can_copy		: 1;
	gint can_cut		: 1;
	gint can_paste		: 1;
	gint can_redo		: 1;
	gint can_undo		: 1;
	gint reload_in_progress : 1;
	gint html_mode		: 1;

	EEditorSelection *selection;

	/* FIXME WEBKIT Is this in widget's competence? */
	GList *spelling_langs;

	GSettings *font_settings;
	GSettings *aliasing_settings;

	GQueue *postreload_operations;
};

G_DEFINE_TYPE (
	EEditorWidget,
	e_editor_widget,
	WEBKIT_TYPE_WEB_VIEW);

enum {
	PROP_0,
	PROP_CHANGED,
	PROP_HTML_MODE,
	PROP_INLINE_SPELLING,
	PROP_MAGIC_LINKS,
	PROP_MAGIC_SMILEYS,
	PROP_SPELL_LANGUAGES,
	PROP_CAN_COPY,
	PROP_CAN_CUT,
	PROP_CAN_PASTE,
	PROP_CAN_REDO,
	PROP_CAN_UNDO
};

enum {
	POPUP_EVENT,
	LAST_SIGNAL
};

typedef void (*PostReloadOperationFunc)	(EEditorWidget *widget,
				 	 gpointer data);

typedef struct  {
	PostReloadOperationFunc	func;
	gpointer		data;
	GDestroyNotify		data_free_func;
} PostReloadOperation;

static guint signals[LAST_SIGNAL] = { 0 };

static void
editor_widget_queue_postreload_operation (EEditorWidget *widget,
					  PostReloadOperationFunc func,
					  gpointer data,
					  GDestroyNotify data_free_func)
{
	PostReloadOperation *op;

	g_return_if_fail (func != NULL);

	if (widget->priv->postreload_operations == NULL)
		widget->priv->postreload_operations = g_queue_new ();

	op = g_new0 (PostReloadOperation, 1);
	op->func = func;
	op->data = data;
	op->data_free_func = data_free_func;

	g_queue_push_head (widget->priv->postreload_operations, op);
}

static WebKitDOMRange *
editor_widget_get_dom_range (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	if (webkit_dom_dom_selection_get_range_count (selection) < 1) {
		return NULL;
	}

	return webkit_dom_dom_selection_get_range_at (selection, 0, NULL);
}

static void
editor_widget_user_changed_contents_cb (EEditorWidget *widget,
					gpointer user_data)
{
	gboolean can_redo, can_undo;

	e_editor_widget_set_changed (widget, TRUE);

	can_redo = webkit_web_view_can_redo (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_redo ? TRUE : FALSE) != (can_redo ? TRUE : FALSE)) {
		widget->priv->can_redo = can_redo;
		g_object_notify (G_OBJECT (widget), "can-redo");
	}

	can_undo = webkit_web_view_can_undo (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_undo ? TRUE : FALSE) != (can_undo ? TRUE : FALSE)) {
		widget->priv->can_undo = can_undo;
		g_object_notify (G_OBJECT (widget), "can-undo");
	}
}

static void
editor_widget_selection_changed_cb (EEditorWidget *widget,
				    gpointer user_data)
{
	gboolean can_copy, can_cut, can_paste;

	/* When the webview is being (re)loaded, the document is in inconsitant
	 * state and there is no selection, so don't propagate the signal further
	 * to EEditorSelection and others and wait until the load is finished */
	if (widget->priv->reload_in_progress) {
		g_signal_stop_emission_by_name (widget, "selection-changed");
		return;
	}

	can_copy = webkit_web_view_can_copy_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_copy ? TRUE : FALSE) != (can_copy ? TRUE : FALSE)) {
		widget->priv->can_copy = can_copy;
		g_object_notify (G_OBJECT (widget), "can-copy");
	}

	can_cut = webkit_web_view_can_cut_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_cut ? TRUE : FALSE) != (can_cut ? TRUE : FALSE)) {
		widget->priv->can_cut = can_cut;
		g_object_notify (G_OBJECT (widget), "can-cut");
	}

	can_paste = webkit_web_view_can_paste_clipboard (WEBKIT_WEB_VIEW (widget));
	if ((widget->priv->can_paste ? TRUE : FALSE) != (can_paste ? TRUE : FALSE)) {
		widget->priv->can_paste = can_paste;
		g_object_notify (G_OBJECT (widget), "can-paste");
	}
}

static gboolean
editor_widget_should_show_delete_interface_for_element (EEditorWidget *widget,
							WebKitDOMHTMLElement *element)
{
	return FALSE;
}

static void
editor_widget_load_status_changed (EEditorWidget *widget)
{
	WebKitLoadStatus status;
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMRange *range;

	status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (widget));
	if (status != WEBKIT_LOAD_FINISHED) {
		return;
	}

	widget->priv->reload_in_progress = FALSE;

 	/* After reload, there is no selection in the document. Fix it. */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);

	range = webkit_dom_document_create_range (document);
	webkit_dom_range_set_start (
		range, WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
		0, NULL);
	webkit_dom_range_set_end (
		range, WEBKIT_DOM_NODE (webkit_dom_document_get_body (document)),
		0, NULL);

	webkit_dom_dom_selection_add_range (selection, range);

	/* Dispatch queued operations */
	while (widget->priv->postreload_operations &&
	       !g_queue_is_empty (widget->priv->postreload_operations)) {

		PostReloadOperation *op;

		op = g_queue_pop_head (widget->priv->postreload_operations);

		op->func (widget, op->data);

		if (op->data_free_func)
			op->data_free_func (op->data);
		g_free (op);
	}
}

static gboolean
editor_widget_button_press_event (GtkWidget *gtk_widget,
				  GdkEventButton *event)
{
	gboolean event_handled;

	if (event->button != 3) {
		event_handled = FALSE;
	} else {
		g_signal_emit (
			gtk_widget, signals[POPUP_EVENT],
			0, event, &event_handled);
	}

	if (event_handled) {
		return TRUE;
	}

	/* Chain up to parent implementation */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->button_press_event (gtk_widget, event);
}

/* Based on original use_pictograms() from GtkHTML */
static const gchar *emoticons_chars =
	/*  0 */ "DO)(|/PQ*!"
	/* 10 */ "S\0:-\0:\0:-\0"
	/* 20 */ ":\0:;=-\"\0:;"
	/* 30 */ "B\"|\0:-'\0:X"
	/* 40 */ "\0:\0:-\0:\0:-"
	/* 50 */ "\0:\0:-\0:\0:-"
	/* 60 */ "\0:\0:\0:-\0:\0"
	/* 70 */ ":-\0:\0:-\0:\0";
static gint emoticons_states[] = {
	/*  0 */  12,  17,  22,  34,  43,  48,  53,  58,  65,  70,
	/* 10 */  75,   0, -15,  15,   0, -15,   0, -17,  20,   0,
	/* 20 */ -17,   0, -14, -20, -14,  28,  63,   0, -14, -20,
	/* 30 */  -3,  63, -18,   0, -12,  38,  41,   0, -12,  -2,
	/* 40 */   0,  -4,   0, -10,  46,   0, -10,   0, -19,  51,
	/* 50 */   0, -19,   0, -11,  56,   0, -11,   0, -13,  61,
	/* 60 */   0, -13,   0,  -6,   0,  68,  -7,   0,  -7,   0,
	/* 70 */ -16,  73,   0, -16,   0, -21,  78,   0, -21,   0 };
static const gchar *emoticons_icon_names[] = {
	"face-angel",
	"face-angry",
	"face-cool",
	"face-crying",
	"face-devilish",
	"face-embarrassed",
	"face-kiss",
	"face-laugh",		/* not used */
	"face-monkey",		/* not used */
	"face-plain",
	"face-raspberry",
	"face-sad",
	"face-sick",
	"face-smile",
	"face-smile-big",
	"face-smirk",
	"face-surprise",
	"face-tired",
	"face-uncertain",
	"face-wink",
	"face-worried"
};

static void
editor_widget_check_magic_smileys (EEditorWidget *widget,
				   WebKitDOMRange *range)
{
	gint pos;
	gint state;
	gint relative;
	gint start;
	gchar *node_text;
	gunichar uc;
	WebKitDOMNode *node;

	node = webkit_dom_range_get_end_container (range, NULL);
	if (!webkit_dom_node_get_node_type (node) == 3) {
		return;
	}

	node_text = webkit_dom_text_get_whole_text ((WebKitDOMText *) node);
	start = webkit_dom_range_get_end_offset (range, NULL) - 1;
	pos = start;
	state = 0;
	while (pos >= 0) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos));
		relative = 0;
		while (emoticons_chars[state + relative]) {
			if (emoticons_chars[state + relative] == uc)
				break;
			relative++;
		}
		state = emoticons_states[state + relative];
		/* 0 .. not found, -n .. found n-th */
		if (state <= 0)
			break;
		pos--;
	}

	/* Special case needed to recognize angel and devilish. */
	if (pos > 0 && state == -14) {
		uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
		if (uc == 'O') {
			state = -1;
			pos--;
		} else if (uc == '>') {
			state = -5;
			pos--;
		}
	}

	if (state < 0) {
		GtkIconInfo *icon_info;
		const gchar *filename;
		gchar *filename_uri, *html;
		WebKitDOMDocument *document;
		WebKitDOMDOMWindow *window;
		WebKitDOMDOMSelection *selection;
		const EEmoticon *emoticon;

		if (pos > 0) {
			uc = g_utf8_get_char (g_utf8_offset_to_pointer (node_text, pos - 1));
			if (uc != ' ' && uc != '\t') {
				g_free (node_text);
				return;
			}
		}

		/* Select the text-smiley and replace it by <img> */
		document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
		window = webkit_dom_document_get_default_view (document);
		selection = webkit_dom_dom_window_get_selection (window);
		webkit_dom_dom_selection_set_base_and_extent (
			selection, webkit_dom_range_get_end_container (range, NULL),
			pos, webkit_dom_range_get_end_container (range, NULL),
			start + 1, NULL);

		emoticon = e_emoticon_chooser_lookup_emoticon (
				emoticons_icon_names[-state - 1]);

		/* Convert a named icon to a file URI. */
		icon_info = gtk_icon_theme_lookup_icon (
			gtk_icon_theme_get_default (),
			emoticons_icon_names[-state - 1], 16, 0);
		g_return_if_fail (icon_info != NULL);
		filename = gtk_icon_info_get_filename (icon_info);
		g_return_if_fail (filename != NULL);
		filename_uri = g_filename_to_uri (filename, NULL, NULL);

		html = g_strdup_printf (
			"<img src=\"%s\" alt=\"%s\" x-evo-smiley=\"%s\" />",
			filename_uri, emoticon ? emoticon->text_face : "",
			emoticons_icon_names[-state - 1]);

		e_editor_selection_insert_html (
			widget->priv->selection, html);

		g_free (html);
		g_free (filename_uri);
		gtk_icon_info_free (icon_info);
	}

	g_free (node_text);
}

static void
editor_widget_set_links_active (EEditorWidget *widget,
			        gboolean active)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *style;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	if (active) {
		style = webkit_dom_document_get_element_by_id (
				document, "--evolution-editor-style-a");
		if (style) {
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (style)),
				WEBKIT_DOM_NODE (style), NULL);
		}
	} else {
		WebKitDOMHTMLHeadElement *head;
		head = webkit_dom_document_get_head (document);

		style = webkit_dom_document_create_element (document, "STYLE", NULL);
		webkit_dom_html_element_set_id (
			WEBKIT_DOM_HTML_ELEMENT (style), "--evolution-editor-style-a");
		webkit_dom_html_element_set_inner_text (
			WEBKIT_DOM_HTML_ELEMENT (style), "a { cursor: text; }", NULL);

		webkit_dom_node_append_child (
			WEBKIT_DOM_NODE (head), WEBKIT_DOM_NODE (style), NULL);
	}
}

static gboolean
editor_widget_key_press_event (GtkWidget *gtk_widget,
			       GdkEventKey *event)
{
	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		editor_widget_set_links_active (
			E_EDITOR_WIDGET (gtk_widget), TRUE);
	}

	/* Propagate the event to WebKit */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->key_press_event (gtk_widget, event);
}

static gboolean
editor_widget_key_release_event (GtkWidget *gtk_widget,
				 GdkEventKey *event)
{
	WebKitDOMRange *range;
	EEditorWidget *widget = E_EDITOR_WIDGET (gtk_widget);

	range = editor_widget_get_dom_range (widget);

	if (widget->priv->magic_smileys &&
	    widget->priv->html_mode) {
		editor_widget_check_magic_smileys (widget, range);
	}

	if ((event->keyval == GDK_KEY_Control_L) ||
	    (event->keyval == GDK_KEY_Control_R)) {

		editor_widget_set_links_active (widget, FALSE);
	}

	/* Propagate the event to WebKit */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->key_release_event (gtk_widget, event);
}

static gboolean
editor_widget_button_release_event (GtkWidget *gtk_widget,
				    GdkEventButton *event)
{
	WebKitWebView *webview;
	WebKitHitTestResult *hit_test;
	WebKitHitTestResultContext context;
	gchar *uri;

	webview = WEBKIT_WEB_VIEW (gtk_widget);
	hit_test = webkit_web_view_get_hit_test_result (webview, event);

	g_object_get (
		hit_test,
	       	"context", &context,
	        "link-uri", &uri,
	       	NULL);

	g_object_unref (hit_test);

	/* Left click on a link */
	if ((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK) &&
	    (event->button == 1)) {

		/* Ctrl + Left Click on link opens it, otherwise ignore the
		 * click completely */
		if (event->state & GDK_CONTROL_MASK) {

			gtk_show_uri (
				gtk_window_get_screen (
					GTK_WINDOW (gtk_widget_get_toplevel (gtk_widget))),
					uri, event->time, NULL);
		}

		return TRUE;
	}

	/* Propagate the event to WebKit */
	return GTK_WIDGET_CLASS (e_editor_widget_parent_class)->button_release_event (gtk_widget, event);
}

static void
clipboard_text_received (GtkClipboard *clipboard,
			 const gchar *text,
			 EEditorWidget *widget)
{
	gchar *html, *escaped_text;
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	/* This is a little trick to escape any HTML characters (like <, > or &).
	 * <textarea> automatically replaces all these unsafe characters
	 * by &lt;, &gt; etc. */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_create_element (document, "TEXTAREA", NULL);
	webkit_dom_html_element_set_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element), text, NULL);
	escaped_text = webkit_dom_html_element_get_inner_html (
		WEBKIT_DOM_HTML_ELEMENT (element));
	g_object_unref (element);

	html = g_strconcat (
		"<blockquote type=\"cite\"><pre>",
		escaped_text, "</pre></blockquote>", NULL);
	e_editor_selection_insert_html (widget->priv->selection, html);

	g_free (escaped_text);
	g_free (html);
}

static void
editor_widget_paste_clipboard_quoted (EEditorWidget *widget)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get_for_display (
			gdk_display_get_default (),
			GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text (
		clipboard,
		(GtkClipboardTextReceivedFunc) clipboard_text_received,
		widget);
}

static void
e_editor_widget_get_property (GObject *object,
			      guint property_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {

		case PROP_HTML_MODE:
			g_value_set_boolean (
				value, e_editor_widget_get_html_mode (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_INLINE_SPELLING:
			g_value_set_boolean (
				value, e_editor_widget_get_inline_spelling (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_MAGIC_LINKS:
			g_value_set_boolean (
				value, e_editor_widget_get_magic_links (
				E_EDITOR_WIDGET (object)));
			return;

		case PROP_MAGIC_SMILEYS:
			g_value_set_boolean (
				value, e_editor_widget_get_magic_smileys (
				E_EDITOR_WIDGET (object)));
			return;
		case PROP_CHANGED:
			g_value_set_boolean (
				value, e_editor_widget_get_changed (
				E_EDITOR_WIDGET (object)));
			return;
		case PROP_CAN_COPY:
			g_value_set_boolean (
				value, webkit_web_view_can_copy_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;
		case PROP_CAN_CUT:
			g_value_set_boolean (
				value, webkit_web_view_can_cut_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;
		case PROP_CAN_PASTE:
			g_value_set_boolean (
				value, webkit_web_view_can_paste_clipboard (
				WEBKIT_WEB_VIEW (object)));
			return;
		case PROP_CAN_REDO:
			g_value_set_boolean (
				value, webkit_web_view_can_redo (
				WEBKIT_WEB_VIEW (object)));
			return;
		case PROP_CAN_UNDO:
			g_value_set_boolean (
				value, webkit_web_view_can_undo (
				WEBKIT_WEB_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_set_property (GObject *object,
			      guint property_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	switch (property_id) {

		case PROP_HTML_MODE:
			e_editor_widget_set_html_mode (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_INLINE_SPELLING:
			e_editor_widget_set_inline_spelling (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_LINKS:
			e_editor_widget_set_magic_links (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;

		case PROP_MAGIC_SMILEYS:
			e_editor_widget_set_magic_smileys (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;
		case PROP_CHANGED:
			e_editor_widget_set_changed (
				E_EDITOR_WIDGET (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_editor_widget_finalize (GObject *object)
{
	EEditorWidgetPrivate *priv = E_EDITOR_WIDGET (object)->priv;

	g_clear_object (&priv->selection);

	/* Chain up to parent's implementation */
	G_OBJECT_CLASS (e_editor_widget_parent_class)->finalize (object);
}

static void
e_editor_widget_class_init (EEditorWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (klass, sizeof (EEditorWidgetPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = e_editor_widget_get_property;
	object_class->set_property = e_editor_widget_set_property;
	object_class->finalize = e_editor_widget_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->button_press_event = editor_widget_button_press_event;
	widget_class->button_release_event = editor_widget_button_release_event;
	widget_class->key_press_event = editor_widget_key_press_event;
	widget_class->key_release_event = editor_widget_key_release_event;

	klass->paste_clipboard_quoted = editor_widget_paste_clipboard_quoted;

	g_object_class_install_property (
		object_class,
		PROP_HTML_MODE,
		g_param_spec_boolean (
			"html-mode",
			"HTML Mode",
			"Edit HTML or plain text",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_INLINE_SPELLING,
		g_param_spec_boolean (
			"inline-spelling",
			"Inline Spelling",
			"Check your spelling as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_LINKS,
		g_param_spec_boolean (
			"magic-links",
			"Magic Links",
			"Make URIs clickable as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_MAGIC_SMILEYS,
		g_param_spec_boolean (
			"magic-smileys",
			"Magic Smileys",
			"Convert emoticons to images as you type",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
			_("Changed property"),
			_("Whether editor changed"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_COPY,
		g_param_spec_boolean (
			"can-copy",
			"Can Copy",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_CUT,
		g_param_spec_boolean (
			"can-cut",
			"Can Cut",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_PASTE,
		g_param_spec_boolean (
			"can-paste",
			"Can Paste",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_REDO,
		g_param_spec_boolean (
			"can-redo",
			"Can Redo",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_CAN_UNDO,
		g_param_spec_boolean (
			"can-undo",
			"Can Undo",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SPELL_LANGUAGES,
		g_param_spec_pointer (
			"spell-languages",
			"Active spell checking languages",
			NULL,
			G_PARAM_READWRITE));

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EEditorWidgetClass, popup_event),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__BOXED,
		G_TYPE_BOOLEAN, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
e_editor_widget_init (EEditorWidget *editor)
{
	WebKitWebSettings *settings;
	WebKitDOMDocument *document;
	GSettings *g_settings;
	GSettingsSchema *settings_schema;
	ESpellChecker *checker;

	editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		editor, E_TYPE_EDITOR_WIDGET, EEditorWidgetPrivate);

	webkit_web_view_set_editable (WEBKIT_WEB_VIEW (editor), TRUE);
	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (editor));

	g_object_set (
		G_OBJECT (settings),
		"enable-developer-extras", TRUE,
		"enable-dom-paste", TRUE,
		"enable-file-access-from-file-uris", TRUE,
	        "enable-plugins", FALSE,
	        "enable-scripts", FALSE,
	        "enable-spell-checking", TRUE,
		NULL);

	webkit_web_view_set_settings (WEBKIT_WEB_VIEW (editor), settings);

	/* Override the spell-checker, use our own */
	checker = g_object_new (E_TYPE_SPELL_CHECKER, NULL);
	webkit_set_text_checker (G_OBJECT (checker));
	g_object_unref (checker);

	/* Don't use CSS when possible to preserve compatibility with older
	 * versions of Evolution or other MUAs */
	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (editor));
	webkit_dom_document_exec_command (
		document, "styleWithCSS", FALSE, "false");

	g_signal_connect (editor, "user-changed-contents",
		G_CALLBACK (editor_widget_user_changed_contents_cb), NULL);
	g_signal_connect (editor, "selection-changed",
		G_CALLBACK (editor_widget_selection_changed_cb), NULL);
	g_signal_connect (editor, "should-show-delete-interface-for-element",
		G_CALLBACK (editor_widget_should_show_delete_interface_for_element), NULL);
	g_signal_connect (editor, "notify::load-status",
		G_CALLBACK (editor_widget_load_status_changed), NULL);

	editor->priv->selection = e_editor_selection_new (
					WEBKIT_WEB_VIEW (editor));

	g_settings = g_settings_new ("org.gnome.desktop.interface");
	g_signal_connect_swapped (
		g_settings, "changed::font-name",
		G_CALLBACK (e_editor_widget_update_fonts), editor);
	g_signal_connect_swapped (
		g_settings, "changed::monospace-font-name",
		G_CALLBACK (e_editor_widget_update_fonts), editor);
	editor->priv->font_settings = g_settings;

	/* This schema is optional.  Use if available. */
	settings_schema = g_settings_schema_source_lookup (
		g_settings_schema_source_get_default (),
		"org.gnome.settings-daemon.plugins.xsettings", FALSE);
	if (settings_schema != NULL) {
		g_settings = g_settings_new ("org.gnome.settings-daemon.plugins.xsettings");
		g_signal_connect_swapped (
			settings, "changed::antialiasing",
			G_CALLBACK (e_editor_widget_update_fonts), editor);
		editor->priv->aliasing_settings = g_settings;
	}

	e_editor_widget_update_fonts (editor);


	/* Make WebKit think we are displaying a local file, so that it
	 * does not block loading resources from file:// protocol */
	webkit_web_view_load_string (
		WEBKIT_WEB_VIEW (editor), "", "text/html", "UTF-8", "file://");

	editor_widget_set_links_active (editor, FALSE);
}

EEditorWidget *
e_editor_widget_new (void)
{
	return g_object_new (E_TYPE_EDITOR_WIDGET, NULL);
}

EEditorSelection *
e_editor_widget_get_selection (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), NULL);

	return widget->priv->selection;
}

gboolean
e_editor_widget_get_changed (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->changed;
}

void
e_editor_widget_set_changed (EEditorWidget *widget,
			     gboolean changed)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->changed ? TRUE : FALSE) == (changed ? TRUE : FALSE))
		return;

	widget->priv->changed = changed;

	g_object_notify (G_OBJECT (widget), "changed");
}

gboolean
e_editor_widget_get_html_mode (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->html_mode;
}

void
e_editor_widget_set_html_mode (EEditorWidget *widget,
			       gboolean html_mode)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->html_mode ? 1 : 0) == (html_mode ? 1 : 0)) {
		return;
	}

	widget->priv->html_mode = html_mode;

	if (widget->priv->html_mode) {
		gchar *plain_text, *html;

		plain_text = e_editor_widget_get_text_plain (widget);

		/* FIXME WEBKIT: This does not process smileys! */
		html = camel_text_to_html (
			plain_text,
			CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_MARK_CITATION |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES |
			CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED,
			0);

		e_editor_widget_set_text_html (widget, html);

		g_free (plain_text);
		g_free (html);
	} else {
		gchar *plain;

		plain = e_editor_widget_get_text_plain (widget);
		e_editor_widget_set_text_plain (widget, plain);

		g_free (plain);
	}

	g_object_notify (G_OBJECT (widget), "html-mode");
}

gboolean
e_editor_widget_get_inline_spelling (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->inline_spelling;
}

void
e_editor_widget_set_inline_spelling (EEditorWidget *widget,
				     gboolean inline_spelling)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->inline_spelling ? TRUE : FALSE) == (inline_spelling ? TRUE : FALSE))
		return;

	widget->priv->inline_spelling = inline_spelling;

	g_object_notify (G_OBJECT (widget), "inline-spelling");
}

gboolean
e_editor_widget_get_magic_links (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_links;
}

void
e_editor_widget_set_magic_links (EEditorWidget *widget,
				 gboolean magic_links)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->magic_links ? TRUE : FALSE) == (magic_links ? TRUE : FALSE))
		return;

	widget->priv->magic_links = magic_links;

	g_object_notify (G_OBJECT (widget), "magic-links");
}

gboolean
e_editor_widget_get_magic_smileys (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), FALSE);

	return widget->priv->magic_smileys;
}

void
e_editor_widget_set_magic_smileys (EEditorWidget *widget,
				   gboolean magic_smileys)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	if ((widget->priv->magic_smileys ? TRUE : FALSE) == (magic_smileys ? TRUE : FALSE))
		return;

	widget->priv->magic_smileys = magic_smileys;

	g_object_notify (G_OBJECT (widget), "magic-smileys");
}

GList *
e_editor_widget_get_spell_languages (EEditorWidget *widget)
{
	g_return_val_if_fail (E_IS_EDITOR_WIDGET (widget), NULL);

	return g_list_copy (widget->priv->spelling_langs);
}

void
e_editor_widget_set_spell_languages (EEditorWidget *widget,
				     GList *spell_languages)
{
	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));
	g_return_if_fail (spell_languages);

	g_list_free_full (widget->priv->spelling_langs, g_object_unref);
	widget->priv->spelling_langs = g_list_copy (spell_languages);
	g_list_foreach (widget->priv->spelling_langs, (GFunc) g_object_ref, NULL);

	g_object_notify (G_OBJECT (widget), "spell-languages");
}

ESpellChecker *
e_editor_widget_get_spell_checker (EEditorWidget *widget)
{
	return E_SPELL_CHECKER (webkit_get_text_checker ());
}


gchar *
e_editor_widget_get_text_html (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *element;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	element = webkit_dom_document_get_document_element (document);
	return webkit_dom_html_element_get_outer_html (
			WEBKIT_DOM_HTML_ELEMENT (element));
}

static void
process_elements (WebKitDOMNode *node,
		  GString *buffer)
{
	WebKitDOMDocument *document;
	WebKitDOMDOMWindow *window;
	WebKitDOMNodeList *nodes;
	WebKitDOMCSSStyleDeclaration *style;
	gchar *display, *tagname;
	gulong ii, length;
	GRegex *regex;

	document = webkit_dom_node_get_owner_document (node);
	window = webkit_dom_document_get_default_view (document);

	/* Is this a block element? */
	style = webkit_dom_dom_window_get_computed_style (
			window, WEBKIT_DOM_ELEMENT (node), "");
	display = webkit_dom_css_style_declaration_get_property_value (
			style, "display");

	tagname = webkit_dom_element_get_tag_name (WEBKIT_DOM_ELEMENT (node));

	/* Replace images with smileys by their text representation */
	if (g_ascii_strncasecmp (tagname, "IMG", 3) == 0) {
		if (webkit_dom_element_has_attribute (
				WEBKIT_DOM_ELEMENT (node), "x-evo-smiley")) {

			gchar *smiley_name;
			const EEmoticon *emoticon;

			smiley_name = webkit_dom_element_get_attribute (
					WEBKIT_DOM_ELEMENT (node), "x-evo-smiley");
			emoticon = e_emoticon_chooser_lookup_emoticon (smiley_name);
			if (emoticon) {
				g_string_append_printf (
					buffer, " %s ", emoticon->text_face);
			}
			g_free (smiley_name);
			g_free (display);

			/* IMG can't have child elements, so we return now */
			return;
		}
	}


	nodes = webkit_dom_node_get_child_nodes (node);
	length = webkit_dom_node_list_get_length (nodes);
	regex = g_regex_new ("\x9", 0, 0, NULL);

	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *child;

		child = webkit_dom_node_list_item (nodes, ii);
		if (webkit_dom_node_get_node_type (child) == 3) {
			gchar *content, *tmp;

			tmp = webkit_dom_node_get_text_content (child);

			/* Replace tabs with 4 whitespaces, otherwise they got
			   replaced by single whitespace */
			content = g_regex_replace (
				regex, tmp, -1, 0, "    ",
				0, NULL);

			g_string_append (buffer, content);
			g_free (tmp);
			g_free (content);
		}

		if (webkit_dom_node_has_child_nodes (child)) {
			process_elements (child, buffer);
		}
	}

	if (g_strcmp0 (display, "block") == 0) {
		g_string_append (buffer, "\n");
	}

	g_free (display);
	g_regex_unref (regex);
}

gchar *
e_editor_widget_get_text_plain (EEditorWidget *widget)
{
	WebKitDOMDocument *document;
	WebKitDOMNode *body;
	GString *plain_text;

	document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));
	body = (WebKitDOMNode *) webkit_dom_document_get_body (document);

	plain_text = g_string_sized_new (1024);
	process_elements (body, plain_text);

	/* Return text content between <body> and </body> */
	return g_string_free (plain_text, FALSE);
}

void
e_editor_widget_set_text_html (EEditorWidget *widget,
			       const gchar *text)
{
	widget->priv->reload_in_progress = TRUE;

	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (widget), text, "file://");
}

static void
do_set_text_plain (EEditorWidget *widget,
		   gpointer data)
{
	WebKitDOMDocument *document;

	document =
		webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (widget));

	webkit_dom_document_exec_command (
		document, "insertText", FALSE, data);
}

void
e_editor_widget_set_text_plain (EEditorWidget *widget,
				const gchar *text)
{
	widget->priv->reload_in_progress = TRUE;

	webkit_web_view_load_html_string (
		WEBKIT_WEB_VIEW (widget), "", "file://");

	/* webkit_web_view_load_html_string() is actually performed
	 * when this functions returns, so the operation below would get
	 * overwritten. Instead queue the insert operation and insert the
	 * actual text when the webview is reloaded */
	editor_widget_queue_postreload_operation (
		widget, do_set_text_plain, g_strdup (text), g_free);
}

void
e_editor_widget_paste_clipboard_quoted (EEditorWidget *widget)
{
	EEditorWidgetClass *klass;

	g_return_if_fail (E_IS_EDITOR_WIDGET (widget));

	klass = E_EDITOR_WIDGET_GET_CLASS (widget);
	g_return_if_fail (klass->paste_clipboard_quoted != NULL);

	klass->paste_clipboard_quoted (widget);
}

void
e_editor_widget_update_fonts (EEditorWidget *widget)
{
	GString *stylesheet;
	gchar *base64;
	gchar *aa = NULL;
	WebKitWebSettings *settings;
	PangoFontDescription *min_size, *ms, *vw;
	const gchar *styles[] = { "normal", "oblique", "italic" };
	const gchar *smoothing = NULL;
	GtkStyleContext *context;
	GdkColor *link = NULL;
	GdkColor *visited = NULL;

	ms = NULL;
	vw = NULL;


	if (ms == NULL) {
		gchar *font;

		font = g_settings_get_string (
				widget->priv->font_settings,
				"monospace-font-name");

		ms = pango_font_description_from_string (
				font ? font : "monospace 10");

		g_free (font);
	}

	if (vw == NULL) {
		gchar *font;

		font = g_settings_get_string (
				widget->priv->font_settings,
				"font-name");

		vw = pango_font_description_from_string (
				font ? font : "serif 10");

		g_free (font);
	}

	if (pango_font_description_get_size (ms) < pango_font_description_get_size (vw)) {
		min_size = ms;
	} else {
		min_size = vw;
	}

	stylesheet = g_string_new ("");
	g_string_append_printf (stylesheet,
		"body {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n",
		pango_font_description_get_family (vw),
		pango_font_description_get_size (vw) / PANGO_SCALE,
		pango_font_description_get_weight (vw),
		styles[pango_font_description_get_style (vw)]);

	if (widget->priv->aliasing_settings != NULL)
		aa = g_settings_get_string (
			widget->priv->aliasing_settings, "antialiasing");

	if (g_strcmp0 (aa, "none") == 0)
		smoothing = "none";
	else if (g_strcmp0 (aa, "grayscale") == 0)
		smoothing = "antialiased";
	else if (g_strcmp0 (aa, "rgba") == 0)
		smoothing = "subpixel-antialiased";

	if (smoothing != NULL)
		g_string_append_printf (
			stylesheet,
			" -webkit-font-smoothing: %s;\n",
			smoothing);

	g_free (aa);

	g_string_append (stylesheet, "}\n");

	g_string_append_printf (stylesheet,
		"pre,code,.pre {\n"
		"  font-family: '%s';\n"
		"  font-size: %dpt;\n"
		"  font-weight: %d;\n"
		"  font-style: %s;\n"
		"}",
		pango_font_description_get_family (ms),
		pango_font_description_get_size (ms) / PANGO_SCALE,
		pango_font_description_get_weight (ms),
		styles[pango_font_description_get_style (ms)]);

	context = gtk_widget_get_style_context (GTK_WIDGET (widget));
	gtk_style_context_get_style (context,
		"link-color", &link,
		"visited-link-color", &visited,
		NULL);

	if (link == NULL) {
		link = g_slice_new0 (GdkColor);
		link->blue = G_MAXINT16;
	}

	if (visited == NULL) {
		visited = g_slice_new0 (GdkColor);
		visited->red = G_MAXINT16;
	}

	g_string_append_printf (stylesheet,
		"a {\n"
		"  color: #%06x;\n"
		"}\n"
		"a:visited {\n"
		"  color: #%06x;\n"
		"}\n",
		e_color_to_value (link),
		e_color_to_value (visited));

	gdk_color_free (link);
	gdk_color_free (visited);

	base64 = g_base64_encode ((guchar *) stylesheet->str, stylesheet->len);
	g_string_free (stylesheet, TRUE);

	stylesheet = g_string_new ("data:text/css;charset=utf-8;base64,");
	g_string_append (stylesheet, base64);
	g_free (base64);

	settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (widget));
	g_object_set (G_OBJECT (settings),
		"default-font-size", pango_font_description_get_size (vw) / PANGO_SCALE,
		"default-font-family", pango_font_description_get_family (vw),
		"monospace-font-family", pango_font_description_get_family (ms),
		"default-monospace-font-size", (pango_font_description_get_size (ms) / PANGO_SCALE),
		"minimum-font-size", (pango_font_description_get_size (min_size) / PANGO_SCALE),
		"user-stylesheet-uri", stylesheet->str,
		NULL);

	g_string_free (stylesheet, TRUE);

	pango_font_description_free (ms);
	pango_font_description_free (vw);
}
