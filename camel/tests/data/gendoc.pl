#!/usr/bin/perl

# Generate 'documents' in different encodings, from po files

if ($#ARGV < 0) {
    print "Usage: gendoc.pl pofile pofile ...\n";
    exit 1;
}

$fmt = "| fmt -u ";

sub read_msgstr()
{
    my $str = "";
    while (<IN>) {
	if (m/^msgstr \"(.*)\"/) {
	    $str = $1;
	    if ($str eq "") {
		while (<IN>) {
		    if (m/\"(.*)\"/) {
			$str .= $1;
		    } else {
			last;
		    }
		}
	    }
	    return $str;
	}
    }
    return "";
}

$unknown = "x-unknown-1";

foreach $name (@ARGV) {
    if ($name =~ m@([^/]*).po$@) {
	$poname = $1;
	    
	open IN,"<$name";

	$header = read_msgstr;
	if ($header =~ /Content-Type:.*charset=([-a-zA-Z0-9]*)/i) {
	    $charset = $1;
	} else {
	    $charset = $unknown++;
	}

	print "Building $poname.$charset.txt from $name\n";

	open OUT,"$fmt > $poname.$charset.txt";
	while (!eof(IN)) {
	    $msg = read_msgstr;
	    # de-escape
	    $msg =~ s/\\n/\n/gso;
	    $msg =~ s/\\t/\t/gso;
	    $msg =~ s/\\(.)/$1/gso;
	    print OUT $msg." ";
	}
	close OUT;
	close IN;
    } else {
	printf("ignoring $name, probably not intended\n");
    }
}

