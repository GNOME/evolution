/*
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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

