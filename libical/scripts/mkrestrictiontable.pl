#!/usr/bin/perl

use Getopt::Std;
getopts('i:');

# the argument should be the path to the restriction datafile, usually
# design-data/restrictions.csv        
open(F,"$ARGV[0]") || die "Can't open restriction data file $ARGV[0]:$!";

# Write the file inline by copying everything before a demarcation
# line, and putting the generated data after the demarcation

if ($opt_i) {

  open(IN,$opt_i) || die "Can't open input file $opt_i";

  while(<IN>){

    if (/Do not edit/){
      last;
    }

    print;

  }    

  print "/* Everything below this line is machine generated. Do not edit. */\n";


  close IN;
}

print "icalrestriction_property_record icalrestriction_property_records[] = {\n";

while(<F>)
{

  chop;

  s/\#.*$//;

  my($method,$targetcomp,$prop,$subcomp,$restr,$sub) = split(/,/,$_);

  next if !$method;
  
  if(!$sub) {
    $sub = "0";
  }

  if($prop ne "NONE"){
    print("    \{ICAL_METHOD_${method},ICAL_${targetcomp}_COMPONENT,ICAL_${prop}_PROPERTY,ICAL_RESTRICTION_${restr},$sub\},\n");
  }

}


print "    {ICAL_METHOD_NONE,ICAL_NO_COMPONENT,ICAL_NO_PROPERTY,ICAL_RESTRICTION_NONE}\n";

print "};\n";

print "icalrestriction_component_record icalrestriction_component_records[] = {\n";


close(F);  
open(F,"$ARGV[0]") || die "Can't open restriction data file $ARGV[0]:$!";

while(<F>)
{

  chop;

  s/\#.*$//;

  my($method,$targetcomp,$prop,$subcomp,$restr) = split(/,/,$_);

  next if !$method;
  

    if($subcomp ne "NONE"){
      print("    \{ICAL_METHOD_${method},ICAL_${targetcomp}_COMPONENT,ICAL_${subcomp}_COMPONENT,ICAL_RESTRICTION_${restr}\},\n");
    }

}

print "    {ICAL_METHOD_NONE,ICAL_NO_COMPONENT,ICAL_NO_COMPONENT,ICAL_RESTRICTION_NONE}\n";
print "};\n";
