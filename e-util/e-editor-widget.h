/*
 * e-editor-widget.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EDITOR_WIDGET_H
#define E_EDITOR_WIDGET_H

#include <webkit/webkit.h>

#include <e-util/e-editor-selection.h>
#include <e-util/e-spell-checker.h>

/* Standard GObject macros */
#define E_TYPE_EDITOR_WIDGET \
	(e_editor_widget_get_type ())
#define E_EDITOR_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EDITOR_WIDGET, EEditorWidget))
#define E_EDITOR_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EDITOR_WIDGET, EEditorWidgetClass))
#define E_IS_EDITOR_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EDITOR_WIDGET))
#define E_IS_EDITOR_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EDITOR_WIDGET))
#define E_EDITOR_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EDITOR_WIDGET, EEditorWidgetClass))

G_BEGIN_DECLS

typedef enum {
	E_EDITOR_WIDGET_MODE_PLAIN_TEXT,
	E_EDITOR_WIDGET_MODE_HTML,
} EEditorWidgetMode;

typedef enum {
	E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE,
	E_EDITOR_WIDGET_REPLACE_ANSWER_REPLACE_ALL,
	E_EDITOR_WIDGET_REPLACE_ANSWER_CANCEL,
	E_EDITOR_WIDGET_REPLACE_ANSWER_NEXT
} EEditorWidgetReplaceAnswer;

typedef struct _EEditorWidget EEditorWidget;
typedef struct _EEditorWidgetClass EEditorWidgetClass;
typedef struct _EEditorWidgetPrivate EEditorWidgetPrivate;

struct _EEditorWidget {
	WebKitWebView parent;

	EEditorWidgetPrivate *priv;
};

struct _EEditorWidgetClass {
	WebKitWebViewClass parent_class;

	void		(*paste_clipboard_quoted)	(EEditorWidget *widget);

	gboolean	(*popup_event)			(EEditorWidget *widget,
							 GdkEventButton *event);
};

GType		e_editor_widget_get_type 	(void);

EEditorWidget *	e_editor_widget_new		(void);

EEditorSelection *
		e_editor_widget_get_selection	(EEditorWidget *widget);


gboolean	e_editor_widget_get_changed	(EEditorWidget *widget);
void		e_editor_widget_set_changed	(EEditorWidget *widget,
						 gboolean changed);

EEditorWidgetMode
		e_editor_widget_get_mode	(EEditorWidget *widget);
void		e_editor_widget_set_mode	(EEditorWidget *widget,
						 EEditorWidgetMode mode);

gboolean	e_editor_widget_get_inline_spelling
						(EEditorWidget *widget);
void		e_editor_widget_set_inline_spelling
						(EEditorWidget *widget,
						 gboolean inline_spelling);
gboolean	e_editor_widget_get_magic_links	(EEditorWidget *widget);
void		e_editor_widget_set_magic_links	(EEditorWidget *widget,
						 gboolean magic_links);
gboolean	e_editor_widget_get_magic_smileys
						(EEditorWidget *widget);
void		e_editor_widget_set_magic_smileys
						(EEditorWidget *widget,
						 gboolean magic_smileys);

GList *		e_editor_widget_get_spell_languages
						(EEditorWidget *widget);
void		e_editor_widget_set_spell_languages
						(EEditorWidget *widget,
						 GList *spell_languages);

ESpellChecker *	e_editor_widget_get_spell_checker
						(EEditorWidget *widget);

gchar *		e_editor_widget_get_text_html	(EEditorWidget *widget);
gchar *		e_editor_widget_get_text_plain	(EEditorWidget *widget);
void		e_editor_widget_set_text_html	(EEditorWidget *widget,
						 const gchar *text);
void		e_editor_widget_set_text_plain	(EEditorWidget *widget,
						 const gchar *text);

void		e_editor_widget_paste_clipboard_quoted
						(EEditorWidget *widget);

void		e_editor_widget_update_fonts	(EEditorWidget *widget);

G_END_DECLS

#endif /* E_EDITOR_WIDGET_H */
