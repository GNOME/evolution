/* -*- Mode: C -*-
  ======================================================================
  FILE: icalrecur.c
  CREATOR: eric 16 May 2000
  
  $Id$
  $Locker$
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


  How this code works:

  Processing starts when the caller generates a new recurrence
  iterator via icalrecur_iterator_new(). This routine copies the
  recurrence rule into the iterator, extracts things like start and
  end dates. Then, it checks if the rule is legal, using some logic
  from RFC2445 and some logic that probably should be in RFC2445.

  Then, icalrecur_iterator_new() re-writes some of the BY*
  arrays. This involves ( via a call to setup_defaults() ) :

  1) For BY rule parts with no data ( ie BYSECOND was not specified )
  copy the corresponding time part from DTSTART into the BY array. (
  So impl->by_ptrs[BY_SECOND] will then have one element if is
  originally had none ) This only happens if the BY* rule part data
  would expand the number of occurrences in the occurrence set. This
  lets the code ignore DTSTART later on and still use it to get the
  time parts that were not specified in any other way.
  
  2) For the by rule part that are not the same interval as the
  frequency -- for HOURLY anything but BYHOUR, for instance -- copy the
  first data element from the rule part into the first occurrence. For
  example, for "INTERVAL=MONTHLY and BYHOUR=10,30", initialize the
  first time to be returned to have an hour of 10.

  Finally, for INTERVAL=YEARLY, the routine expands the rule to get
  all of the days specified in the rule. The code will do this for
  each new year, and this is the first expansion. This is a special
  case for the yearly interval; no other frequency gets expanded this
  way. The yearly interval is the most complex, so some special
  processing is required.

  After creating a new iterator, the caller will make successive calls
  to icalrecur_iterator_next() to get the next time specified by the
  rule. The main part of this routine is a switch on the frequency of
  the rule. Each different frequency is handled by a different
  routine. 

  For example, next_hour handles the case of INTERVAL=HOURLY, and it
  is called by other routines to get the next hour. First, the routine
  tries to get the next minute part of a time with a call to
  next_minute(). If next_minute() returns 1, it has reached the end of
  its data, usually the last element of the BYMINUTE array. Then, if
  there is data in the BYHOUR array, the routine changes the hour to
  the next one in the array. If INTERVAL=HOURLY, the routine advances
  the hour by the interval.

  If the routine used the last hour in the BYHOUR array, and the
  INTERVAL=HOURLY, then the routine calls increment_monthday() to set
  the next month day. The increment_* routines may call higher routine
  to increment the month or year also.

  The code for INTERVAL=DAILY is handled by next_day(). First, the
  routine tries to get the next hour part of a time with a call to
  next_hour. If next_hour() returns 1, it has reached the end of its
  data, usually the last element of the BYHOUR array. This means that
  next_day() should increment the time to the next day. If FREQUENCY==DAILY,
  the routine increments the day by the interval; otherwise, it
  increments the day by 1.

  Next_day() differs from next_hour because it does not use the BYDAY
  array to select an appropriate day. Instead, it returns every day (
  incrementing by 1 if the frequency is not DAILY with INTERVAL!=1)
  Any days that are not specified in an non-empty BYDAY array are
  filtered out later.

  Generally, the flow of these routine is for a next_* call a next_*
  routine of a lower interval ( next_day calls next_hour) and then to
  possibly call an increment_* routine of an equal or higher
  interval. ( next_day calls increment_monthday() )

  When the call to the original next_* routine returns,
  icalrecur_iterator_next() will check the returned data against other
  BYrule parts to determine if is should be excluded by calling
  check_contracting_rules. Generally, a contracting rule is any with a
  larger time span than the interval. For instance, if
  INTERVAL=DAILY, BYMONTH is a contracting rule part. 

  Check_contracting_rules() uses check_restriction() to do its
  work. Check_restriction() uses expand_map[] to determine if a rule
  is contracting, and if it is, and if the BY rule part has some data,
  then the routine checks if the value of a component of the time is
  part of the byrule part. For instance, for "INTERVAL=DAILY;
  BYMONTH=6,10", check_restriction() would check that the time value
  given to it has a month of either 6 or 10.
  icalrecurrencetype_test()

  Finally, icalrecur_iterator_next() does a few other checks on the
  time value, and if it passes, it returns the time.

 ======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icalrecur.h"
#include "icalerror.h"
#include "icalmemory.h"
#include <stdlib.h> /* for malloc */
#include <errno.h> /* for errno */
#include <string.h> /* for icalmemory_strdup */
#include <assert.h>
#include <limits.h> /* for SHRT_MAX */

#define TEMP_MAX 1024



enum byrule {
    NO_CONTRACTION = -1,
    BY_SECOND = 0,
    BY_MINUTE = 1,
    BY_HOUR = 2,
    BY_DAY = 3,
    BY_MONTH_DAY = 4,
    BY_YEAR_DAY = 5,
    BY_WEEK_NO = 6,
    BY_MONTH = 7,
    BY_SET_POS
};



struct icalrecur_iterator_impl {
	
	struct icaltimetype dtstart; 
	struct icaltimetype last; /* last time return from _iterator_next*/
	int occurrence_no; /* number of step made on this iterator */
	struct icalrecurrencetype rule;

	short days[366];
	short days_index;

	enum byrule byrule;
	short by_indices[9];


	short *by_ptrs[9]; /* Pointers into the by_* array elements of the rule */
};

int icalrecur_iterator_sizeof_byarray(short* byarray)
{
    int array_itr;

    for(array_itr = 0; 
	byarray[array_itr] != ICAL_RECURRENCE_ARRAY_MAX;
	array_itr++){
    }

    return array_itr;
}

enum expand_table {
    UNKNOWN  = 0,
    CONTRACT = 1,
    EXPAND =2,
    ILLEGAL=3
};

struct expand_split_map_struct 
{ 
	icalrecurrencetype_frequency frequency;

	/* Elements of the 'map' array correspond to the BYxxx rules:
           Second,Minute,Hour,Day,Month Day,Year Day,Week No,Month*/

	short map[8];
}; 

struct expand_split_map_struct expand_map[] =
{
    {ICAL_SECONDLY_RECURRENCE,1,1,1,1,1,1,1,1},
    {ICAL_MINUTELY_RECURRENCE,2,1,1,1,1,1,1,1},
    {ICAL_HOURLY_RECURRENCE,  2,2,1,1,1,1,1,1},
    {ICAL_DAILY_RECURRENCE,   2,2,2,1,1,1,1,1},
    {ICAL_WEEKLY_RECURRENCE,  2,2,2,2,3,3,1,1},
    {ICAL_MONTHLY_RECURRENCE, 2,2,2,2,2,3,3,1},
    {ICAL_YEARLY_RECURRENCE,  2,2,2,2,2,2,2,2},
    {ICAL_NO_RECURRENCE,      0,0,0,0,0,0,0,0}

};



/* Check that the rule has only the two given interday byrule parts. */
int icalrecur_two_byrule(struct icalrecur_iterator_impl* impl,
			 enum byrule one,enum byrule two)
{
    short test_array[9];
    enum byrule itr;
    int passes = 0;

    memset(test_array,0,9);

    test_array[one] = 1;
    test_array[two] = 1;

    for(itr = BY_DAY; itr != BY_SET_POS; itr++){

	if( (test_array[itr] == 0  &&
	     impl->by_ptrs[itr][0] != ICAL_RECURRENCE_ARRAY_MAX
	    ) ||
	    (test_array[itr] == 1  &&
	     impl->by_ptrs[itr][0] == ICAL_RECURRENCE_ARRAY_MAX
		) 
	    ) {
	    /* test failed */
	    passes = 0;
	}
    }

    return passes;

} 

/* Check that the rule has only the one given interdat byrule parts. */
int icalrecur_one_byrule(struct icalrecur_iterator_impl* impl,enum byrule one)
{
    int passes = 1;
    enum byrule itr;

    for(itr = BY_DAY; itr != BY_SET_POS; itr++){
	
	if ((itr==one && impl->by_ptrs[itr][0] == ICAL_RECURRENCE_ARRAY_MAX) ||
	    (itr!=one && impl->by_ptrs[itr][0] != ICAL_RECURRENCE_ARRAY_MAX)) {
	    passes = 0;
	}
    }

    return passes;
} 

int count_byrules(struct icalrecur_iterator_impl* impl)
{
    int count = 0;
    enum byrule itr;

    for(itr = BY_DAY; itr <= BY_SET_POS; itr++){
	if(impl->by_ptrs[itr][0] != ICAL_RECURRENCE_ARRAY_MAX){
	    count++;
	}
    }

    return count;
}


void setup_defaults(struct icalrecur_iterator_impl* impl, 
		    enum byrule byrule, icalrecurrencetype_frequency req,
		    short deftime, int *timepart)
{

    icalrecurrencetype_frequency freq;
    freq = impl->rule.freq;

    /* Re-write the BY rule arrays with data from the DTSTART time so
       we don't hav to explicitly deal with DTSTART */

    if(impl->by_ptrs[byrule][0] == ICAL_RECURRENCE_ARRAY_MAX &&
	expand_map[freq].map[byrule] != CONTRACT){
	impl->by_ptrs[byrule][0] = deftime;
    }

    /* Initialize the first occurence */
    if( freq != req && expand_map[freq].map[byrule] != CONTRACT){
	*timepart = impl->by_ptrs[byrule][0];
    }


}

int expand_year_days(struct icalrecur_iterator_impl* impl,short year);


icalrecur_iterator* icalrecur_iterator_new(struct icalrecurrencetype rule, struct icaltimetype dtstart)
{
    struct icalrecur_iterator_impl* impl;
    icalrecurrencetype_frequency freq;

    if ( ( impl = (struct icalrecur_iterator_impl *)
	   malloc(sizeof(struct icalrecur_iterator_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(impl,0,sizeof(struct icalrecur_iterator_impl));

    impl->rule = rule;
    impl->last = dtstart;
    impl->dtstart = dtstart;
    impl->days_index =0;
    impl->occurrence_no = 0;
    freq = impl->rule.freq;

    /* Set up convienience pointers to make the code simpler. Allows
       us to iterate through all of the BY* arrays in the rule. */

    impl->by_ptrs[BY_MONTH]=impl->rule.by_month;
    impl->by_ptrs[BY_WEEK_NO]=impl->rule.by_week_no;
    impl->by_ptrs[BY_YEAR_DAY]=impl->rule.by_year_day;
    impl->by_ptrs[BY_MONTH_DAY]=impl->rule.by_month_day;
    impl->by_ptrs[BY_DAY]=impl->rule.by_day;
    impl->by_ptrs[BY_HOUR]=impl->rule.by_hour;
    impl->by_ptrs[BY_MINUTE]=impl->rule.by_minute;
    impl->by_ptrs[BY_SECOND]=impl->rule.by_second;
    impl->by_ptrs[BY_SET_POS]=impl->rule.by_set_pos;


    /* Check if the recurrence rule is legal */

    /* If the BYYEARDAY appears, no other date rule part may appear.   */

    if(icalrecur_two_byrule(impl,BY_YEAR_DAY,BY_MONTH) ||
       icalrecur_two_byrule(impl,BY_YEAR_DAY,BY_WEEK_NO) ||
       icalrecur_two_byrule(impl,BY_YEAR_DAY,BY_MONTH_DAY) ||
       icalrecur_two_byrule(impl,BY_YEAR_DAY,BY_DAY) ){

	icalerror_set_errno(ICAL_USAGE_ERROR);

	return 0;
    }

    /* BYWEEKNO and BYMONTH rule parts may not both appear.*/

    if(icalrecur_two_byrule(impl,BY_WEEK_NO,BY_MONTH)){
	icalerror_set_errno(ICAL_USAGE_ERROR);

	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }

    /* BYWEEKNO and BYMONTHDAY rule parts may not both appear.*/

    if(icalrecur_two_byrule(impl,BY_WEEK_NO,BY_MONTH_DAY)){
	icalerror_set_errno(ICAL_USAGE_ERROR);

	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }


    /*For MONTHLY recurrences (FREQ=MONTHLY) neither BYYEARDAY nor
      BYWEEKNO may appear. */

    if(freq == ICAL_MONTHLY_RECURRENCE && 
       ( icalrecur_one_byrule(impl,BY_WEEK_NO) ||
	 icalrecur_one_byrule(impl,BY_YEAR_DAY)) ) {

	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }


    /*For WEEKLY recurrences (FREQ=WEEKLY) neither BYMONTHDAY nor
      BYYEARDAY may appear. */

    if(freq == ICAL_WEEKLY_RECURRENCE && 
       ( icalrecur_one_byrule(impl,BY_MONTH_DAY) ||
	 icalrecur_one_byrule(impl,BY_YEAR_DAY)) ) {
	
	icalerror_set_errno(ICAL_USAGE_ERROR);
	return 0;
    }


    /* Rewrite some of the rules and set up defaults to make later
       processing easier */


    setup_defaults(impl,BY_SECOND,ICAL_SECONDLY_RECURRENCE,impl->dtstart.second,
		   &(impl->last.second));

    setup_defaults(impl,BY_MINUTE,ICAL_MINUTELY_RECURRENCE,impl->dtstart.minute,
		   &(impl->last.minute));

    setup_defaults(impl,BY_HOUR,ICAL_HOURLY_RECURRENCE,impl->dtstart.hour,
		   &(impl->last.hour));

    setup_defaults(impl,BY_MONTH_DAY,ICAL_DAILY_RECURRENCE,impl->dtstart.day,
		   &(impl->last.day));

    setup_defaults(impl,BY_MONTH,ICAL_MONTHLY_RECURRENCE,impl->dtstart.month,
		   &(impl->last.month));

    if(impl->rule.freq == ICAL_WEEKLY_RECURRENCE &&
       impl->by_ptrs[BY_DAY][0] == ICAL_RECURRENCE_ARRAY_MAX){
	impl->by_ptrs[BY_DAY][0] = icaltime_day_of_week(impl->dtstart);
    }


    if(impl->rule.freq == ICAL_YEARLY_RECURRENCE){
	expand_year_days(impl,impl->dtstart.year);
    }

    return impl;
}


void icalrecur_iterator_free(icalrecur_iterator* i)
{
    
    struct icalrecur_iterator_impl* impl = 
	(struct icalrecur_iterator_impl*)i;

    icalerror_check_arg_rv((impl!=0),"impl");

    free(impl);

}




void increment_year(struct icalrecur_iterator_impl* impl, int inc)
{
    impl->last.year+=inc;
}




void increment_month(struct icalrecur_iterator_impl* impl, int inc)
{
    impl->last.month+=inc;

    if (impl->last.month > 12){
	impl->last.month = 1;
	increment_year(impl,1);
    }
}

void increment_monthday(struct icalrecur_iterator_impl* impl, int inc)
{
    
    short days_in_month = icaltime_days_in_month(impl->last.month,impl->last.year);

    impl->last.day+=inc;

    if (impl->last.day > days_in_month){
	int md = impl->last.day -days_in_month;
	impl->last.day = md;
	increment_month(impl,1);
    }

}


void increment_hour(struct icalrecur_iterator_impl* impl, int inc)
{
    impl->last.hour+=inc;

    if (impl->last.hour > 24){
	impl->last.hour = 0;
	increment_monthday(impl,1);
    }

}

void increment_minute(struct icalrecur_iterator_impl* impl, int inc)
{
    impl->last.minute+=inc;

    if (impl->last.minute > 59){
	impl->last.minute = 0;
	increment_hour(impl,1);
    }

}

void increment_second(struct icalrecur_iterator_impl* impl, int inc)
{
    impl->last.second+=inc;

    if (impl->last.second > 59){
	impl->last.second = 0;
	increment_minute(impl,1);
    }

}


short next_second(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_SECOND][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_SECONDLY_RECURRENCE);

  short end_of_data = 0;

  assert(has_by_data || this_frequency);

  if(  has_by_data ){
      /* Ignore the frequency and use the byrule data */

      impl->by_indices[BY_SECOND]++;

      if (impl->by_ptrs[BY_SECOND][impl->by_indices[BY_SECOND]]
	  ==ICAL_RECURRENCE_ARRAY_MAX){
	  impl->by_indices[BY_SECOND] = 0;

	  end_of_data = 1;
      }


      impl->last.second = 
	  impl->by_ptrs[BY_SECOND][impl->by_indices[BY_SECOND]];
      
      
  } else if( !has_by_data &&  this_frequency ){
      /* Compute the next value from the last time and the frequency interval*/
      increment_second(impl, impl->rule.interval);

  }

  /* If we have gone through all of the seconds on the BY list, then we
     need to move to the next minute */

  if(has_by_data && end_of_data && this_frequency ){
      increment_minute(impl,1);
  }

  return end_of_data;

}

int next_minute(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_MINUTE][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_MINUTELY_RECURRENCE);

  short end_of_data = 0;

  assert(has_by_data || this_frequency);


  if (next_second(impl) == 0){
      return 0;
  }

  if(  has_by_data ){
      /* Ignore the frequency and use the byrule data */

      impl->by_indices[BY_MINUTE]++;
      
      if (impl->by_ptrs[BY_MINUTE][impl->by_indices[BY_MINUTE]]
	  ==ICAL_RECURRENCE_ARRAY_MAX){

	  impl->by_indices[BY_MINUTE] = 0;
	  
	  end_of_data = 1;
      }

      impl->last.minute = 
	  impl->by_ptrs[BY_MINUTE][impl->by_indices[BY_MINUTE]];

  } else if( !has_by_data &&  this_frequency ){
      /* Compute the next value from the last time and the frequency interval*/
      increment_minute(impl,impl->rule.interval);
  } 

/* If we have gone through all of the minutes on the BY list, then we
     need to move to the next hour */

  if(has_by_data && end_of_data && this_frequency ){
      increment_hour(impl,1);
  }

  return end_of_data;
}

int next_hour(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_HOUR][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_HOURLY_RECURRENCE);

  short end_of_data = 0;

  assert(has_by_data || this_frequency);

  if (next_minute(impl) == 0){
      return 0;
  }

  if(  has_by_data ){
      /* Ignore the frequency and use the byrule data */

      impl->by_indices[BY_HOUR]++;
      
      if (impl->by_ptrs[BY_HOUR][impl->by_indices[BY_HOUR]]
	  ==ICAL_RECURRENCE_ARRAY_MAX){
	  impl->by_indices[BY_HOUR] = 0;
	  
	  end_of_data = 1;
      }

      impl->last.hour = 
	  impl->by_ptrs[BY_HOUR][impl->by_indices[BY_HOUR]];

  } else if( !has_by_data &&  this_frequency ){
      /* Compute the next value from the last time and the frequency interval*/
      increment_hour(impl,impl->rule.interval);

  }

  /* If we have gone through all of the hours on the BY list, then we
     need to move to the next day */

  if(has_by_data && end_of_data && this_frequency ){
      increment_monthday(impl,1);
  }

  return end_of_data;

}

int next_day(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_DAY][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_DAILY_RECURRENCE);

  assert(has_by_data || this_frequency);

  if (next_hour(impl) == 0){
      return 0;
  }

  /* Always increment through the interval, since this routine is not
     called by any other next_* routine, and the days that are
     excluded will be taken care of by restriction filtering */

  if(this_frequency){
      increment_monthday(impl,impl->rule.interval);
  } else {
      increment_monthday(impl,1);
  }


  return 0;

}

/* This routine is only called by next_month and next_year, so it does
   not have a clause for this_frequency */
int next_monthday(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_MONTH_DAY][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short mday;
  short end_of_data = 0;

  assert(has_by_data );

  if (next_hour(impl) == 0){
      return 0;
  }

  impl->by_indices[BY_MONTH_DAY]++;
  
  mday = impl->by_ptrs[BY_MONTH_DAY][impl->by_indices[BY_MONTH_DAY]];

  if ( mday ==ICAL_RECURRENCE_ARRAY_MAX){
      impl->by_indices[BY_MONTH_DAY] = 0;
      
      end_of_data = 1;
  }

  if (mday > 0){
      impl->last.day = mday;
  } else {
      short days_in_month = icaltime_days_in_month(impl->last.month,
						   impl->last.year);
      impl->last.day = days_in_month-mday+1;
  }

  if(has_by_data && end_of_data ){
      increment_month(impl,1);
  }

  return end_of_data;

}

int next_yearday(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_YEAR_DAY][0]!=ICAL_RECURRENCE_ARRAY_MAX);

  short end_of_data = 0;

  assert(has_by_data );

  if (next_hour(impl) == 0){
      return 0;
  }

  impl->by_indices[BY_YEAR_DAY]++;
  
  if (impl->by_ptrs[BY_YEAR_DAY][impl->by_indices[BY_YEAR_DAY]]
      ==ICAL_RECURRENCE_ARRAY_MAX){
      impl->by_indices[BY_YEAR_DAY] = 0;
      
      end_of_data = 1;
  }
  
  impl->last.day = 
      impl->by_ptrs[BY_YEAR_DAY][impl->by_indices[BY_YEAR_DAY]];
  
  if(has_by_data && end_of_data){
      increment_year(impl,1);
  }

  return end_of_data;

}

/* This routine is only called by next_week or next_month, so it does
not have a clause for this_frequency. In both cases, it is certain
that BY_DAY has data */

int next_weekday(struct icalrecur_iterator_impl* impl)
{

  short end_of_data = 0;
  short start_of_week, dow;
  struct icaltimetype next;

  if (next_hour(impl) == 0){
      return 0;
  }

  assert( impl->by_ptrs[BY_DAY][0]!=ICAL_RECURRENCE_ARRAY_MAX);

  impl->by_indices[BY_DAY]++;
  
  if (impl->by_ptrs[BY_DAY][impl->by_indices[BY_DAY]]
      ==ICAL_RECURRENCE_ARRAY_MAX){
      impl->by_indices[BY_DAY] = 0;
      
      end_of_data = 1;
  }

  dow = impl->by_ptrs[BY_DAY][impl->by_indices[BY_DAY]];
  
  start_of_week = icaltime_start_doy_of_week(impl->last);
  next = icaltime_from_day_of_year(start_of_week + dow - 1,impl->last.year);

  impl->last.day =  next.day;
  impl->last.month =  next.month;
  
  return end_of_data;

}

int next_month(struct icalrecur_iterator_impl* impl)
{

  short has_by_data = (impl->by_ptrs[BY_MONTH][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_MONTHLY_RECURRENCE);

  short end_of_data = 0;

  assert(has_by_data || this_frequency);

  /* Week day data overrides monthday data */
  if(impl->by_ptrs[BY_DAY][0]!=ICAL_RECURRENCE_ARRAY_MAX){
      if (next_weekday(impl) == 0){
	  return 0;
      }
  } else {
      if (next_monthday(impl) == 0){
	  return 0;
      }
  }

  if(  has_by_data ){
      /* Ignore the frequency and use the byrule data */

      impl->by_indices[BY_MONTH]++;
      
      if (impl->by_ptrs[BY_MONTH][impl->by_indices[BY_MONTH]]
	  ==ICAL_RECURRENCE_ARRAY_MAX){
	  impl->by_indices[BY_MONTH] = 0;
	  
	  end_of_data = 1;
      }

      impl->last.month = 
	  impl->by_ptrs[BY_MONTH][impl->by_indices[BY_MONTH]];

  } else if( !has_by_data &&  this_frequency ){
      /* Compute the next value from the last time and the frequency interval*/
      increment_month(impl,impl->rule.interval);

  }

  if(has_by_data && end_of_data && this_frequency ){
      increment_year(impl,1);
  }
  return end_of_data;

}


int next_week(struct icalrecur_iterator_impl* impl)
{
  short has_by_data = (impl->by_ptrs[BY_WEEK_NO][0]!=ICAL_RECURRENCE_ARRAY_MAX);
  short this_frequency = (impl->rule.freq == ICAL_WEEKLY_RECURRENCE);
  short end_of_data = 0;

  int sec_in_week = 60*60*24*7;

  if (next_weekday(impl) == 0){
      return 0;
  }

  if( impl->by_ptrs[BY_WEEK_NO][0]!=ICAL_RECURRENCE_ARRAY_MAX){
    /* Use the Week Number byrule data */
      int week_no;
      time_t tt;
      struct icaltimetype t;
      
      impl->by_indices[BY_WEEK_NO]++;
      
      if (impl->by_ptrs[BY_WEEK_NO][impl->by_indices[BY_WEEK_NO]]
	  ==ICAL_RECURRENCE_ARRAY_MAX){
	  impl->by_indices[BY_WEEK_NO] = 0;
	  
	  end_of_data = 1;
      }
      
      t = impl->last;
      t.month=1; /* HACK, should be setting to the date of the first week of year*/
      t.day=1;
      
      week_no = impl->by_ptrs[BY_WEEK_NO][impl->by_indices[BY_WEEK_NO]];
      
      tt = icaltime_as_timet(impl->last);
    
      tt+=sec_in_week*week_no;
      
      impl->last = icaltime_from_timet(tt,impl->last.is_date,impl->last.is_utc);
      
  } else if( !has_by_data &&  this_frequency ){
      increment_monthday(impl,7*impl->rule.interval);
  }

  if(has_by_data && end_of_data && this_frequency ){
      increment_year(impl,1);
  }

  return end_of_data;
  
}

int has_by_data(struct icalrecur_iterator_impl* impl, enum byrule byrule){

    return (impl->by_ptrs[byrule][0] != ICAL_RECURRENCE_ARRAY_MAX);
}


/* For INTERVAL=YEARLY, set up the days[] array in the iterator to
   list all of the days of the current year that are specified in this
   rule. */

int expand_year_days(struct icalrecur_iterator_impl* impl,short year)
{
    int j,k;
    int days_index=0;
    struct icaltimetype t;


    memset(&t,0,sizeof(t));
    memset(impl->days,ICAL_RECURRENCE_ARRAY_MAX_BYTE,sizeof(impl->days));
    
    if(has_by_data(impl,BY_MONTH) && !has_by_data(impl,BY_MONTH_DAY)){
	
        for(j=0;impl->by_ptrs[BY_MONTH][j]!=ICAL_RECURRENCE_ARRAY_MAX;j++){
	    struct icaltimetype t;
	    short month = impl->by_ptrs[BY_MONTH][j];	    
            short doy;

	    t = impl->dtstart;
	    t.year = year;
	    t.month = month;

	    doy = icaltime_day_of_year(t);
	    
            impl->days[days_index++] = doy;

        }


    }
    else if ( has_by_data(impl,BY_MONTH) && has_by_data(impl,BY_DAY)){

        for(j=0;impl->by_ptrs[BY_MONTH][j]!=ICAL_RECURRENCE_ARRAY_MAX;j++){
	    short month = impl->by_ptrs[BY_MONTH][j];
	    short days_in_month = icaltime_days_in_month(month,year);
		
	    struct icaltimetype t;
	    memset(&t,0,sizeof(struct icaltimetype));
	    t.day = 1;
	    t.year = year;
	    t.month = month;

	    for(t.day = 1; t.day <=days_in_month; t.day++){
		
		short current_dow = icaltime_day_of_week(t);
		
		for(k=0;impl->by_ptrs[BY_DAY][k]!=ICAL_RECURRENCE_ARRAY_MAX;k++){
		    
		    enum icalrecurrencetype_weekday dow =
			icalrecurrencetype_day_day_of_week(impl->by_ptrs[BY_DAY][k]);
		    
		    if(current_dow == dow){
			short doy = icaltime_day_of_year(t);
			/* HACK, incomplete Nth day of week handling */
			impl->days[days_index++] = doy;
			
		    }
		}
            }
        }
    } else if (has_by_data(impl,BY_MONTH) && has_by_data(impl,BY_MONTH_DAY)){

        for(j=0;impl->by_ptrs[BY_MONTH][j]!=ICAL_RECURRENCE_ARRAY_MAX;j++){
            for(k=0;impl->by_ptrs[BY_MONTH_DAY][k]!=ICAL_RECURRENCE_ARRAY_MAX;k++)
           {
                short month = impl->by_ptrs[BY_MONTH][j];
                short month_day = impl->by_ptrs[BY_MONTH_DAY][k];
                short doy;

		t.day = month_day;
		t.month = month;
		t.year = year;

		doy = icaltime_day_of_year(t);

		impl->days[days_index++] = doy;

            }
        }
    } else if (has_by_data(impl,BY_WEEK_NO) && !has_by_data(impl,BY_DAY)){

	struct icaltimetype t;
	short dow;

	t.day = impl->dtstart.day;
	t.month = impl->dtstart.month;
	t.year = year;

        dow = icaltime_day_of_week(t);

    } else if (has_by_data(impl,BY_WEEK_NO) && has_by_data(impl,BY_DAY)){

    } else if (has_by_data(impl,BY_YEAR_DAY)){

    } else if (has_by_data(impl,BY_MONTH_DAY) ){

    } else if (has_by_data(impl,BY_DAY)){

    } else {

    }

    return 0;
}                                  


int next_year(struct icalrecur_iterator_impl* impl)
{
    struct icaltimetype next;
    short end_of_data=0;

    if (next_hour(impl) == 0){
	return 0;
    }

    impl->days_index++;

    if (impl->days[impl->days_index] == ICAL_RECURRENCE_ARRAY_MAX){
	impl->days_index = 0;
	end_of_data = 1;
    }

    next = icaltime_from_day_of_year(impl->days[impl->days_index],impl->last.year);
    
    impl->last.day =  next.day;
    impl->last.month =  next.month;
  

    if(end_of_data){
	increment_year(impl,impl->rule.interval);
	expand_year_days(impl,impl->last.year);
    }
    
    return 1;
}

int check_restriction(struct icalrecur_iterator_impl* impl,
		      enum byrule byrule, short v)
{
    int pass = 0;
    int itr;
    icalrecurrencetype_frequency freq = impl->rule.freq;

    if(impl->by_ptrs[byrule][0]!=ICAL_RECURRENCE_ARRAY_MAX &&
	expand_map[freq].map[byrule] == CONTRACT){
	for(itr=0; impl->by_ptrs[byrule][itr]!=ICAL_RECURRENCE_ARRAY_MAX;itr++){
	    if(impl->by_ptrs[byrule][itr] == v){
		pass=1;
		break;
	    }
	}

	return pass;
    } else {
	/* This is not a contracting byrule, or it has no data, so the
           test passes*/
	return 1;
    }
}

int check_contracting_rules(struct icalrecur_iterator_impl* impl)
{
    enum byrule;

    int day_of_week=0;
    int week_no=0;
    int year_day=0;

    if (
	check_restriction(impl,BY_SECOND,impl->last.second) &&
	check_restriction(impl,BY_MINUTE,impl->last.minute) &&
	check_restriction(impl,BY_HOUR,impl->last.hour) &&
	check_restriction(impl,BY_DAY,day_of_week) &&
	check_restriction(impl,BY_WEEK_NO,week_no) &&
	check_restriction(impl,BY_MONTH_DAY,impl->last.day) &&
	check_restriction(impl,BY_MONTH,impl->last.month) &&
	check_restriction(impl,BY_YEAR_DAY,year_day) )
    {

	return 1;
    } else {
	return 0;
    }
}

struct icaltimetype icalrecur_iterator_next(icalrecur_iterator *itr)
{
    struct icalrecur_iterator_impl* impl = 
	(struct icalrecur_iterator_impl*)itr;
    
    if( (impl->rule.count!=0 &&impl->occurrence_no >= impl->rule.count) ||
       (!icaltime_is_null_time(impl->rule.until) && 
	icaltime_compare(impl->last,impl->rule.until) > 0)) {
	return icaltime_null_time();
    }

    if(impl->occurrence_no == 0){
	impl->occurrence_no++;
	return impl->last;
    }


    do {
    	switch(impl->rule.freq){
	    
	    case ICAL_SECONDLY_RECURRENCE: {
		next_second(impl);
		break;
	    }
	    case ICAL_MINUTELY_RECURRENCE: {
		next_minute(impl);
		break;
	    }
	    case ICAL_HOURLY_RECURRENCE: {
		next_hour(impl);
		break;
	    }
	    case ICAL_DAILY_RECURRENCE: {
		next_day(impl);
		break;
	    }
	    case ICAL_WEEKLY_RECURRENCE: {
		next_week(impl);
		break;
	    }
	    case ICAL_MONTHLY_RECURRENCE: {
		next_month(impl);
		break;
	    }
	    case ICAL_YEARLY_RECURRENCE:{
		next_year(impl);
		break;
	    }
	    default:{
		assert(0); /* HACK, need a better error */
	    }
	}    

	if(impl->last.year >= 2038){
	    /* HACK */
	    return icaltime_null_time();
	}

	
    } while(!check_contracting_rules(impl) 
	    || icaltime_compare(impl->last,impl->dtstart) < 0);


    if( !icaltime_is_null_time(impl->rule.until) && 
	icaltime_compare(impl->last,impl->rule.until) > 0) {
	return icaltime_null_time();
    }

    impl->occurrence_no++;

    return impl->last;
}

#include "ical.h"
void icalrecurrencetype_test()
{
    icalvalue *v = icalvalue_new_from_string(
	ICAL_RECUR_VALUE,
	"FREQ=YEARLY;UNTIL=20060101T000000;INTERVAL=2;BYDAY=SU,WE;BYSECOND=15,30; BYMONTH=1,6,11");

    struct icalrecurrencetype r = icalvalue_get_recur(v);
    struct icaltimetype t = icaltime_from_timet( time(0), 0, 0);
    struct icaltimetype next;
    time_t tt;

    struct icalrecur_iterator_impl* itr 
	= (struct icalrecur_iterator_impl*) icalrecur_iterator_new(r,t);

    do {

	next = icalrecur_iterator_next(itr);
	tt = icaltime_as_timet(next);

	printf("%s",ctime(&tt ));		

    } while( ! icaltime_is_null_time(next));
 
}

