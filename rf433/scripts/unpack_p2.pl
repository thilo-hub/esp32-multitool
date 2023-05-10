use Data::Dumper;
use MIME::Base64;
my $typ = 1; # Verbose or "pure"
while(<>) {
	my @v=split(/\s+/,$_);
	my $b=decode_base64($v[1]);
	my @i=unpack("v8vH*",$b);
	my $p=$i[9];
	my $cnt = $i[8];
	print ">> $p <<\n";
	print Dumper(\@i) if $typ;
	my $k=1;
	my $c="-";
	while($cnt-- >0) {
		if ($k == 1) { 
			$k= 1<<24;
			$p0=substr($p,0,6,"");
			print "\nX:$cnt $p0\n";
			$k+=hex(substr($p0,0,2,""));
			$k+=hex(substr($p0,0,2,""))<<8;
			$k+=hex(substr($p0,0,2))<<16;
		}  
		$j= $k&7;
		$k >>=3;
		$j=$i[$j] unless $typ;
		print " $c$j";
		$c = $c eq "-" ? "+" : "-";
	}
	print "\n"
}

