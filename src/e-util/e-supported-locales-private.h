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
 */

#ifndef E_SUPPORTED_LOCALES_PRIVATE_H
#define E_SUPPORTED_LOCALES_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

/* This should be included only in e-misc-utils.c and after e-misc-utils.h */

/* An array of known supported translations with corresponding locale codes */

static ESupportedLocales e_supported_locales[] = {
	{ "ab", "ab_AB" },
	{ "af", "af_ZA" },
	{ "am", "am_ET" },
	{ "an", "an_ES" },
	{ "ar", "ar_IN" },
	{ "as", "as_IN" },
	{ "ast", "ast_ES" },
	{ "az", "az_AZ" },
	{ "be", "be_BY" },
	{ "be@latin", "be_BY@latin" },
	{ "bg", "bg_BG" },
	{ "bn", "bn_IN" },
	{ "br", "br_FR" },
	{ "bs", "bs_BA" },
	{ "ca", "ca_ES" },
	{ "ca@valencia", "ca_ES@valencia" },
	{ "ckb", "ckb_IQ" },
	{ "cs", "cs_CZ" },
	{ "cy", "cy_GB" },
	{ "da", "da_DK" },
	{ "de", "de_DE" },
	{ "dz", "dz_BT" },
	{ "el", "el_GR" },
	{ "en", "en_US" },
	{ "en_AU", "en_AU" },
	{ "en_CA", "en_CA" },
	{ "en_GB", "en_GB" },
	{ "en@shaw", "en_US@shaw" },
	{ "eo", "eo_EO" },
	{ "es", "es_ES" },
	{ "et", "et_EE" },
	{ "eu", "eu_ES" },
	{ "fa", "fa_IR" },
	{ "fi", "fi_FI" },
	{ "fr", "fr_FR" },
	{ "fur", "fur_IT" },
	{ "ga", "ga_IE" },
	{ "gl", "gl_ES" },
	{ "gu", "gu_IN" },
	{ "he", "he_IL" },
	{ "hi", "hi_IN" },
	{ "hr", "hr_HR" },
	{ "hu", "hu_HU" },
	{ "id", "id_ID" },
	{ "ie", "ie_XX" },
	{ "is", "is_IS" },
	{ "it", "it_IT" },
	{ "ja", "ja_JP" },
	{ "ka", "ka_GE" },
	{ "kab", "kab_DZ" },
	{ "kk", "kk_KZ" },
	{ "kn", "kn_IN" },
	{ "ko", "ko_KR" },
	{ "ku", "ku_TR" },
	{ "lt", "lt_LT" },
	{ "lv", "lv_LV" },
	{ "mai", "mai_IN" },
	{ "mk", "mk_MK" },
	{ "ml", "ml_IN" },
	{ "mn", "mn_MN" },
	{ "mr", "mr_IN" },
	{ "ms", "ms_MY" },
	{ "nb", "nb_NO" },
	{ "nds", "nds_NL" },
	{ "ne", "ne_NP" },
	{ "nl", "nl_NL" },
	{ "nn", "nn_NO" },
	{ "oc", "oc_FR" },
	{ "or", "or_IN" },
	{ "pa", "pa_IN" },
	{ "pl", "pl_PL" },
	{ "ps", "ps_AF" },
	{ "pt", "pt_PT" },
	{ "pt_BR", "pt_BR" },
	{ "ro", "ro_RO" },
	{ "ru", "ru_RU" },
	{ "rw", "rw_RW" },
	{ "si", "si_LK" },
	{ "sk", "sk_SK" },
	{ "sl", "sl_SI" },
	{ "sq", "sq_AL" },
	{ "sr", "sr_RS" },
	{ "sr@latin", "sr_RS@latin" },
	{ "sv", "sv_FI" },
	{ "ta", "ta_IN" },
	{ "te", "te_IN" },
	{ "tg", "tg_TJ" },
	{ "th", "th_TH" },
	{ "tr", "tr_TR" },
	{ "ug", "ug_CN" },
	{ "uk", "uk_UA" },
	{ "vi", "vi_VN" },
	{ "wa", "wa_BE" },
	{ "xh", "xh_ZA" },
	{ "zh_CN", "zh_CN" },
	{ "zh_HK", "zh_HK" },
	{ "zh_TW", "zh_TW" },
	{ NULL, NULL }
};

G_END_DECLS

#endif /* E_SUPPORTED_LOCALES_PRIVATE_H */
