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

# %events = ( 'em-events.xml' =>
# 	    { 'files' => [ 'em-folder-view.c', 'em-composer-utils.c', 'mail-folder-cache.c' ],
# 	      'module' => 'mail' },
# 	    );

# foreach $out (keys %events) {
#     print "generating events doc $out\n";
#     %data = %{$events{$out}};
#     @files = @{$data{'files'}};
#     $module = $data{'module'};
#     open OUT,">$out";
#     foreach $file (@files) {
# 	open IN,"<../../$module/$file";
# 	while (<IN>) {
# 	    if (m/\@Event: (.*)/) {
# 		$title = $1;
# 		$name = $1;
# 		$target = "";
# 		while (<IN>) {
# 		    if (m/\@Title: (.*)/) {
# 			$title = $1;
# 		    } elsif (m/\@Target: (.*)/) {
# 			$target = $1;
# 		    } elsif (m/\* (.*)/) {
# 			$desc.= $1."\n";
# 		    }
# 		    last if (m/\*\//);
# 		}
# 		if ($target eq "") {
# 		    print "Warning: No target defined for event $name ($title)\n";
# 		}
# 		print OUT <<END;
# 	<sect2>
# 	  <title>$title</title>
# 	  <informaltable>
# 	    <tgroup cols="2">
# 	      <colspec colnum="1" colname="field" colwidth="1*"/>
# 	      <colspec colnum="2" colname="value" colwidth="4*"/>
# 	      <tbody valign="top">
# 		<row>
# 		  <entry>Name</entry>
# 		  <entry><constant>$name</constant></entry>
# 		</row>
# 		<row>
# 		  <entry>Target</entry>
# 		  <entry>
# 		    <link
# 		      linkend="$module-hooks-event-$target">$target</link>
# 		  </entry>
# 		</row>
# 		<row>
# 		  <entry>Description</entry>
# 		  <entry>
# 		  <simpara>
# 		  $desc
# 		  </simpara>
# 		  </entry>
# 		</row>
# 	      </tbody>
# 	    </tgroup>
# 	  </informaltable>
# 	</sect2>
# END
# 	    }
# 	}
# 	close IN;
#     }
#     close OUT;
# }

#
# Generic table builder, still experimental.
#

sub buildxml {
    my $type = $_[0];
    my $out = $_[1];
    my %data = %{$_[2]};
    my @files, $module;
    my $line;

    print "generating doc $out for $type\n";
    @files = @{$data{'files'}};
    $module = $data{'module'};
    open OUT,">$out";
    foreach $file (@files) {
	my $line = 0;

	open IN,"<../../$module/$file" || die ("Cannot open \"$module/$file\"");
	while (<IN>) {
	    if (m/\/\*\* \@$type: (.*)/) {
		my $key = "";
		my $val = "";
		my $desc = 0;
		my $title = $1;
		my %blob = { };
		my @blobs = ();

		while (<IN>) {
		    $line++;
		    if (m/\@(.*): (.*)/) {
			if ($val ne "") {
			    $blob{$key} = $val;
			}
			$key = $1;
			$val = $2;
			push @blobs, $key;
		    } elsif (m/\* (.+)/) {
			$val .= $1."\n";
		    } else {
			if ($desc == 0) {
			    if ($val ne "") {
				$blob{$key} = $val;
			    }
			    $val = "";
			    $key = "";
			} else {
			    $val .= "\n";
			}
			if (m/\*\s*$/) {
			    $desc = 1;
			}
		    }
		    last if (m/\*\//);
		}
		print OUT<<END;
	<sect2>
	  <title>$title</title>
END
		if ($val ne "") {
		    $val =~ s/[\n]+$//gos;
		    $val =~ s/\n\n/\<\/simpara\>\n\<simpara\>/g;
		    print OUT "<simpara>$val</simpara>\n";
		}
		print OUT <<END;
	  <informaltable>
	    <tgroup cols="2">
	      <colspec colnum="1" colname="field" colwidth="1*"/>
	      <colspec colnum="2" colname="value" colwidth="4*"/>
	      <tbody valign="top">
END

		foreach $key (@blobs) {
		    print OUT <<END;
		<row>
		  <entry>$key</entry>
		  <entry>$blob{$key}</entry>
		</row>
END
}
		print OUT <<END;
		<row>
		  <entry>Defined</entry>
		  <entry>$module/$file:$line</entry>
		</row>
	      </tbody>
	    </tgroup>
	  </informaltable>
	</sect2>
END
	    }
	    $line++;
	}
	close IN;
    }
    close OUT;
}


%hooks = ( 'es-hooks.xml' =>
	    { 'type' => 'HookClass',
	      'files' => [ 'es-menu.c', 'es-event.c' ],
	      'module' => 'shell' },
	   'es-menus.xml' =>
	    { 'type' => 'HookPoint',
	      'files' => [ 'e-shell-window.c' ],
	      'module' => 'shell' },
	   'es-events.xml' =>
	    { 'type' => 'Event',
	      'files' => [ 'e-shell.c' ],
	      'module' => 'shell' },
	   'em-events.xml' =>
	    { 'type' => 'Event',
	      'files' => [ 'em-folder-view.c', 'em-composer-utils.c', 'mail-folder-cache.c' ],
	      'module' => 'mail' },
	   'em-popups.xml' =>
	    { 'type' => 'HookPoint-EMPopup',
	      'files' => [ 'em-folder-tree.c', 'em-folder-view.c', 'em-format-html-display.c', '../composer/e-msg-composer-attachment-bar.c' ],
	      'module' => 'mail' },
	   'ecal-popups.xml' =>
	    { 'type' => 'HookPoint-ECalPopup',
	      'files' => [ 'gui/e-calendar-view.c', 'gui/calendar-component.c', 'gui/e-calendar-view.c', 'gui/tasks-component.c' ],
	      'module' => 'calendar' },
	   'em-configs.xml' =>
	    { 'type' => 'HookPoint-EMConfig',
	      'files' => [ 'em-mailer-prefs.c', 'em-account-editor.c', 'em-folder-properties.c', 'em-composer-prefs.c' ],
	      'module' => 'mail' },
	   'em-menus.xml' =>
	    { 'type' => 'HookPoint-EMMenu',
	      'files' => [ 'em-folder-browser.c', 'em-message-browser.c' ],
	      'module' => 'mail' },
	    );

foreach $out (keys %hooks) {
    %data = %{$hooks{$out}};

    &buildxml($data{'type'}, $out, \%data);
}


