/* GnomeCard - a graphical contact manager.
 *
 * pairs.h: This file is part of GnomeCard.
 * 
 * Copyright (C) 1999 The Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __E_CARD_PAIRS_H__
#define __E_CARD_PAIRS_H__

#include "../libversit/vcc.h"
#include <e-card.h>

struct pair
{
	char *str;
	enum PropertyType id;
};

struct pair prop_lookup[] = {
		{        VCFullNameProp, PROP_FNAME },
		{            VCNameProp, PROP_NAME },
		{           VCPhotoProp, PROP_PHOTO },
		{       VCBirthDateProp, PROP_BDAY },
		{             VCAdrProp, PROP_DELADDR },
		{   VCDeliveryLabelProp, PROP_DELLABEL },
		{       VCTelephoneProp, PROP_PHONE },
		{    VCEmailAddressProp, PROP_EMAIL },
		{          VCMailerProp, PROP_MAILER },
		{        VCTimeZoneProp, PROP_TIMEZN },
		{             VCGeoProp, PROP_GEOPOS },
		{           VCTitleProp, PROP_TITLE },
		{    VCBusinessRoleProp, PROP_ROLE },
		{            VCLogoProp, PROP_LOGO },
		{           VCAgentProp, PROP_AGENT },
		{             VCOrgProp, PROP_ORG },
		{      VCCategoriesProp, PROP_CATEGORIES },
		{         VCCommentProp, PROP_COMMENT },
		{     VCLastRevisedProp, PROP_REV },
		{   VCPronunciationProp, PROP_SOUND },
		{             VCURLProp, PROP_URL },
		{    VCUniqueStringProp, PROP_UID },
		{         VCVersionProp, PROP_VERSION },
		{       VCPublicKeyProp, PROP_KEY },
		{           VCValueProp, PROP_VALUE },
		{        VCEncodingProp, PROP_ENCODING },
		{ VCQuotedPrintableProp, PROP_QUOTED_PRINTABLE },
		{            VC8bitProp, PROP_8BIT },
		{          VCBase64Prop, PROP_BASE64 },
		{        VCLanguageProp, PROP_LANG },
		{         VCCharSetProp, PROP_CHARSET },
		{ NULL, PROP_NONE} };

struct pair addr_pairs[] = {
		{ VCDomesticProp, ADDR_DOM },
		{ VCInternationalProp, ADDR_INTL },
		{ VCPostalProp, ADDR_POSTAL },
		{ VCParcelProp, ADDR_PARCEL },
		{ VCHomeProp, ADDR_HOME },
		{ VCWorkProp, ADDR_WORK },
		{ NULL, 0} };
	  
struct pair photo_pairs[] = {
		{ VCGIFProp, PHOTO_GIF },
		{ VCCGMProp, PHOTO_CGM },
		{ VCWMFProp, PHOTO_WMF },
		{ VCBMPProp, PHOTO_BMP },
		{ VCMETProp, PHOTO_MET },
		{ VCPMBProp, PHOTO_PMB },
		{ VCDIBProp, PHOTO_DIB },
		{ VCPICTProp, PHOTO_PICT },
		{ VCTIFFProp, PHOTO_TIFF },
		{ VCPDFProp, PHOTO_PDF },
		{ VCPSProp, PHOTO_PS },
		{ VCJPEGProp, PHOTO_JPEG },
		{ VCMPEGProp, PHOTO_MPEG },
		{ VCMPEG2Prop, PHOTO_MPEG2 },
		{ VCAVIProp, PHOTO_AVI },
		{ VCQuickTimeProp, PHOTO_QTIME },
		{ NULL, 0 } };

struct pair phone_pairs[] = {
		{ VCPreferredProp, PHONE_PREF },
		{ VCWorkProp, PHONE_WORK },
		{ VCHomeProp, PHONE_HOME },
		{ VCVoiceProp, PHONE_VOICE },
		{ VCFaxProp, PHONE_FAX },
		{ VCMessageProp, PHONE_MSG },
		{ VCCellularProp, PHONE_CELL },
		{ VCPagerProp, PHONE_PAGER },
		{ VCBBSProp, PHONE_BBS },
		{ VCModemProp, PHONE_MODEM },
		{ VCCarProp, PHONE_CAR },
		{ VCISDNProp, PHONE_ISDN },
		{ VCVideoProp, PHONE_VIDEO },
		{ NULL, 0 } };

struct pair email_pairs[] = {
		{ VCAOLProp, EMAIL_AOL },
		{ VCAppleLinkProp, EMAIL_APPLE_LINK },
		{ VCATTMailProp, EMAIL_ATT },
		{ VCCISProp, EMAIL_CIS },
		{ VCEWorldProp, EMAIL_EWORLD },
		{ VCInternetProp, EMAIL_INET },
		{ VCIBMMailProp, EMAIL_IBM },
		{ VCMCIMailProp, EMAIL_MCI },
		{ VCPowerShareProp, EMAIL_POWERSHARE },
		{ VCProdigyProp, EMAIL_PRODIGY },
		{ VCTLXProp, EMAIL_TLX },
		{ VCX400Prop, EMAIL_X400 },
		{ NULL, 0 } };

struct pair sound_pairs[] = {
		{ VCAIFFProp, SOUND_AIFF },
		{ VCPCMProp, SOUND_PCM },
		{ VCWAVEProp, SOUND_WAVE },
		{ NULL, 0 } };

struct pair key_pairs[] = {
		{ VCX509Prop, KEY_X509 },
		{ VCPGPProp, KEY_PGP },
		{ NULL, 0 } };
	  

#endif /* ! __E_CARD_PAIRS_H__ */
