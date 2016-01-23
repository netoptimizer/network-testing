#!/usr/bin/perl -w

=head1 NAME

 ethtool_stats.pl - Sample changing adapter statistics from ethtool -S

=head1 SYNOPSIS

 ethtool_stats.pl --dev DEVICE [options]

 options:
    --dev	Ethernet adapter/device to get stats from.
    --count	How many seconds sampling will run (default: infinite)
    --sec	Sets sample interval in seconds (default: 1.0 sec)
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
use Time::HiRes;

my @DEV    = ();
my $debug  = 0;
my $dumper = 0;
my $help   = 0;
my $man    = 0;
my $all    = 0;
my $count  = 0;
my $delay  = 1;

GetOptions (
    'dev=s'    => \@DEV,
    'count=s'  => \$count,
    'sec=s'    => \$delay,
    'all!'     => \$all,
    'debug=s'  => \$debug,
    'dumper!'  => \$dumper,
    'help|?'   => sub { Getopt::Long::HelpMessage(-verbose => 1) },
    'man'      => \$man,
    ) or pod2usage(2);
pod2usage(-exitstatus => 0, -verbose => 2) if $man;
pod2usage(-exitstatus => 1, -verbose => 1) unless scalar @DEV;

my %STATS;

sub collect_stats($) {
    # Parse ethtool stats and return key=value hash
    my $device = shift;
    my %hash;
    open(ETHTOOL, "sudo /usr/sbin/ethtool -S $device |");
    $hash{timestamp} = Time::HiRes::time();
    while (defined(my $line = <ETHTOOL>)) {
	chomp($line);
	if ($line =~ m/\s*(.+):\s?(\d+)/) {
	    my $key   = $1;
	    my $value = $2;
	    $hash{$key} = $value;
	    print "PARSED: $line -- key:$key val:$value\n" if ($debug > 2);
	} else {
	    print "WARN: could not parse line:\"$line\"\n" if ($debug > 1);
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

sub difference($$$) {
    my ($device, $stat, $prev)= @_;
    my $something_changed = 0;
    if (!defined($prev)) {
	return 0;
    }
    # The sleep function might not be accurate enough, and this
    # program also add some delay, thus calculate sampling period by
    # highres timestamps
    my $period = $stat->{timestamp} - $prev->{timestamp};
    print "timestamp $stat->{timestamp} - $prev->{timestamp} = $period\n"
	if $debug;
    if (($period > $delay * 2) || ($period < ($delay / 2))) {
	print " ***WARN***: Sample period ($delay) not accurate ($period)\n";
    }
    delete $prev->{timestamp};

    my @keys = (sort keys %$prev);
    foreach my $key (@keys) {
	my $value_now  = $stat->{$key};
	my $value_prev = $prev->{$key};
	my $diff = ($value_now - $value_prev) / $period;
	next if (($diff == 0) && !$all);
	# Round off number
	$diff = sprintf("%.0f", $diff);
	my $pretty = $diff;
	# Add thousands comma separators (use Number::Format instead?)
	$pretty =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1,/g;
	# Right-justify via printf
	printf("Ethtool(%-8s) stat: %12d (%15s) <= %s /sec\n",
	       $device, $diff, $pretty, $key);
	$something_changed++;
    }
    return $something_changed;
}

sub stats_loop() {
    my $collect = $count + 1; # First round was empty (+1)
    my %prev = ();
    my %stats = ();

    # count == 0 is infinite
    while ( ($count == 0) ? 1 : $collect-- ) {
	print "\nShow adapter " . join(' ', @DEV) . " statistics (ONLY that changed!)\n";
	my $changes = 0;
	if (!scalar keys %prev) {
	    print " ***NOTE***: Collecting stats for next round ($delay sec)\n";
	}
	foreach my $device (@DEV){
		$stats{$device} = collect_stats($device);
		$changes += difference($device, $stats{$device}, $prev{$device});
	}
	if (!$changes) {
	    print " ***WARN***: No counters changed\n" ;
	}
	%prev = %stats;
	Time::HiRes::sleep($delay);
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
