#!/usr/bin/perl -w 
#
#  Script for translators that extract .sgml files into .sgml.po ones
#
#  Copyright (C) 2001 Héctor García Álvarez.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This script is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this library; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#
#  Authors: Héctor García Álvarez <hector@scouts-es.org>

## Loaded modules
use strict; 
use File::Basename;
use Getopt::Long;

my $LANG = $ARGV[0];
#my $OUTFILE = "./tmp/$FILE.h";

my %string = ();
my @elements;
my @items;
my $n=0;

if (! $LANG){
        print "Usage: update_po.pl LANGCODE\n";
	exit;
} else { $LANG .=".po"; }

chdir ("./C");
if ( !(-d "./tmp") ) { mkdir ("./tmp", 0755) ; }
my $comand="";

open FILES, "<POTFILES.in" ;
#open POTFILE, ">POTFILES.in.h"; 
while (<FILES>) {
	undef (%string);
        s/\n//g;
	my $Original_file = $_ ;
	s/.\///g;
	my $Converted_file = "./tmp/".$_.".h";
	#       print $Original_file."\n";
	#       print $Translated_file."\n";
	print "Converting ".$Original_file."\n" ;
	system "rm -f $Converted_file";
	&Convert ($Original_file);
	open OUT, ">>$Converted_file";
	&addMessages;
	close OUT;
	$comand = "xgettext --default-domain=$Original_file ";
	$comand .="--directory=. --add-comments --keyword=_ --keyword=N_ ";
	$comand .="$Converted_file ";
	system ( $comand );
	print ("Updating $Original_file.po\n");
	system ("mv $Original_file.po ../$LANG/$Original_file.pot");
	system ("cp ../$LANG/$Original_file.po ../$LANG/$Original_file.po.old");
	system ("msgmerge ../$LANG/$Original_file.po.old ../$LANG/$Original_file.pot -o ../$LANG/$Original_file.po");
	system ("msgfmt --statistics ../$LANG/$Original_file.po");
	system ("rm -f ../$LANG/$Original_file.pot");
#	print POTFILE $Converted_file."\n";
	print ".\n";
}
close FILES;

system ("rm -rf ./tmp ");
exit 0;


sub Convert() {

    ## Reading the file
  open (IN, "<$_[0]") || die "can't open $_[0]: $!";
 
        ### For translatable Sgml files ###
  while (<IN>) {
  	if ( /<!--/ ) {
		my $Salida =  $_ ;
		if ( (/-->/) ) {
			$string{$Salida} = [];
		} else {
			while (<IN>) {
				$Salida .=  $_ ;
				if ( /-->/ ) { last ; }
			}
			$string{$Salida} = [];
		}
	}
	elsif ( /<para>/ ) {
		my $number_of_para = 1;
		my $Salida =  $_ ;
		if ( /<\/para>/ ) {
			$string{$Salida} = [];
		} else {
			while (<IN>) {
				if ( /<para>/ ) { $number_of_para++; }
				$Salida .=  $_ ;
				if ( /<\/para>/ ) { 
					$number_of_para--;
					if ( $number_of_para==0) {last ; }
				}
			}
			$string{$Salida} = [];
		}
	}
        elsif ( /<title>/ ) {
		my $Salida =  $_ ;
		if ( /<\/title>/ ) {
			$string{$Salida} = [];
		} else {
			while (<IN>) {
				$Salida .=  $_ ;
				if ( /<\/title>/ ) { last ; }
			}
			$string{$Salida} = [];
		}
	}
	elsif ( /<glossterm>/ ) {
		my $Salida =  $_ ;
		if ( /<\/glossterm>/ ) {
			$string{$Salida} = [];
		} else {
			while (<IN>) {
				$Salida .=  $_ ;
				if ( /<\/glossterm>/ ) { last ; }
			}
			$string{$Salida} = [];
		}
	}
	elsif ( /<guilabel>/ ) {
		my $Salida =  $_ ;
		if ( /<\/guilabel>/ ) {
			$string{$Salida} = [];
		} else {
			while (<IN>) {
				$Salida .=  $_ ;
				if ( /<\/guilabel>/ ) { last ; }
			}
			$string{$Salida} = [];
		}
	}
  }
  close (IN);
}

sub addMessages{

    foreach my $theMessage (sort keys %string) { 
    
    my ($tag) = $string{$theMessage} ;

    # Replace XML codes for special chars to
    # geniune gettext syntax
    #---------------------------------------
    $theMessage =~ s/"/\\"/mg;
    $theMessage =~ s/\n/\\n\n/mg;
    
#    $theMessage =~ s/&lt;/</mg;
#    $theMessage =~ s/&gt;/>/mg;

    if ($theMessage =~ /\n/) {
        #if ($tag) { print OUT "/* $tag */\n"; } 
        @elements =  split (/\n/, $theMessage);
        for ($n = 0; $n < @elements; $n++) {
           
           if ($n == 0) {
	       print OUT "gchar *s = N_"; 
               print OUT "(\"$elements[$n]\"";
	       if ($n == @elements - 1) { print OUT ");\n"; }
	       print OUT "\n"; 
           }

           elsif ($n == @elements - 1) { 
	       print OUT "             ";
               print OUT "\"$elements[$n]\");\n\n";
           }

           elsif ($n > 0)  { 	
	       print OUT "             ";
               print OUT "\"$elements[$n]\"\n";
           }
        }

    } else {
    	
#	if ($tag) { print OUT "/* $tag */\n"; }		
	print OUT "gchar *s = N_(\"$theMessage\");\n";
    }
	    
    }
}

