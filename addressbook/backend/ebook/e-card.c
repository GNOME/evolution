/*
 * Authors: 
 *   Arturo Espinosa (arturo@nuclecu.unam.mx)
 *   Nat Friedman    (nat@helixcode.com)
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 1999 The Free Software Foundation
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "../libversit/vcc.h"
#include <e-card-pairs.h>
#include <e-card.h>

#define is_a_prop_of(obj,prop) isAPropertyOf (obj,prop)
#define str_val(obj) the_str = (vObjectValueType (obj))? fakeCString (vObjectUStringZValue (obj)) : calloc (1, 1)
#define has(obj,prop) (vo = isAPropertyOf (obj, prop))

static VObject *card_convert_to_vobject (Card *crd);

/*
 * ECard lifecycle management and vCard loading/saving.
 */

/**
 * e_card_new:
 */
ECard *
e_card_new (void)
{
	ECard *c;
	
	c = g_new0 (ECard, 1);
	
	c->fname      = 
	c->mailer     = 
	c->title      = 
	c->role       = 
	c->comment    = 
	c->categories = 
	c->url        = 
	c->uid        = e_card_prop_str_empty ();
	
	c->photo.type = PHOTO_JPEG;
	c->logo.type  = PHOTO_JPEG;
	c->rev.utc    = -1;
	c->sound.type = SOUND_PHONETIC;
	c->key.type   = KEY_PGP;

	c->categories.prop.encod = ENC_QUOTED_PRINTABLE;
	c->comment.prop.encod    = ENC_QUOTED_PRINTABLE;
	
	c->name.prop   = c->photo.prop = c->bday.prop     = c->timezn.prop   = 
	c->geopos.prop = c->logo.prop  = c->org.prop      = c->rev.prop      =
	c->sound.prop  = c->key.prop   = c->deladdr.prop  = c->dellabel.prop =
	c->phone.prop  = c->email.prop = c->xtension.prop = c->prop = e_card_prop_empty ();
	
	c->prop.type            = PROP_CARD;
	c->fname.prop.type      = PROP_FNAME;
	c->name.prop.type       = PROP_NAME;
	c->photo.prop.type      = PROP_PHOTO;
	c->bday.prop.type       = PROP_BDAY;
	
	c->deladdr.prop.type    = PROP_DELADDR_LIST;
	c->dellabel.prop.type   = PROP_DELLABEL_LIST;
	c->phone.prop.type      = PROP_PHONE_LIST;
	c->email.prop.type      = PROP_EMAIL_LIST;
	c->xtension.prop.type   = PROP_XTENSION_LIST;
	c->mailer.prop.type     = PROP_MAILER;
	c->timezn.prop.type     = PROP_TIMEZN;
	c->geopos.prop.type     = PROP_GEOPOS;
	c->title.prop.type      = PROP_TITLE;
	c->role.prop.type       = PROP_ROLE;
	c->logo.prop.type       = PROP_LOGO;
	c->org.prop.type        = PROP_ORG;
	c->categories.prop.type = PROP_CATEGORIES;
	c->comment.prop.type    = PROP_COMMENT;
	c->rev.prop.type        = PROP_REV;
	c->sound.prop.type      = PROP_SOUND;
	c->url.prop.type 	= PROP_URL;
	c->uid.prop.type 	= PROP_UID;
	c->key.prop.type 	= PROP_KEY;
	
	return c;
}

static void
e_card_str_free (CardStrProperty *sp)
{
	g_free (sp->str);

	e_card_prop_free (sp->prop);
}

static void
e_card_name_free (CardName *name)
{
	g_free (name->family);
	g_free (name->given);
	g_free (name->additional);
	g_free (name->prefix);
	g_free (name->suffix);

	e_card_prop_free (name->prop);
}

static void
e_card_photo_free (CardPhoto *photo)
{
	g_free (photo->data);

	e_card_prop_free (photo->prop);
}

/**
 * e_card_free:
 */
void
e_card_free (ECard *card)
{
	GList *l;

	g_return_if_fail (card != NULL);

	e_card_name_free (& card->name);
	e_card_str_free  (& card->fname);

	e_card_photo_free (card->photo);

	e_card_logo_free (card->logo);
	e_card_org_free (card->org);
	e_card_key_free (card->key);
	e_card_sound_free (card->sound);

	e_card_prop_str_free (& card->mailer);
	e_card_prop_str_free (& card->title);
	e_card_prop_str_free (& card->role);
	e_card_prop_str_free (& card->categories);
	e_card_prop_str_free (& card->comment);
	e_card_prop_str_free (& card->url);
	e_card_prop_str_free (& card->uid);

	/* address is a little more complicated */
	card_prop_free (card->deladdr.prop);
	while ((l = card->deladdr.l)) {

		e_card_deladdr_free ((CardDelAddr *) l->data);

		card->deladdr.l = g_list_remove_link (card->deladdr.l, l);
		g_list_free (l);
	}
	
	g_free (card);
}

typedef struct
{
	char c;
	int id;
	
	GList *sons;
} tree;

extern CardProperty 
e_card_prop_empty (void)
{
	CardProperty prop;
	
	prop.used = FALSE;
	
	prop.type = PROP_NONE;
	prop.encod = ENC_7BIT;
	prop.value = VAL_INLINE;
	prop.charset = NULL;
	prop.lang = NULL;
	prop.grp = NULL;
	prop.xtension = NULL;
	
	prop.user_data = NULL;
	
	return prop;
}

static CardStrProperty 
e_card_prop_str_empty (void)
{
	CardStrProperty strprop;
	
	strprop.prop = card_prop_empty ();
	strprop.str = NULL;
	
	return strprop;
}

/* Intended to check asserts. */
extern int card_check_prop (CardProperty prop)
{
	if (((prop.used == FALSE) || (prop.used == TRUE)) &&
	    ((prop.type >= PROP_NONE) && (prop.type <= PROP_LAST)) &&
	    ((prop.encod >= ENC_NONE) && (prop.encod <= ENC_LAST)) &&
	    ((prop.value >= VAL_NONE) && (prop.value <= VAL_LAST)))
	  return TRUE;
	    
	return FALSE;
}

extern void
card_prop_free (CardProperty prop)
{
	GList *l;
	
	g_free (prop.charset);
	g_free (prop.lang);
	
	for (l = prop.xtension; l; l = l->next) {
		CardXAttribute *xa = (CardXAttribute *) l->data;
		g_free (xa->name);
		g_free (xa->data);
	}
	
	g_list_free (l);
	
	prop.used = FALSE;
}
	
e_card_deladdr_free (ECardDelAddr *c)
{
	card_prop_free (c->prop);

	g_free (p->pobox);
	g_free (p->ext);
	g_free (p->street);
	g_free (p->city);
	g_free (p->region);
	g_free (p->code);
	g_free (p->country);
}

void 
card_free (Card *crd)
{
}

static tree *
new_tree (char c, int id)
{
	tree *t;
	
	t = malloc (sizeof (tree));
	t->c = c;
	t->id = id;
	t->sons = NULL;
	
	return t;
}

static void 
add_branch (tree *t, char *str, int id)
{
	tree *tmp;
	char *end;
	
	end = str + strlen (str) + 1;

	while (str != end) {
		tmp = new_tree (*str, id);
		t->sons = g_list_append (t->sons, (gpointer) tmp);
		t = tmp;
		
		str ++;
	}
}

static tree *
add_to_tree (tree *t, struct pair p)
{
	GList *node;
	char *c, *end;
	tree *tmp;
	
	  c = p.str;
	end = c + strlen (c) + 1;
	tmp = t;
	
	while (c != end) {
		for (node = tmp->sons; node; node = node->next)
		  if (((tree *) node->data)->c == *c) {
			  break;
		  }
		
		if (node) {
			tmp = (tree *) node->data;
			tmp->id = 0;
			c++;
		}
		else {
			add_branch (tmp, c, p.id);
			break;
		}
	}
	
	return t;
}
		
static tree *
create_search_tree (void)
{
	tree *t;
	int i;

	t = new_tree (0, 0);
	for (i = 0; prop_lookup[i].str; i++)
	  t = add_to_tree (t, prop_lookup[i]);
	
	return t;
}
		
static int 
card_lookup_name (const char *c)
{
	static tree *search_tree = NULL;
	GList *node;
	tree *tmp;
	const char *end;
	
	if (!search_tree)
	  search_tree = create_search_tree ();
	
	tmp = search_tree;
	end = c + strlen (c) + 1;
	
	while (tmp->id == 0 && c != end) {
		for (node = tmp->sons; node; node = node->next)
		  if (((tree *) node->data)->c == *c) {
			  break;
		  }
	
		if (node) {
			tmp = (tree *) node->data;
			c++;
		}
		else
		  return 0;
	}
	
	return tmp->id;
}

static enum PhotoType 
get_photo_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; photo_pairs[i].str; i++)
	  if (has (o, photo_pairs[i].str))
	    return photo_pairs[i].id;

	g_warning ("? < No PhotoType for Photo property. Falling back to JPEG.");
	return PHOTO_JPEG;
}

static int 
get_addr_type (VObject *o)
{
	VObject *vo;
	int ret = 0;
	int i;
	
	for (i = 0; addr_pairs[i].str; i++)
	  if (has (o, addr_pairs[i].str))
	    ret |= addr_pairs[i].id;
	
	return ret;
}

static int 
get_phone_type (VObject *o)
{
	VObject *vo;
	int ret = 0;
	int i;
	
	for (i = 0; phone_pairs[i].str; i++)
	  if (has (o, phone_pairs[i].str))
	    ret |= phone_pairs[i].id;
	
	return ret;
}

static enum EMailType 
get_email_type (VObject *o)
{
	VObject *vo;
	int i;

	for (i = 0; email_pairs[i].str; i++)
	  if (has (o, email_pairs[i].str))
	    return email_pairs[i].id;

	g_warning ("? < No EMailType for EMail property. Falling back to INET.");
	return EMAIL_INET;
}

static CardProperty 
get_CardProperty (VObject *o)
{
	VObjectIterator i;
	CardProperty prop;

	prop = card_prop_empty ();
	prop.used = TRUE;
	
	initPropIterator (&i, o);
	while (moreIteration (&i)) {
		VObject *vo = nextVObject (&i);
		const char *n = vObjectName (vo);
		int propid;
	
		propid = card_lookup_name (n);
		
		switch (propid) {
			
		 case PROP_VALUE:
		
			if (has (vo, VCContentIDProp))
			  prop.value = VAL_CID;
			else if (has (vo, VCURLValueProp))
			  prop.value = VAL_URL;
			break;
			
		 case PROP_ENCODING:
			if (has (vo, VCQuotedPrintableProp))
			  prop.encod = ENC_QUOTED_PRINTABLE;
			else if (has (vo, VC8bitProp))
			  prop.encod = ENC_8BIT;
			else if (has (vo, VCBase64Prop))
			  prop.encod = ENC_BASE64;
			break;
			
		 case PROP_QUOTED_PRINTABLE:
			prop.encod = ENC_QUOTED_PRINTABLE;
			break;
			
		 case PROP_8BIT:
			prop.encod = ENC_8BIT;
			break;
			
		 case PROP_BASE64:
			prop.encod = ENC_BASE64;
			break;
	
		 case PROP_LANG:
			if (vObjectValueType (vo)) {
				prop.lang = 
				  g_strdup (vObjectStringZValue (vo));
			} else
			  g_warning ("? < No value for LANG attribute.");
			break;
			
		 case PROP_CHARSET:
			if (vObjectValueType (vo)) {
				prop.charset = 
				  g_strdup (vObjectStringZValue (vo));
				g_warning (prop.charset); 
			} else
			  g_warning ("? < No value for CHARSET attribute.");
			break;
		 default:
				{
					CardXAttribute *c;

					c = malloc (sizeof (CardXAttribute));
					c->name = g_strdup (n);
					
					if (vObjectValueType (vo))
					  c->data = 
					  g_strdup (vObjectStringZValue (vo));
					else
					  c->data = NULL;
					
					prop.xtension = 
					  g_list_append (prop.xtension, c);
				}
		}
	}

	return prop;
}

static gboolean
e_card_prop_has (VObject    *o,
		 const char *id)
{
	g_assert (o  != NULL);
	g_assert (id != NULL);

	if (isAPropertyOf (o, id) == NULL)
		return FALSE;

	return TRUE;
}

static const char *
e_card_prop_get_str (VObject    *o,
		     const char *id)
{
	VObject *strobj;

	g_assert (o  != NULL);
	g_assert (id != NULL);

	strobj = isAPropertyOf (o, id);

	if (strobj == NULL)
		return g_strdup ("");

	if (vObjectValueType (strobj) != NULL) {
		char *str;
		char *g_str;

		str = fakeCString (vObjectStringZValue (strobj));
		g_str = g_strdup (str);
		free (str);

		return g_str;
	}

	return g_strdup ("");
}

static ECardName *
e_card_get_name (VObject *o)
{
	CardName *name;
	VObject *vo;
	char *the_str;

	name = g_new0 (ECardName, 1);

	name->family     = e_card_prop_get_substr (o, VCFamilyNameProp);
	name->given      = e_card_prop_get_substr (o, VCGivenNameProp);
	name->additional = e_card_prop_get_substr (o, VCAdditionalNamesProp);
	name->prefix     = e_card_prop_get_substr (o, VCNamePrefixesProp);
	name->suffix     = e_card_prop_get_substr (o, VCNameSuffixesProp);

	return name;
}

static CardBDay 
strtoCardBDay (char *str)
{
	char *s;
	int i;
	CardBDay bday;

	bday.year = 0;
	bday.month = 0;
	bday.day = 0;
	
	if (strchr (str, '-')) {
		for (s = strtok (str, "-"), i = 0; s; 
		     s = strtok (NULL, "-"), i++)
		  switch (i) {
		   case 0:
			  bday.year = atoi (s);
			  break;
		   case 1:
			  bday.month = atoi (s);
			  break;
		   case 2:
			  bday.day = atoi (s);
			  break;
		   default:
			  g_warning ("? < Too many values for BDay property.");
		  }
		
		if (i < 2)
		  g_warning ("? < Too few values for BDay property.");
	} else {
		if (strlen (str) >= 8) {
			bday.day = atoi (str + 6);
			str[6] = 0;
			bday.month = atoi (str + 4);
			str[4] = 0;
			bday.year = atoi (str);
		} else
		  g_warning ("? < Bad format for BDay property.");
	}
	
	return bday;
}

static ECardDelAddr *
e_card_get_del_addr (VObject *o)
{
	ECardDelAddr *addr;
	
	addr = g_new0 (ECardDelAddr, 1);
	
	addr->type    = get_addr_type (o);
	addr->po      = e_card_prop_get_substr (o, VCPostalBoxProp);
	addr->ext     = e_card_prop_get_substr (o, VCExtAddressProp);
	addr->street  = e_card_prop_get_substr (o, VCStreetAddressProp);
	addr->city    = e_card_prop_get_substr (o, VCCityProp);
	addr->region  = e_card_prop_get_substr (o, VCRegionProp);
	addr->code    = e_card_prop_get_substr (o, VCPostalBoxProp);
	addr->country = e_card_prop_get_substr (o, VCCountryNameProp);
	
	return addr;
}

static CardDelLabel *
get_CardDelLabel (VObject *o)
{
	CardDelLabel *dellabel;
	char *the_str;
	
	dellabel = malloc (sizeof (CardDelLabel));
	
	dellabel->type = get_addr_type (o);
	dellabel->data = g_strdup (str_val (o));
	
	free (the_str);
	return dellabel;
}

static CardPhone *
get_CardPhone (VObject *o)
{
	CardPhone *ret;
	char *the_str;
	
	ret = malloc (sizeof (CardPhone));
	ret->type = get_phone_type (o);
	ret->data = g_strdup (str_val (o));
	
	free (the_str);

	return ret;
}

static CardEMail *
get_CardEMail (VObject *o)
{
	CardEMail *ret;
	char *the_str; 
	
	ret = malloc (sizeof (CardEMail)); 
	ret->type = get_email_type (o);
	ret->data = g_strdup (str_val (o));
	
	free (the_str);
	
	return ret;
}
	
static CardTimeZone 
strtoCardTimeZone (char *str)
{
	char s[3];
	CardTimeZone tz;
	
	if (*str == '-') {
		tz.sign = -1;
		str++;
	} else
	  tz.sign = 1;
	
	tz.hours = 0;
	tz.mins = 0;
	
	s[2] = 0;
	if (strlen (str) > 2) {
		s[0] = str[0];
		s[1] = str[1];
		tz.hours = atoi (s);
	} else {
		g_warning ("? < TimeZone value is too short.");
		return tz;
	}
	
	str += 2;
	if (*str == ':')
	  str++;
	
	if (strlen (str) >= 2) {
		s[0] = str[0];
		s[1] = str[1];
		tz.mins = atoi (s);
	} else {
		g_warning ("? < TimeZone value is too short.");
		return tz;
	}
	
	if (strlen (str) > 3)
		g_warning ("? < TimeZone value is too long.");

	return tz;
}

static CardGeoPos 
strtoCardGeoPos (char *str)
{
	CardGeoPos gp;
	char *s;
	
	gp.lon = 0;
	gp.lat = 0;
	  
	s = strchr (str, ',');
	
	if (! s) {
		g_warning ("? < Bad format for GeoPos property.");
		return gp;
	}
	
	*s = 0;
	s++;
	
	gp.lon = atof (str);
	gp.lat = atof (s);
	
	return gp;
}
	
static CardOrg *
e_card_vobject_to_org (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardOrg *org;

	org = g_new0 (CardOrg, 1);

	if (has (o, VCOrgNameProp)) {
		org.name = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnitProp)) {
		org.unit1 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit2Prop)) {
		org.unit2 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit3Prop)) {
		org.unit3 = g_strdup (str_val (vo));
		free (the_str);
	}
	if (has (o, VCOrgUnit4Prop)) {
		org.unit4 = g_strdup (str_val (vo));
		free (the_str);
	}
	
	return org;
}

static CardXProperty *
get_XProp (VObject *o)
{
	char *the_str;
	CardXProperty *ret;
	
	ret = malloc (sizeof (CardXProperty)); 
	ret->name = g_strdup (vObjectName (o));
	ret->data = g_strdup (str_val (o));
	free (the_str);
	
	return ret;
}

static CardRev 
strtoCardRev (char *str)
{
	char s[3], *t, *ss;
	int len, i;
	CardRev rev;
	
	rev.utc = 0;
	len = strlen (str);
	
	if (str[len] == 'Z') {              /* Is it UTC? */
		rev.utc = 1;
		str[len] = 0;
	}
	  
	s[2] = 0;
	t = strchr (str, 'T');
	if (t) {                            /* Take the Time */
		*t = 0;
		t++;
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_hour = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}
		
		t += 2;
		if (*t == ':')             /* Ignore ':' separator */
		  t++;
		
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_min = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}
		
		t += 2;
		if (*t == ':')
		  t++;
		
		if (strlen (t) > 2) {
			s[0] = t[0];
			s[1] = t[1];
			rev.tm.tm_sec = atoi (s);
		} else {
			g_warning ("? < Rev value is too short.");
			return rev;
		}

		if (strlen (str) > 3)
		  g_warning ("? < Rev value is too long.");
		
	} else {
		g_warning ("? < No time value for Rev property.");
	}

	/* Now the date (the part before the T) */
	
	if (strchr (str, '-')) {                        /* extended iso 8601 */
		for (ss = strtok (str, "-"), i = 0; ss;
		     ss = strtok (NULL, "-"), i++)
		  switch (i) {
		   case 0:
			  rev.tm.tm_year = atoi (ss);
			  break;
		   case 1:
			  rev.tm.tm_mon = atoi (ss);
			  break;
		   case 2:
			  rev.tm.tm_mday = atoi (ss);
			  break;
		   default:
			  g_warning ("? < Too many values for Rev property.");
		  }
		
		if (i < 2)
		  g_warning ("? < Too few values for Rev property.");
	} else {
		if (strlen (str) >= 8) {             /* short representation */
			rev.tm.tm_mday = atoi (str + 6);
			str[6] = 0;
			rev.tm.tm_mon = atoi (str + 4);
			str[4] = 0;
			rev.tm.tm_year = atoi (str);
		} else
		  g_warning ("? < Bad format for Rev property.");
	}
	
	return rev;
}
		
static enum KeyType 
get_key_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; key_pairs[i].str; i++)
	  if (has (o, key_pairs[i].str))
	    return key_pairs[i].id;

	g_warning ("? < No KeyType for Key property. Falling back to PGP.");
	return KEY_PGP;
}

static CardPhoto 
get_CardPhoto (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardPhoto photo;

	photo.type = get_photo_type (o);
	
	if (has (o, VCDataSizeProp)) {
		photo.size = vObjectIntegerValue (vo);
		photo.data = malloc (photo.size);
		memcpy (photo.data, vObjectAnyValue (o), photo.size);
	} else {
		photo.size = strlen (str_val (o)) + 1;
		photo.data = g_strdup (the_str);
		free (the_str);
	}
	
	return photo;
}

static enum SoundType 
get_sound_type (VObject *o)
{
	VObject *vo;
	int i;
	
	for (i = 0; sound_pairs[i].str; i++)
	  if (has (o, sound_pairs[i].str))
	    return sound_pairs[i].id;

	return SOUND_PHONETIC;
}
	
static CardSound 
get_CardSound (VObject *o)
{
	VObject *vo;
	char *the_str;
	CardSound sound;

	sound.type = get_sound_type (o);
	
	if (has (o, VCDataSizeProp)) {
		sound.size = vObjectIntegerValue (vo);
		sound.data = malloc (sound.size);
		memcpy (sound.data, vObjectAnyValue (o), sound.size);
	} else {
		sound.size = strlen (str_val (o));
		sound.data = g_strdup (the_str);
		free (the_str);
	}
	
	return sound;
}

/* Loads our card contents from a VObject */
static ECard *
e_card_construct_from_vobject (ECard   *card,
			       VObject *vcrd)
{
	VObjectIterator i;
	Card *crd;
	char *the_str;

	initPropIterator (&i, vcrd);
	crd = card_new ();

	while (moreIteration (&i)) {
		VObject *o = nextVObject (&i);
		const char *n = vObjectName (o);
		int propid;
		CardProperty *prop = NULL;

		propid = card_lookup_name (n);
		
		switch (propid) {
		case PROP_FNAME:
			prop = &crd->fname.prop;
			crd->fname.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_NAME:
			prop = &crd->name.prop;
			crd->name = e_card_get_name (o);
			break;
		 case PROP_PHOTO:
			prop = &crd->photo.prop;
			crd->photo = get_CardPhoto (o);
			break;
		 case PROP_BDAY:
			prop = &crd->bday.prop;
			crd->bday = strtoCardBDay (str_val (o));
			free (the_str);
			break;
		 case PROP_DELADDR:
				{
					CardDelAddr *c;
					c = get_CardDelAddr (o);
					prop = &c->prop;
					crd->deladdr.l = g_list_append (crd->deladdr.l, c);
				}
			break;
		 case PROP_DELLABEL:
				{
					CardDelLabel *c;
					c = get_CardDelLabel (o);
					prop = &c->prop;
					crd->dellabel.l = g_list_append (crd->dellabel.l, c);
				}
			break;
		 case PROP_PHONE:
				{
					CardPhone *c;
					
					c = get_CardPhone (o);
					prop = &c->prop;
					crd->phone.l = g_list_append (crd->phone.l, c);
				}
			break;
		 case PROP_EMAIL:
				{
					CardEMail *c;
					
					c = get_CardEMail (o);
					prop = &c->prop;
					crd->email.l = g_list_append (crd->email.l, c);
				}
			break;
		 case PROP_MAILER:
			prop = &crd->mailer.prop;
			crd->mailer.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_TIMEZN:
			prop = &crd->timezn.prop;
			crd->timezn = strtoCardTimeZone (str_val (o));
			free (the_str);
			break;
		 case PROP_GEOPOS:
			prop = &crd->geopos.prop;
			crd->geopos = strtoCardGeoPos (str_val (o));
			break;
		 case PROP_TITLE:
			prop = &crd->title.prop;
			crd->title.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_ROLE:
			prop = &crd->role.prop;
			crd->role.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_LOGO:
			prop = &crd->logo.prop;
			crd->logo = get_CardPhoto (o);
			break;
		 case PROP_AGENT:
			crd->agent = card_create_from_vobject (o);
			break;
		 case PROP_ORG:
			prop = &crd->org.prop;
			crd->org = get_CardOrg (o);
			break;
		 case PROP_CATEGORIES:
			prop = &crd->categories.prop;
			crd->categories.str = g_strdup (str_val (o));
			crd->categories.prop.encod = ENC_QUOTED_PRINTABLE;
			free (the_str);
			break;
		 case PROP_COMMENT:
			prop = &crd->comment.prop;
			crd->comment.str = g_strdup (str_val (o));
			crd->comment.prop.encod = ENC_QUOTED_PRINTABLE;
			free (the_str);
			break;
		 case PROP_REV:
			prop = &crd->rev.prop;
			crd->rev = strtoCardRev (str_val (o));
			free (the_str);
			break;
		 case PROP_SOUND:
			prop = &crd->sound.prop;
			crd->sound = get_CardSound (o);
			break;
		 case PROP_URL:
			prop = &crd->url.prop;
			crd->url.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_UID:
			prop = &crd->uid.prop;
			crd->uid.str = g_strdup (str_val (o));
			free (the_str);
			break;
		 case PROP_VERSION:
				{
					char *str;
					str = str_val (o);
					if (strcmp (str, "2.1"))
					  g_warning ("? < Version doesn't match.");
					free (the_str);
				}
			break;
		 case PROP_KEY:
			prop = &crd->key.prop;
			crd->key.type = get_key_type (o);
			crd->key.data = g_strdup (str_val (o));
			free (the_str);
			break;
		 default:
				{
					CardXProperty *c;
				
					c = get_XProp (o);
					prop = &c->prop;
					crd->xtension.l = g_list_append (crd->xtension.l, c);
				}
			break;
		}
		
		if (prop) {
			*prop = get_CardProperty (o);
			prop->type = propid;
		}
	}
	
	return crd;
}
		
/* Loads a card from a file */
GList *
card_load (GList *crdlist, char *fname)
{
	VObject *vobj, *tmp;
	
	vobj = Parse_MIME_FromFileName (fname);
	if (!vobj) {
		g_warning ("Could not load the cardfile");
		return NULL;
	}

	while (vobj) {
		const char *n = vObjectName (vobj);
		
		if (strcmp (n, VCCardProp) == 0) {
			crdlist = g_list_append (crdlist, (gpointer)
					    card_create_from_vobject (vobj));
		}
		tmp = vobj;
		vobj = nextVObjectInList (vobj);
		cleanVObject (tmp);
	}

	cleanVObject (vobj);
	cleanStrTbl ();
	return crdlist;
}

static VObject *
add_strProp (VObject *o, const char *id, char *val)
{
	VObject *vo = NULL;
	
	if (val)
	  vo = addPropValue (o, id, val);

	return vo;
}

static VObject *
add_CardProperty (VObject *o, CardProperty *prop)
{
	GList *node;
	
	switch (prop->encod) {
	 case ENC_BASE64:
		addProp (o, VCBase64Prop);
		break;
	 case ENC_QUOTED_PRINTABLE:
		addProp (o, VCQuotedPrintableProp);
		break;
	 case ENC_8BIT:
		addProp (o, VC8bitProp);
		break;
	 case ENC_7BIT:
		/* Do nothing: 7BIT is the default. Avoids file clutter. */
		break;
	 default:
		g_warning ("? < Card had invalid encoding type.");
	}
	
	switch (prop->value) {
	 case VAL_CID:
		addProp (o, VCContentIDProp);
		break;
	 case VAL_URL:
		addProp (o, VCURLValueProp);
		break;
	 case VAL_INLINE:
		/* Do nothing: INLINE is the default. Avoids file clutter. */
		break;
	 default:
		g_warning ("? < Card had invalid value type.");
	}
	
	for (node = prop->xtension; node; node = node->next) {
		CardXAttribute *xa = (CardXAttribute *) node->data;
		if (xa->data)
		  addPropValue (o, xa->name, xa->data);
		else
		  addProp (o, xa->name);
	}

	add_strProp (o, VCCharSetProp, prop->charset);
	add_strProp (o, VCLanguageProp, prop->lang);
	
	return o;
}

static VObject *
add_CardStrProperty (VObject *vobj, const char *id, CardStrProperty *strprop)
{
	VObject *vprop;
	
	if (strprop->prop.used) {
		vprop = add_strProp (vobj, id, strprop->str);
		add_CardProperty (vprop, &strprop->prop);
	}
	
	return vobj;
}

static VObject *
add_PhotoType (VObject *o, enum PhotoType photo_type)
{
	int i;
	
	for (i = 0; photo_pairs[i].str; i++)
	  if (photo_type == photo_pairs[i].id) {
		  addProp (o, photo_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No PhotoType for Photo property. Falling back to JPEG.");
	addProp (o, VCJPEGProp);
	
	return o;
}

static VObject *
add_AddrType (VObject *o, int addr_type)
{
	int i;
	
	for (i = 0; addr_pairs[i].str; i++)
	  if (addr_type & addr_pairs[i].id)
	    addProp (o, addr_pairs[i].str);
	
	return o;
}

static void
add_strAddrType (GString *string, int addr_type)
{
	int i, first = 1;
	char *str;
	
	if (addr_type) {
		g_string_append (string, " (");
		
		for (i = 0; addr_pairs[i].str; i++)
		  if (addr_type & addr_pairs[i].id) {
			  if (!first)
			    g_string_append (string, ", ");
			  first = 0;
			  str = my_cap (addr_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_PhoneType (VObject *o, int phone_type)
{
	int i;
	
	for (i = 0; phone_pairs[i].str; i++)
	  if (phone_type & phone_pairs[i].id)
	    addProp (o, phone_pairs[i].str);
	
	return o;
}

static void
add_strPhoneType (GString *string, int phone_type)
{
	int i, first = 1;
	char *str;
	
	if (phone_type) {
		g_string_append (string, " (");
		
		for (i = 0; phone_pairs[i].str; i++)
		  if (phone_type & phone_pairs[i].id) {
			  if (!first)
			    g_string_append (string, ", ");
			  first = 0;
			  str = my_cap (phone_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_EMailType (VObject *o, enum EMailType email_type)
{
	int i;
	
	for (i = 0; email_pairs[i].str; i++)
	  if (email_type == email_pairs[i].id) {
		  addProp (o, email_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No EMailType for EMail property. Falling back to INET.");
	addProp (o, VCInternetProp);
	
	return o;
}

static void
add_strEMailType (GString *string, int email_type)
{
	int i;
	char *str;
	
	if (email_type) {
		g_string_append (string, " (");
		
		for (i = 0; email_pairs[i].str; i++)
		  if (email_type == email_pairs[i].id) {
			  str = my_cap (email_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
			  break;
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_KeyType (VObject *o, enum KeyType key_type)
{
	int i;
	
	for (i = 0; key_pairs[i].str; i++)
	  if (key_type == key_pairs[i].id) {
		  addProp (o, key_pairs[i].str);
		  return o;
	  }

	g_warning ("? > No KeyType for Key property. Falling back to PGP.");
	addProp (o, VCPGPProp);
	
	return o;
}

static void
add_strKeyType (GString *string, int key_type)
{
	int i;
	char *str;
	
	if (key_type) {
		g_string_append (string, " (");
		
		for (i = 0; key_pairs[i].str; i++)
		  if (key_type == key_pairs[i].id) {
			  str = my_cap (key_pairs[i].str);
			  g_string_append (string, str);
			  g_free (str);
			  break;
		  }
		
		g_string_append_c (string, ')');
	}
}

static VObject *
add_SoundType (VObject *o, enum SoundType sound_type)
{
	int i;
	
	for (i = 0; sound_pairs[i].str; i++)
	  if (sound_type == sound_pairs[i].id) {
		  addProp (o, sound_pairs[i].str);
		  return o;
	  }

	return o;
}

char *card_bday_str (CardBDay bday)
{
	char *str;
	
	str = malloc (12);
	snprintf (str, 12, "%04d-%02d-%02d", bday.year, bday.month, bday.day);
	
	return str;
}

char *card_timezn_str (CardTimeZone timezn)
{
	char *str;
	
	str = malloc (7);
	snprintf (str, 7, (timezn.sign == -1)? "-%02d:%02d" : "%02d:%02d",
		 timezn.hours, timezn.mins);
	return str;
}

char *card_geopos_str (CardGeoPos geopos)
{
	char *str;
	
	str = malloc (15);
	snprintf (str, 15, "%03.02f,%03.02f", geopos.lon, geopos.lat);
	return str;
}


static VObject *
card_convert_to_vobject (Card *crd)
{
	VObject *vobj, *vprop;
	
	vobj = newVObject (VCCardProp);

	add_CardStrProperty (vobj, VCFullNameProp, &crd->fname);
	if (crd->name.prop.used) {
		vprop = addProp (vobj, VCNameProp);
		add_strProp (vprop, VCFamilyNameProp, crd->name.family);
		add_strProp (vprop, VCGivenNameProp, crd->name.given);
		add_strProp (vprop, VCAdditionalNamesProp, crd->name.additional);
		add_strProp (vprop, VCNamePrefixesProp, crd->name.prefix);
		add_strProp (vprop, VCNameSuffixesProp, crd->name.suffix);
		add_CardProperty (vprop, &crd->name.prop);
	}
	
	if (crd->photo.prop.used) {
		vprop = addPropSizedValue (vobj, VCPhotoProp, 
					  crd->photo.data, crd->photo.size);
		add_PhotoType (vprop, crd->photo.type);
		add_CardProperty (vprop, &crd->photo.prop);
	}
	
	if (crd->bday.prop.used) {
		char *date_str;
		
		date_str = card_bday_str (crd->bday);
		vprop = addPropValue (vobj, VCBirthDateProp, date_str);
		free (date_str);
		add_CardProperty (vprop, &crd->bday.prop);
	}

	if (crd->xtension.l) {
		GList *node;
		
		for (node = crd->xtension.l; node; node = node->next) {
			CardXProperty *xp = (CardXProperty *) node->data;
			addPropValue (vobj, xp->name, xp->data);
			add_CardProperty (vobj, &xp->prop);
		}
	}
			
	
	if (crd->deladdr.l) {
		GList *node;
		
		for (node = crd->deladdr.l; node; node = node->next) {
			CardDelAddr *deladdr = (CardDelAddr *) node->data;
			
			if (deladdr->prop.used) {
				vprop = addProp (vobj, VCAdrProp);
				add_AddrType (vprop, deladdr->type);
			        add_strProp (vprop, VCPostalBoxProp, deladdr->po);
				add_strProp (vprop, VCExtAddressProp,deladdr->ext);
				add_strProp (vprop, VCStreetAddressProp,deladdr->street);
				add_strProp (vprop, VCCityProp, deladdr->city);
				add_strProp (vprop, VCRegionProp, deladdr->region);
				add_strProp (vprop, VCPostalCodeProp, deladdr->code);
				add_strProp (vprop, VCCountryNameProp, deladdr->country);
				add_CardProperty (vprop, &deladdr->prop);
			}
		}
	}
	
	if (crd->dellabel.l) {
		GList *node;
		
		for (node = crd->dellabel.l; node; node = node->next) {
			CardDelLabel *dellabel = (CardDelLabel *) node->data;
			
			vprop = add_strProp (vobj, VCDeliveryLabelProp, 
					    dellabel->data);
			add_AddrType (vprop, dellabel->type);
			add_CardProperty (vprop, &dellabel->prop);
		}
	}
	
	if (crd->phone.l) {
		GList *node;
		
		for (node = crd->phone.l; node; node = node->next) {
			CardPhone *phone = (CardPhone *) node->data;

			if (phone->prop.used) {
				vprop = add_strProp (vobj, VCTelephoneProp,
						    (phone->data)? 
						     phone->data: "");
				add_PhoneType (vprop, phone->type);
				add_CardProperty (vprop, &phone->prop);
			}
		}
	}

	if (crd->email.l) {
		GList *node;
		
		for (node = crd->email.l; node; node = node->next) {
			CardEMail *email = (CardEMail *) node->data;
			
			if (email->prop.used) {
				vprop = add_strProp (vobj, VCEmailAddressProp, 
						    email->data);
				add_EMailType (vprop, email->type);
				add_CardProperty (vprop, &email->prop);
			}
		}
	}

	add_CardStrProperty (vobj, VCMailerProp, &crd->mailer);
	
	if (crd->timezn.prop.used) {
		char *str;
		
		str = card_timezn_str (crd->timezn);
		vprop = addPropValue (vobj, VCTimeZoneProp, str);
		free (str);
		add_CardProperty (vprop, &crd->timezn.prop);
	}
	
	if (crd->geopos.prop.used) {
		char *str;
		
		str = card_geopos_str (crd->geopos);
		vprop = addPropValue (vobj, VCGeoLocationProp, str);
		free (str);
		add_CardProperty (vprop, &crd->geopos.prop);
	}
	
        add_CardStrProperty (vobj, VCTitleProp, &crd->title);
        add_CardStrProperty (vobj, VCBusinessRoleProp, &crd->role);
	
	if (crd->logo.prop.used) {
		vprop = addPropSizedValue (vobj, VCLogoProp, 
					  crd->logo.data, crd->logo.size);
		add_PhotoType (vprop, crd->logo.type);
		add_CardProperty (vprop, &crd->logo.prop);
	}
	
	if (crd->agent)
	  addVObjectProp (vobj, card_convert_to_vobject (crd->agent));
	
	if (crd->org.prop.used) {
		vprop = addProp (vobj, VCOrgProp);
		add_strProp (vprop, VCOrgNameProp, crd->org.name);
		add_strProp (vprop, VCOrgUnitProp, crd->org.unit1);
		add_strProp (vprop, VCOrgUnit2Prop, crd->org.unit2);
		add_strProp (vprop, VCOrgUnit3Prop, crd->org.unit3);
		add_strProp (vprop, VCOrgUnit4Prop, crd->org.unit4);
		add_CardProperty (vprop, &crd->org.prop);
	}
	
        add_CardStrProperty (vobj, VCCategoriesProp, &crd->categories);
        add_CardStrProperty (vobj, VCCommentProp, &crd->comment);
	
	if (crd->sound.prop.used) {
		if (crd->sound.type != SOUND_PHONETIC)
		  vprop = addPropSizedValue (vobj, VCPronunciationProp,
					    crd->sound.data, crd->sound.size);
		else
		  vprop = addPropValue (vobj, VCPronunciationProp, 
				       crd->sound.data);
		
		add_SoundType (vprop, crd->sound.type);
		add_CardProperty (vprop, &crd->sound.prop);
	}
	
        add_CardStrProperty (vobj, VCURLProp, &crd->url);
        add_CardStrProperty (vobj, VCUniqueStringProp, &crd->uid);
	
	if (crd->key.prop.used) {
		vprop = addPropValue (vobj, VCPublicKeyProp, crd->key.data);
		add_KeyType (vprop, crd->key.type);
		add_CardProperty (vprop, &crd->key.prop);
	}
	
	return vobj;
}

static void add_CardStrProperty_to_string (GString *string, char *prop_name,
					   CardStrProperty *strprop)
{
	if (strprop->prop.used) {
		if (prop_name)
		  g_string_append (string, prop_name);
		
		g_string_append (string, strprop->str);
	}
}

static void add_strProp_to_string (GString *string, char *prop_name, char *val)
{
	if (val) {
		if (prop_name)
		  g_string_append (string, prop_name);
		
		g_string_append (string, val);
	}
}

static void addProp_to_string (GString *string, char *prop_name)
{
	if (prop_name)
	  g_string_append (string, prop_name);
}

char *
card_to_string (Card *crd)
{
	GString *string;
	char *ret;
	
	string = g_string_new ("");
	
	add_CardStrProperty_to_string (string, _ ("Card: "), &crd->fname);
	if (crd->name.prop.used) {
		addProp_to_string (string, _ ("\nName: "));
		add_strProp_to_string (string, _ ("\n  Prefix:     "), crd->name.prefix);
		add_strProp_to_string (string, _ ("\n  Given:      "), crd->name.given);
		add_strProp_to_string (string, _ ("\n  Additional: "), crd->name.additional);
		add_strProp_to_string (string, _ ("\n  Family:     "), crd->name.family);
		add_strProp_to_string (string, _ ("\n  Suffix:     "), crd->name.suffix);
		g_string_append_c (string, '\n');
	}
	
/*	if (crd->photo.prop.used) {
		addPropSizedValue (string, _ ("\nPhoto: "), 
					  crd->photo.data, crd->photo.size);
		add_PhotoType (string, crd->photo.type);
	}*/
	
	if (crd->bday.prop.used) {
		char *date_str;
		
		date_str = card_bday_str (crd->bday);
		add_strProp_to_string (string, _ ("\nBirth Date: "), date_str);
		free (date_str);
	}
	
	if (crd->deladdr.l) {
		GList *node;
		
		for (node = crd->deladdr.l; node; node = node->next) {
			CardDelAddr *deladdr = (CardDelAddr *) node->data;
			
			if (deladdr->prop.used) {
				addProp_to_string (string, _ ("\nAddress:"));
				add_strAddrType (string, deladdr->type);
				add_strProp_to_string (string, _ ("\n  Postal Box:  "), deladdr->po);
				add_strProp_to_string (string, _ ("\n  Ext:         "),deladdr->ext);
				add_strProp_to_string (string, _ ("\n  Street:      "),deladdr->street);
				add_strProp_to_string (string, _ ("\n  City:        "), deladdr->city);
				add_strProp_to_string (string, _ ("\n  Region:      "), deladdr->region);
				add_strProp_to_string (string, _ ("\n  Postal Code: "), deladdr->code);
				add_strProp_to_string (string, _ ("\n  Country:     "), deladdr->country);
			}
		}
		
		g_string_append_c (string, '\n');
	}
	
	if (crd->dellabel.l) {
		GList *node;
		
		for (node = crd->dellabel.l; node; node = node->next) {
			CardDelLabel *dellabel = (CardDelLabel *) node->data;
			
			add_strProp_to_string (string, _ ("\nDelivery Label: "),
					    dellabel->data);
			add_strAddrType (string, dellabel->type);
		}
	}
	
	if (crd->phone.l) {
		GList *node;
		char *sep;
		
		if (crd->phone.l->next) {
			sep = "  ";
			g_string_append (string, _ ("\nTelephones:\n"));
		} else {
			sep = " ";
			g_string_append (string, _ ("\nTelephone:"));
		}
		
		for (node = crd->phone.l; node; node = node->next) {
			CardPhone *phone = (CardPhone *) node->data;

			if (phone->prop.used) {
				g_string_append (string, sep);
				g_string_append (string, phone->data);
				add_strPhoneType (string, phone->type);
				g_string_append_c (string, '\n');
			}
		}
		
		if (crd->phone.l->next)
		  g_string_append_c (string, '\n');
	}

	if (crd->email.l) {
		GList *node;
		char *sep;
		
		if (crd->email.l->next) {
			sep = "  ";
			g_string_append (string, _ ("\nE-mail:\n"));
		} else {
			sep = " ";
			g_string_append (string, _ ("\nE-mail:"));
		}
		
		
		for (node = crd->email.l; node; node = node->next) {
			CardEMail *email = (CardEMail *) node->data;
			
			if (email->prop.used) {
				g_string_append (string, sep);
				g_string_append (string, email->data);
				add_strEMailType (string, email->type);
				g_string_append_c (string, '\n');
			}
		}
		
		if (crd->email.l->next)
		  g_string_append_c (string, '\n');
	}

	add_CardStrProperty_to_string (string, _ ("\nMailer: "), &crd->mailer);
	
	if (crd->timezn.prop.used) {
		char *str;
		
		str = card_timezn_str (crd->timezn);
		add_strProp_to_string (string, _ ("\nTime Zone: "), str);
		free (str);
	}
	
	if (crd->geopos.prop.used) {
		char *str;
		
		str = card_geopos_str (crd->geopos);
		add_strProp_to_string (string, _ ("\nGeo Location: "), str);
		free (str);
	}
	
        add_CardStrProperty_to_string (string, _ ("\nTitle: "), &crd->title);
        add_CardStrProperty_to_string (string, _ ("\nBusiness Role: "), &crd->role);
	
/*	if (crd->logo.prop.used) {
		addPropSizedValue (string, _ ("\nLogo: "), 
					  crd->logo.data, crd->logo.size);
		add_PhotoType (string, crd->logo.type);
	}*/
	
/*	if (crd->agent)
	  addstringectProp (string, card_convert_to_stringect (crd->agent));*/
	
	if (crd->org.prop.used) {
		addProp_to_string (string, _ ("\nOrg: "));
		add_strProp_to_string (string, _ ("\n  Name:  "), crd->org.name);
		add_strProp_to_string (string, _ ("\n  Unit:  "), crd->org.unit1);
		add_strProp_to_string (string, _ ("\n  Unit2: "), crd->org.unit2);
		add_strProp_to_string (string, _ ("\n  Unit3: "), crd->org.unit3);
		add_strProp_to_string (string, _ ("\n  Unit4: "), crd->org.unit4);
		g_string_append_c (string, '\n');
	}
	
        add_CardStrProperty_to_string (string, _ ("\nCategories: "), &crd->categories);
        add_CardStrProperty_to_string (string, _ ("\nComment: "), &crd->comment);
	
/*	if (crd->sound.prop.used) {
		if (crd->sound.type != SOUND_PHONETIC)
		  addPropSizedValue (string, _ ("\nPronunciation: "),
					    crd->sound.data, crd->sound.size);
		else
		  add_strProp_to_string (string, _ ("\nPronunciation: "), 
				       crd->sound.data);
		
		add_SoundType (string, crd->sound.type);
	}*/
	
        add_CardStrProperty_to_string (string, _ ("\nURL: "), &crd->url);
        add_CardStrProperty_to_string (string, _ ("\nUnique String: "), &crd->uid);
	
	if (crd->key.prop.used) {
		add_strProp_to_string (string, _ ("\nPublic Key: "), crd->key.data);
		add_strKeyType (string, crd->key.type);
	}
	
	ret = g_strdup (string->str);
	g_string_free (string, TRUE);
	
	return ret;
}

char *
card_to_vobj_string (Card *crd)
{
	VObject *object;
	char *data, *ret_val;
	
	g_assert (crd != NULL);

	object = card_convert_to_vobject (crd);
	data = writeMemVObject (0, 0, object);
        ret_val = g_strdup (data);
	free (data);
		
	cleanVObject (object);

	return ret_val;
}

void 
card_save (Card *crd, FILE *fp)
{
	VObject *object;
	
	g_return_if_fail (crd != NULL);

	object = card_convert_to_vobject (crd);
	writeVObject (fp, object);
	cleanVObject (object);
}
