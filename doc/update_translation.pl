#!/usr/bin/perl -w 
#
#  Script that translates .sgml files using the .po files generated from
#  the script update_po.pl
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
use File::Basename;
use Getopt::Long;

my $LANG = $ARGV[0];

my %string;
my $texto_original="";
my $texto_traducido="";

if (! $LANG){
    print "Usage: update_translation.pl LANGCODE\n";
    exit;
}

chdir ("./C");

## Reading the po file
#print "Loading ".$LANG.".po\n";
#&load_translated_strings ($LANG.".po");

## Checking for the lang dir
if ( !(-d "../".$LANG) ) { mkdir ("../".$LANG, 0755) ; }

open FILES, "<POTFILES.in" ;
while (<FILES>) {
	undef %string;
	s/\n//g;
	$Original_file = $_ ;
	s/.\///g;
	$Translated_file = "../".$LANG."/".$_;
#	print $Original_file."\n";
#	print $Translated_file."\n";
	&load_translated_strings ("../".$LANG.".po/".$_.".po");
	print "Translating ".$Original_file ;
	system "rm -f $Translated_file";
	&translate_file ($Translated_file , $Original_file);
	print ".\n";
}
close FILES;							    

exit 0;




sub load_translated_strings () 
{
	my $FILE=$_[0];
	open (IN, "<$FILE") || die "I can't find $FILE";

	while (<IN>) {
	        if ( /#: /) {
			&original;
			&traduccion;
#               print "Original \n##".$texto_original."##\n";
#               print "Traducción \n##".$texto_traducido."##\n";
	                $string{$texto_original} = $texto_traducido;
		}
	}
	close (IN);
}

sub translate_file ()
{
	my $OUTFILE=$_[0];
	my $INFILE=$_[1];
	
	open OUT, ">>$OUTFILE";
	open (IN, "<$INFILE") || die "can't open $INFILE: $!";

	while (<IN>) {
		my $imprimir = 0;
		if ( /<!--/ ) {
			$Salida =  $_ ;
			if ( !(/-->/) ) {
				while (<IN>) {
					$Salida .=  $_ ;
					if ( /-->/ ) { last ; }
				}
			}
			$imprimir = 1;
		}
		elsif ( /<para>/ ) {
			my $number_of_para = 1;
			$Salida = $_ ;
			if ( !(/<\/para>/) ) {
				while (<IN>) {
					if ( /<para>/ ) { $number_of_para++; }
					$Salida .=  $_ ;
					if ( /<\/para>/ ) {
						$number_of_para--;
						if ( $number_of_para==0) {last ; }
					}
				}
			}
			$imprimir = 1;
		}
		elsif ( /<title>/ ) {
			$Salida = $_ ;
			if ( !(/<\/title>/) ) {
				while (<IN>) {
					$Salida .=  $_ ;
					if ( /<\/title>/ ) { last ; }
				}
			}
			$imprimir = 1;
		}
		elsif ( /<glossterm>/ ) {
			$Salida = $_ ;
			if ( !(/<\/glossterm>/) ) {
				while (<IN>) {
					$Salida .=  $_ ;
					if ( /<\/glossterm>/ ) { last ; }
				}
			}
			$imprimir = 1;
		}
		elsif ( /<guilabel>/ ) {
			$Salida = $_ ;
			if ( !(/<\/guilabel>/) ) {
				while (<IN>) {
					$Salida .=  $_ ;
					if ( /<\/guilabel>/ ) { last ; }
				}
			}
			$imprimir = 1;
		}
	        if ( $imprimir == 0 ) {  print (OUT $_); }
		else { 
			my $impreso=0;
			foreach my $theMessage (sort keys %string) {
				if (!($theMessage cmp $Salida)) {
					my $tag = $string{$Salida} ;
					
					if ( $tag cmp "") {
						$tag =~ s/\\"/"/mg ;
						print (OUT $tag);
					}
					else {
						print (OUT $Salida);
					}
					$impreso=1;
				}
			}
			if ( $impreso == 0) {
				print "No lo encuentro\n##".$Salida."##\n";
				$impreso=0;
			} 
			$imprimir = 0;
		}
	}
	close IN;
	close OUT;
}
#exit 0;

sub original () 
{
	my $tmp = "";
	while (<IN>) {
		if ( !(/^#: /) ) {
			if ( /msgid ""/) { s/msgid ""\n//; }
			if ( /msgstr/) {
				$tmp =~ s/\\n/\n/sg ;
				$tmp =~ s/\\t/\t/sg ;
				$tmp =~ s/\\"/"/sg ;
				$texto_original = $tmp;
				last ;
			}
			s/msgid "//;
			s/\s*"// ;
			s/"\n// ;
			s/\n// ;
			$tmp .=  $_;
		}
	}
}
		
sub traduccion ()
{
        my $tmp = "";
	my $first = 0;
	if ( /msgstr "/) {
		if ( /msgstr ""/) {
			$tmp = "";
			$first = 1;
		} else {
			$tmp = $_;
			$tmp =~ s/msgstr "//;
			$tmp =~ s/"\n// ;
		}
	}
	while (<IN>) {
	
		if ( !($_ cmp "\n") )  {
			$tmp =~ s/\\n/\n/sg ;
			$tmp =~ s/\\t/\t/sg ;
			$tmp =~ s/\"/"/sg ;
			if ( $first == 1 ) { $texto_traducido = "" ; }
			else { $texto_traducido = $tmp; }
			last ;
		}
		$first = 0;
		s/msgstr "//;
		s/"\n// ;
		s/\s*"// ;
		$tmp .=  $_;	
	}
	if ( eof IN ) {
		$tmp =~ s/\\n/\n/sg ;
		$tmp =~ s/\\t/\t/sg ;
		$tmp =~ s/\"/"/sg ;
		$texto_traducido = $tmp; 
	}
}
