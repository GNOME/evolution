#!/usr/bin/perl

#
# API reference
#

%byref = ( 'e-popup.xml' =>
	   { 'files' => [ 'e-popup.h', 'e-popup.c' ],
	     'module' => 'e-util' },

	   'e-menu.xml' =>
	   { 'files' => [ 'e-menu.h', 'e-menu.c' ],
	     'module' => 'e-util' },

	   'e-event.xml' =>
	   { 'files' => [ 'e-event.h', 'e-event.c' ],
	     'module' => 'e-util' },

	   'e-config.xml' =>
	   { 'files' => [ 'e-config.h', 'e-config.c' ],
	     'module' => 'e-util' },

	   'e-plugin.xml' =>
	   { 'files' => [ 'e-plugin.h', 'e-plugin.c' ],
	     'module' => 'e-util' },

	   'em-popup.xml' =>
	   { 'files' => [ 'em-popup.h', 'em-popup.c' ],
	     'module' => 'mail' },

	   'em-format.xml' =>
	   { 'files' => [ 'em-format-hook.h' , 'em-format-hook.c',
			  'em-format.h', 'em-format.c',
			  'em-format-html.h', 'em-format-html.c' ],
	     'module' => 'mail' },
	   );

foreach $out (keys %byref) {
    print "file $out\n";
    %data = %{$byref{$out}};
    @files = @{$data{'files'}};
    $module = $data{'module'};
    $files = "";
    foreach $file (@files) {
	$files .= " ../../".$module."/".$file;
    }
    system("kernel-doc -docbook $files > $out");
}

#
# Event reference
#

%events = ( 'em-events.xml' =>
	    { 'files' => [ 'em-folder-view.c', 'em-composer-utils.c', 'mail-folder-cache.c' ],
	      'module' => 'mail' },
	    );

foreach $out (keys %events) {
    print "generating events doc $out\n";
    %data = %{$events{$out}};
    @files = @{$data{'files'}};
    $module = $data{'module'};
    open OUT,">$out";
    foreach $file (@files) {
	open IN,"<../../$module/$file";
	while (<IN>) {
	    if (m/\@Event: (.*)/) {
		$title = $1;
		$name = $1;
		$target = "";
		while (<IN>) {
		    if (m/\@Title: (.*)/) {
			$title = $1;
		    } elsif (m/\@Target: (.*)/) {
			$target = $1;
		    } elsif (m/\* (.*)/) {
			$desc.= $1."\n";
		    }
		    last if (m/\*\//);
		}
		if ($target eq "") {
		    print "Warning: No target defined for event $name ($title)\n";
		}
		print OUT <<END;
	<sect2>
	  <title>$title</title>
	  <informaltable>
	    <tgroup cols="2">
	      <colspec colnum="1" colname="field" colwidth="1*"/>
	      <colspec colnum="2" colname="value" colwidth="4*"/>
	      <tbody valign="top">
		<row>
		  <entry>Name</entry>
		  <entry><constant>$name</constant></entry>
		</row>
		<row>
		  <entry>Target</entry>
		  <entry>
		    <link
		      linkend="$module-hooks-event-$target">$target</link>
		  </entry>
		</row>
		<row>
		  <entry>Description</entry>
		  <entry>
		  <simpara>
		  $desc
		  </simpara>
		  </entry>
		</row>
	      </tbody>
	    </tgroup>
	  </informaltable>
	</sect2>
END
	    }
	}
	close IN;
    }
    close OUT;

}

