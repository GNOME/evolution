#!/usr/bin/perl

# get addresses out of messages

if ($#ARGV < 0) {
    print "Usage: $0 message(s) mbox(es)\n";
    exit 1;
}

foreach $name (@ARGV) {
    open IN,"<$name";
    while (<IN>) {
	if (/^From: (.*)/i
	    || /^To: (.*)/i
	    || /^Cc: (.*)/i) {
	    $base = $1;
	    while (<IN>) {
		if (/^\s+(.*)/) {
		    $base .= " ".$1;
		} else {
		    last;
		}
	    }
	    $uniq{$base} = 1;
	}
    }
    close IN;
}

foreach $key (sort keys %uniq) {
    print $key."\n";
}
