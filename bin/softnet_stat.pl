#!/usr/bin/perl -w

=head1 NAME

 softnet_stat.pl - Sampling /proc/net/softnet_stat statistics

=head1 SYNOPSIS

 softnet_stat.pl [options]

 options:
    --count	How many seconds sampling will run (default: infinite)
    --sec	Sets sample interval in seconds (default: 1.0 sec)
    --pretty	Add thousands comma separators
    --help	Brief usage/help message.
    --man	Full documentation.


=head1 DESCRIPTION

 The output format in /proc/net/softnet_stat is fairly undocumented,
 plus values are printed in hex.  E.g. to decode the columns you
 need to read kernel function kernel softnet_seq_show() in
 kernel/net/core/net-procfs.c.

 To make things easier I wrote this small perl script for get
 so human readable statistics from /proc/net/softnet_stat.

=cut

use strict;
use warnings FATAL => 'all';
use Data::Dumper;
use Pod::Usage;
use Getopt::Long;
use Time::HiRes;

my $debug  = 0;
my $dumper = 0;
my $help   = 0;
my $man    = 0;
my $count  = 0;
my $delay  = 1;
my $pretty_print = 0;

GetOptions (
    'count=s'  => \$count,
    'sec=s'    => \$delay,
    'debug=s'  => \$debug,
    'dumper!'  => \$dumper,
    'pretty!'  => \$pretty_print,
    'help|?'   => sub { Getopt::Long::HelpMessage(-verbose => 1) },
    'man'      => \$man,
    ) or pod2usage(2);
pod2usage(-exitstatus => 0, -verbose => 2) if $man;

my %STATS;


sub collect_stats() {
    # Parse stats and return hash
    my %hash;
    my $cpu_iterator = 0;
    open(STAT, "/proc/net/softnet_stat");
    $hash{timestamp} = Time::HiRes::time();
    while (defined(my $line = <STAT>)) {
	chomp($line);
	# Hint: For format look at kernel func softnet_seq_show()
	# total dropped squeezed 0{5} collision received_rps flow_limit_count
	if ($line =~ m/(\w+)\s+(\w+)\s+(\w+)[\s+\w+]{5}\s+(\w+)\s+(\w+)\s+(\w+)/) {
	    my $cpu = $cpu_iterator++;
	    my $total            = hex $1;
	    my $dropped          = hex $2;
	    my $squeezed         = hex $3;
	    my $collision        = hex $4;
	    my $received_rps     = hex $5;
	    my $flow_limit_count = hex $6;

	    $hash{$cpu}{total}      = $total;
	    $hash{$cpu}{dropped}    = $dropped;
	    $hash{$cpu}{squeezed}   = $squeezed;
	    $hash{$cpu}{collision}  = $collision;
	    $hash{$cpu}{rx_rps}     = $received_rps;
	    $hash{$cpu}{flow_limit} = $flow_limit_count;

	    print "PARSED: $line -- tot:$total\n" if ($debug > 2);
	} else {
	    print "WARN: could not parse line:\"$line\"\n" if ($debug > 1);
	}
    }
    close(STAT);
    return \%hash;
}

sub print_value($$) {
    my ($value, $enable_pretty_print)= @_;

    # Round off number
    $value = sprintf("%.0f", $value);

    if ($enable_pretty_print) {
	my $pretty = $value;
	# Add thousands comma separators (use Number::Format instead?)
	$pretty =~ s/(\d{1,3}?)(?=(\d{3})+$)/$1,/g;
	printf("%15s ", $pretty);
    } else {
	printf("%15d ", $value);
    }
}

sub difference($$) {
    my ($stat, $prev)= @_;
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

    # my @stat_keys = keys %{$prev->{$cpu}};
    # my @stat_keys = keys %{$prev->{0}};
    # print Dumper(\@stat_keys) if $dumper;
    #
    # Fixed manual order of keys
    my @stat_keys = (
          'total',
          'dropped',
          'squeezed',
          'collision',
          'rx_rps',
          'flow_limit',
        );

    # Header
    printf("CPU    ");
    foreach my $key (@stat_keys) {
	printf("%11s/sec ", $key);
    }
    printf("\n");

    # Reset sum hash, just to be sure (not really necessary)
    my %sum;
    foreach my $key (@stat_keys) {
	$sum{$key} = 0;
    }

    my @cpus = (sort keys %$prev);
    foreach my $cpu (@cpus) {
	printf("CPU:%02d ", $cpu);

	foreach my $key (@stat_keys) {
	    # print Dumper($stat->{$cpu}) if $dumper;

	    my $value_now  = $stat->{$cpu}{$key};
	    my $value_prev = $prev->{$cpu}{$key};
	    my $diff = ($value_now - $value_prev) / $period;
	    $sum{$key} += $diff;
	    print_value($diff, $pretty_print);
	    $something_changed++;
	}
	printf("\n");
    }

    # Sum columns
    printf("\nSummed:");
    foreach my $key (@stat_keys) {
	print_value($sum{$key}, $pretty_print);
    }
    printf("\n\n");

    return $something_changed;
}

sub stats_loop() {
    my $collect = $count + 1; # First round was empty (+1)
    my $prev = undef;
    my $stats = {};

    # count == 0 is infinite
    while ( ($count == 0) ? 1 : $collect-- ) {
	my $changes = 0;
	$stats = collect_stats();
	$changes = difference($stats, $prev);
	if (not defined $prev) {
	    print " ***NOTE***: Collecting stats for next round ($delay sec)\n";
	} elsif (!$changes) {
	    print " ***WARN***: No counters changed\n" ;
	}
	$prev = $stats;
	Time::HiRes::sleep($delay);
    }
}
stats_loop();

__END__

=head1 AUTHOR

 Jesper Dangaard Brouer <netoptimizer@brouer.com>

=cut
