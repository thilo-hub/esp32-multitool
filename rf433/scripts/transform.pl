#!/usr/local/bin/perl

# Script to convert from other format (old) to more dense
# Essentially just the binary struct pattern_t
#
# Usage:  {$0} file

# Where file is something like:
# pattern -s {lookup}[8] base64-"4-bit:4-bit" encoded


use MIME::Base64;
@v = split( /\s+/, <> );
shift @v if $v[0] eq "pattern";
shift @v if $v[0] eq "-s";
$b = pop @v;
die "Wrong number of args $_" unless scalar(@v) == 8;
$b = decode_base64($b);  
$h=unpack( "H*", $b ); 
$h =~ s/f*$//;

my $len = length($h);

print STDERR ">>($len) Patterns<<\n"; # $h<<\n";

$o="";
while( $v = substr($h,0,8,"") ) {
	#print STDERR "$h\nX: $v\n"; 
	my $k= reverse  $v;
	#print STDERR "$h\nO: $k\n"; 
	#$k = oct("0$k");
#exit;




    	$k= sprintf "%06x", oct( "0$k" ); 
	$k =~ s/(..)(..)(..)/$3$2$1/;  
	$o .= $k; 
	# printf STDERR "P: $k  ";
}
my $pat = encode_base64( pack( "v8vH*", @v,$len, $o ), "" );
print "send2 $pat\n";
