#include <sys/timex.h>
#include <stdio.h>

static long get_tai_offset(void)
{
	struct ntptimeval ntpt;

	ntp_gettimex(&ntpt);
	return ntpt.tai;
}

int main(int argc, char *argv[])
{
	printf("On this system the TAI offset is: %ld\n", get_tai_offset());
}
