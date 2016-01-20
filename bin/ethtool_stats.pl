#!/usr/bin/perl -w

=head1 NAME

 ethtool_stats.pl - Sample changing adapter statistics from ethtool -S

=head1 SYNOPSIS

 ethtool_stats.pl --dev DEVICE [options]

 options:
    --dev	Ethernet adapter/device to get stats from.
    --count	How many seconds sampling will run (default: infinite)
    --all	List all zero stats
    --help	Brief usage/help message.
    --man	Full documentation.


=head1 DESCRIPTION

 This script shows ethtool (-S|--statistics) stats, but only stats
 that changes.  And then reports stats per sec.

 Created this script because some driver, e.g. mlx5, report false
 stats via ifconfig.

=cut

use strict;
use warnings FATAL => 'all';
use Data::Dumper;
use Pod::Usage;
use Getopt::Long;

my $DEV    = undef;
my $debug  = 0;
my $dumper = 0;
my $help   = 0;
my $man    = 0;
my $all    = 0;
my $count  = 0;

GetOptions (
    'dev=s'    => \$DEV,
    'count=s'  => \$count,
    'all!'     => \$all,
    'debug!'   => \$debug,
    'dumper!'  => \$dumper,
    'help|?'   => sub { Getopt::Long::HelpMessage(-verbose => 1) },
    'man'      => \$man,
    ) or pod2usage(2);
pod2usage(-exitstatus => 0, -verbose => 2) if $man;
pod2usage(-exitstatus => 1, -verbose => 1) unless defined $DEV;

my %STATS;

sub collect_stats($) {
    # Parse ethtool stats and return key=value hash
    my $device = shift;
    my %hash;
    open(ETHTOOL, "sudo /usr/sbin/ethtool -S $device |");
    while (defined(my $line = <ETHTOOL>)) {
	chomp($line);
	if ($line =~ m/\s*(.+):\s?(\d+)/) {
	    my $key   = $1;
	    my $value = $2;
	    $hash{$key} = $value;
	    print "PARSED: $line -- key:$key val:$value\n" if $debug;
	} else {
	    print "WARN: could not parse line:\"$line\"\n" if $debug;
	}
    }
    close(ETHTOOL);
    return \%hash;
}

# Example
sub traverse_hash(%) {
    my $hash = shift;
    while(my ($key, $value) = each %$hash) {
	print "key:$key val:$value\n";
    }
}
# Example sort
sub traverse_hash_sorted(%) {
    my $hash = shift;
    my @keys = (sort keys %$hash);
    foreach my $key (@keys) {
	print "key:$key val:$hash->{$key}\n";
    }
}

sub difference($$) {
    my ($stat, $prev)= @_;
    my $something_changed = 0;
    my @keys = (sort keys %$prev);
    foreach my $key (@keys) {
	my $value_now  = $stat->{$key};
	my $value_prev = $prev->{$key};
	my $diff = $value_now - $value_prev;
	next if (($diff == 0) && !$all);
	my $pretty = $diff; # sprintf("%15d", $diff);
	# Add thousands comma separators (use Number::Format instead?)
	$pretty =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1,/g;
	# Right-justify via printf
	printf("Ethtool($DEV) stat: %12d (%15s) <= %s /sec\n",
	       $diff, $pretty, $key);
	$something_changed++;
    }
    return $something_changed;
}

sub stats_loop() {
    my $collect = $count + 1; # First round was empty (+1)
    my $prev = undef;
    my $stats = {};

    # count == 0 is infinite
    while ( ($count == 0) ? 1 : $collect-- ) {
	print "\nShow adapter $DEV statistics (ONLY that changed!)\n";
	$stats = collect_stats($DEV);
	my $changes = difference($stats, $prev);
	if (!(defined $prev)) {
	    print " ***NOTE***: Collecting stats for next round\n";
	} elsif (!$changes) {
	    print " ***WARN***: No counters changed\n" ;
	}
	$prev = $stats;
	sleep 1;
    }
}
stats_loop();

#my $HASH = collect_stats($DEV);
#traverse_hash_sorted($HASH);

#print Dumper($DEV) if $dumper;
#print Dumper(\%STATS) if $dumper;
#print Dumper($HASH) if $dumper;

__END__

=head1 AUTHOR

 Jesper Dangaard Brouer <netoptimizer@brouer.com>

=cut
