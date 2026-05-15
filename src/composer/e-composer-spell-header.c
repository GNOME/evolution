/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-composer-spell-header.h"

G_DEFINE_TYPE (
	EComposerSpellHeader,
	e_composer_spell_header,
	E_TYPE_COMPOSER_TEXT_HEADER)

static void
e_composer_spell_header_class_init (EComposerSpellHeaderClass *class)
{
	EComposerTextHeaderClass *composer_text_header_class;

	composer_text_header_class = E_COMPOSER_TEXT_HEADER_CLASS (class);
	composer_text_header_class->entry_type = E_TYPE_SPELL_ENTRY;
}

static void
e_composer_spell_header_init (EComposerSpellHeader *header)
{
}

EComposerHeader *
e_composer_spell_header_new_label (ESourceRegistry *registry,
                                   const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_SPELL_HEADER,
		"label", label, "button", FALSE,
		"registry", registry, NULL);
}

EComposerHeader *
e_composer_spell_header_new_button (ESourceRegistry *registry,
                                    const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_SPELL_HEADER,
		"label", label, "button", TRUE,
		"registry", registry, NULL);
}

