#ifndef __E_NAME_WESTERN_TABLES_H__
#define __E_NAME_WESTERN_TABLES_H__

char *e_name_western_pfx_table[] = {

	/*
	 * English.
	 */
	"mister", "miss.", "mr.", "mrs.", "ms.",
	"miss", "mr", "mrs", "ms", "sir", 
	"professor", "prof.", "dr", "dr.", "doctor",
	"judge", "justice", "chief justice", 
	"congressman", "congresswoman", "commander",
	"lieutenant", "lt.", "colonel", "col.", "major", "maj.",
	"general", "gen.", "admiral", "admr.", "sergeant", "sgt.",
	"lord", "lady", "baron", "baroness", "duke", "duchess",
	"king", "queen", "prince", "princess",

	"the most honorable", "the honorable",
	"the reverend", "his holiness",
	"his eminence", "his majesty", "her majesty", 
	"his grace", "her grace",

	"president", "vice president", "secretary", "undersecretary",
	"consul", "ambassador",

	"senator", "saint", "st.", "pastor", "deacon",
	"father", "bishop", "archbishop", "cardinal", "pope",
	"reverend", "rev.", "rabbi", 

	/*
	 * French.
	 */
	"monsieur", "m.", "mademoiselle", "melle",
	"madame", "mme", "professeur", "dauphin", "dauphine",

	/*
	 * German
	 */
	"herr", "frau", "fraulein", "herr doktor", "doktor frau", "doktor frau doktor",
	"frau doktor",
 

	/*
	 * Spanish.
	 */
	"senor", "senora", "sra.", "senorita", "srita.", 

	NULL};

char *e_name_western_sfx_table[]  = {

	/*
	 * English.
	 */
         "junior", "senior", "jr", "sr", "I", "II", "III", "IV", "V",
	 "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV",
	 "XV", "XVI", "XVII", "XVIII", "XIX", "XX", "XXI", "XXII",
	 "phd", "ms", "md", "esq", "esq.", "esquire",

	NULL};

char *e_name_western_twopart_sfx_table[] = {

	/*
	 * English.
	 */
	"the first", "the second", "the third",

	NULL};

char *e_name_western_complex_last_table[] = {"van", "von", "de", "di", NULL};

#endif /* ! __E_NAME_WESTERN_TABLES_H__ */
