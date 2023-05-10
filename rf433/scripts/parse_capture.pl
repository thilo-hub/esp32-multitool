#!/usr/local/bin/perl
#
# Parse capture output from ESP
#
# Essentially one long line with timing infos
#
# Tries to join beloning groups of impulses 
# based on the value of "$jit"
#
# If if cannot fit the points into 8 different groups,
# it croaks and creates an error - log file
# It might be needed to adjust the jitter.
# If it works an "old" format pattern file is generated
# This needs to be converted to a new format...


use Data::Dumper;
use MIME::Base64;
my @pattern;
$jit = 100;
my $eop = 10003;
# Read input
$Data::Dumper::Sortkeys = 1;

my @pt = grep { s/Length: \d+ Data:\s*(\+\d+\s+)?// } <>;


my $fcnt="001";
my $idx="001";
foreach(@pt) {
	my @vl=split(/\s+/);
	my @cl=sort { abs($b) <=> abs($a) } @vl;
        my $dt=0;
	my $v={};
	my $res={};
	my $lkup={};
	my $lcnt=0;
	my $jit_msg="";
	foreach(@cl) {
		$prev = $curr;
		$curr = abs($_);
		$curr = $eop if $curr > $eop;
		$dt = abs($curr - $prev );
		if ( abs($curr - $prev ) > $jit  ) {
			$jit_msg .="DT: $dt > $jit ($curr - $prev)\n";
			if ( $v->{n} > 0 ) {
				my $avg = sprintf("%d",abs($v->{sum}/$v->{n}));
				$v->{avg} = $avg;
				$v->{idx} = $lcnt++;
				$res->{$avg} = $v;
			}
			$v = {};
		}
		$lkup->{$_} = $v;
		$v->{sum} += $curr;
		$v->{n}++;
		$v->{max}=$curr;
		$v->{min}=$curr unless defined ($v->{min});
        }
	my $avg = sprintf("%d",$v->{sum}/$v->{n});
	$v->{idx} = $lcnt++;
	$v->{avg} = $avg;
	$res->{$avg} = $v;

	
        my @lkup=();
	foreach( keys %$res ) { $lkup[$res->{$_}->{idx}] = $_; }
	my $frq = scalar(@lkup);
	push @lkup,0 while(scalar(@lkup) < 8);
	if ( scalar(@lkup) > 8 ) {
		# Maybe that helps understanding why clustering didnt work...
		my @or = sort map { 
				sprintf("%5d- %d",$res->{$_}->{min},$_),
				sprintf("%5d- %d",$res->{$_}->{max},$_)
			} keys %$res;
		open(F,">Errors.$fcnt"); 
		print F $jit_msg;
		print F Dumper(\@or,$res) ."\nJitter too big?";
		close(F);
		print STDERR "Errors: Errors.$fcnt -- ".length($in)."  -- $frq pattern\n";
	} else {
		@vl = map{ $lkup->{$_}->{idx} } @vl;
		push @vl,0 if scalar(@vl)%2;
		$in=pack("H*",join("",@vl,"FF"));
		$in=encode_base64($in,"");
		open(F,">Pattern.$fcnt"); 
		print F "pattern -s ".join(" ",@lkup)."  $in\n";
		close(F);
		print STDERR "Generated: Pattern.$fcnt -- ".length($in)."  -- $frq pattern\n";
	}
	$fcnt++;
}
        

exit(0);
