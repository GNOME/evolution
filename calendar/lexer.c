/*
 * lexer.c: Reads in the .calendar files
 */
#include <stdio.h>
#include <glib.h>
#include "cal_struct.h"


#define opener "["
#define closer "]"
#define VersionMajor 2

GSList *eventlist;

void print_glist(gpointer data, gpointer user_data)
{
	struct event *myevent = (struct event*)data;

	if (data == NULL)
		return;
	printf ("===============================================\nNew event\n");
	printf ("Start: %s %02d:%02d  End: %s %02d:%02d\n", myevent->start.date, myevent->start.time / 60, myevent->start.time % 60, myevent->end.date, myevent->end.time / 60, myevent->end.time % 60);
	printf ("Contents: %s\n", myevent->description);
	printf ("Repeat = %d (%d)", (int)(myevent->repeat), myevent->repeatcount);
}


int skip_chars(FILE *fp, char *terminator)
{
	int c;
	int cnt;

	cnt = 0;
	while( (c = fgetc(fp)) != EOF) {
		if (c == terminator[cnt]) {
			cnt++;
			if (terminator[cnt] == '\0')
				return TRUE;
		} else
			cnt = 0;
	}
	return FALSE;
}

int peek_char(FILE *fp, char *c)
{
	if ( ((*c) = fgetc(fp)) != EOF) {
		ungetc((*c), fp);
		return TRUE;
	} else	
		return FALSE;
}

int skip_whitespace(FILE *fp)
{
	int c;

	while( (c = fgetc(fp)) != EOF)
		if (!isspace(c)) {
			ungetc(c, fp);
			return TRUE;
		}
	return FALSE;
}

int get_until(FILE *fp, char terminator, char *buf) 
{
	int c;

	while( (c = fgetc(fp)) != EOF) {
		if (c == terminator) {
		        *buf = '\0';
			return TRUE;
	        }
		*buf = (char)c;
		buf++;
	}
	*buf = '\0';
	return FALSE;
}

int get_number(FILE *fp, int *x) 
{
	char buf[50];
	int c;
	int cnt;

	cnt = 0;
	buf[cnt] = '\0';
	while( (c= fgetc(fp)) != EOF) {
	   if (!isdigit(c)) {
	   	ungetc(c, fp);
	   	*x = atoi(buf);
	   	return TRUE;
	   }
	   buf[cnt++] = (char)c;
	   buf[cnt] = '\0';
	}
	*x = atoi(buf);
	return FALSE;
}	

/* Get string until EOF or closer_char */
int get_string(FILE *fp, char *string)
{
	int c;
	int cnt;

	cnt = 0;
	while ( (c = fgetc(fp)) != EOF) {
		if (c == closer[0]) {
			string[cnt] = '\0';
			ungetc((char)c, fp);
			return TRUE;
		}
		string[cnt++] = (char)c;
	}	
	return FALSE;
}

int get_dates(FILE *fp, char *keyword, struct event *ptr)
{
	char *c;
	int x;

	if (strncmp("Single", keyword, 6) == 0) {
		ptr->repeat = Single;
		/* It's a single date */
		if (! skip_whitespace(fp) || !get_until(fp, ' ', ptr->start.date))
			return FALSE;
		if (! skip_chars(fp, "End"))
			return FALSE;
		return TRUE;
	} else if (strncmp("Days", keyword, 4) == 0) {
		ptr->repeat = Days;
		if (! skip_whitespace(fp) || !get_until(fp, ' ', ptr->start.date))
			return FALSE;
		if (! skip_whitespace(fp) || !get_number(fp, &(ptr->repeatcount)))
			return FALSE;
		if (! skip_chars(fp, "End"))
			return FALSE;
		return TRUE;
	}

	return FALSE;
}

int getid(FILE *fp, char *string)
{
	int c;
	int cnt;

	cnt = 0;
	while( (c =fgetc(fp)) != EOF) {
		if (isalnum(c)) 
			string[cnt++] = (char)c;
		else {
			string[cnt] = '\0';
			return TRUE;
		}
	}
	string[cnt] = '\0';
	return FALSE;
}

int parse_appointment(FILE *fp, struct event *ptr, char keyword[])
{
	char buf[50];
	int x,y,c;

	if (strcmp(keyword, "Start") == 0) {
		if ( ! skip_whitespace(fp) || ! get_number(fp, &x) ) {
			g_error("Unable to get start time");
			return FALSE;
		}
		g_print ("Appointment start = %02d:%02d\n", x/60, x % 60);
		ptr->start.time = x;
		return TRUE;
	}

	if (strcmp(keyword, "Length") == 0) {
		if ( ! skip_whitespace(fp) || ! get_number(fp, &x) ) {
			g_error("Unable to get length");
			return FALSE;
		}
		g_print ("Appointment length = %d\n", x);
		ptr->end.time = ptr->start.time + x;
		return TRUE;
	}

	if (strcmp(keyword, "Alarms") == 0) {
		while(TRUE) {
			skip_whitespace(fp);
			if (!peek_char(fp, (char*)&c)) {
				g_error("Cannot read alarm list");
				return FALSE;
			}
			if (!isdigit(c))
				break;

			if (! get_number(fp, &x))
				return FALSE;

			g_print("New alarm %d\n", x);
		}
		return TRUE;
	}

	g_print("Unknown keyword %s\n", keyword);
	return FALSE;
}
			
int parse_item(FILE *fp, struct event *ptr, char keyword[]) 
{
	char buf[50];
	int x, y, c;

	if (strcmp(keyword, "Remind") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get remind level");
			return FALSE;
		}
		g_print("Remind level = %d\n", x);
		return TRUE;
	}

	if (strcmp(keyword, "Owner") == 0) {
		if (!get_string(fp, buf)) {
			g_error("Cannot get owner information");
			return FALSE;
		}
		g_print("Owner = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Uid") == 0) {
		if (!skip_whitespace(fp) || !get_until(fp, *closer, buf)) {
			g_error("Cannot get unique ID");
			return FALSE;
		}
		g_print("UID = %s\n", buf);
		return TRUE;
	}

	if (strcmp(keyword, "Contents") == 0) {
		if (!get_string(fp, buf)) {
			g_error("Cannot get item text");
			return FALSE;
		}
		g_print("Contents = %s\n", buf);
		strcpy(ptr->description,buf);
		return TRUE;
	}

	if (strcmp(keyword, "Text") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x) ||
		    (x < 0) || ! skip_whitespace(fp) || ! skip_chars(fp, opener) ) {
			g_error("Cannot get item text");
			return FALSE;
		}
		y = 0;
		while(y < x) {
			if ( (c = fgetc(fp)) == EOF) {
				g_error("Short item text");
				return FALSE;
			}
			buf[y++] = (char)c;
		}
		buf[y] = '\0';
		g_print("Text = %s\n", buf);
		return TRUE;
	}
	
	if (strcmp(keyword, "Dates") == 0) {
		if ( ! getid(fp, buf)) {
			g_error("Cannot get date");
			return FALSE;
		}
		return get_dates(fp, buf,ptr);
	}

	if (strcmp(keyword, "Deleted") == 0) {
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted day");
			return FALSE;
		}
		g_print("%d/", x);
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted month");
			return FALSE;
		}
		g_print("%d/", x);
		if (! skip_whitespace(fp) || ! get_number(fp, &x)) {
			g_error("Cannot get deleted year");
			return FALSE;
		}
		g_print("%d\n", x);
		return TRUE;
	}

	if (strcmp(keyword, "Hilite") == 0) {
		if (! get_string(fp, buf) ) {
			g_error("Cannot get hilite data");
			return FALSE;
		}
		g_print("Hilite = %s\n", buf);
		return  TRUE;
	}

	if (strcmp(keyword, "Todo") == 0) {
		g_print("Todo\n");
		return TRUE;
	}


	if (strcmp(keyword, "Done") == 0) {
		g_print("Done\n");
		return TRUE;
	}

	return FALSE;
}

void parse_ical_file(char const *file)
{
	FILE *fp;
	int finished;
	char keyword[50];
	int file_major, file_minor;
	char c;
	int item_type;
	int incomplete_item;
	struct event *myevent;

	if ( (fp = fopen(file, "r")) == NULL) {
		g_error("couldn't open file");
		return;
	}

	finished = FALSE;

	if (!skip_whitespace(fp))
		return;

	if (! skip_chars(fp, "Calendar") || ! skip_whitespace(fp) ) {
		g_error("unable to find calendar file");
		fclose(fp);
		return;
	}

	if (! skip_chars(fp, opener) || ! skip_chars(fp, "v") ) {
		g_error("Unable to get version line");
		fclose(fp);
		return;
	}
	if (! get_number(fp, &file_major) || ! (file_major >=0) || (file_major > VersionMajor)) {
		g_error("Missing/bad major version");
		fclose(fp);
		return;
	}

	if (! skip_chars(fp, ".") || ! get_number(fp, &file_minor) ||
	    ! skip_chars(fp, "]") || ! skip_whitespace(fp) ) {
		g_error("Missing  minor version");
		fclose(fp);
		return;
	}
	if (file_minor > 0) {
		g_error("Bad  minor version");
		fclose(fp);
		return;
	}

	while(TRUE) {
		g_print("----------------------------------------\n");
		item_type= 0;
		skip_whitespace(fp);
		if (! getid(fp,keyword) || ! skip_whitespace(fp) ||
		    ! skip_chars(fp, opener) || ! skip_whitespace(fp) ) {
			fclose(fp);
			return;
		}

		if (strcmp(keyword, "Appt") == 0) {
			g_print("New Appointment\n");
			item_type = 1;

		} else if (strcmp(keyword, "Note") == 0) {
			g_print("New Note\n");
			item_type = 2;
		} else 
			g_print("New ??? (%s)\n", keyword);

		incomplete_item = TRUE;
		myevent = g_malloc0(sizeof(struct event));
		while(incomplete_item) {
			if (! skip_whitespace(fp) || ! peek_char(fp, &c)) {
				g_warning("Incomplete item\n");
				fclose(fp);
				return;
			}
			if (c == closer[0]) {
				(void)fgetc(fp);
				g_print("done!\n");
				incomplete_item = FALSE;
				g_slist_append(eventlist, myevent);
				break;
			}
		
			if (! getid(fp,keyword) || ! skip_whitespace(fp) ||
			    ! skip_chars(fp, opener) ) {
				g_error("Error reading item property name");
				fclose(fp);
				return;
			}
			if ( ! parse_item(fp, myevent, keyword) && ! parse_appointment(fp, myevent, keyword) ) {
				g_warning("Unable to parse line\n");
				fclose(fp);
				return;
			}
			if ( ! skip_whitespace(fp) || ! skip_chars(fp, closer)) {
				g_error("Error reading item property");
				fclose(fp);
				return;
			}
		} /* while */
	} /* while */
}
		
	


int main(int argc, char *argv[])
{

	eventlist = g_slist_alloc();
	parse_ical_file("/home/csmall/.calendar");
	g_slist_foreach(eventlist, print_glist, NULL);
	return 0;
}

