/*
 * switch.h
 */

struct switch_fwd {
	int valid;	// validity flag
	char dst;	// destination host ID
	int port;	// port # (0-3)
};

void switch_main(int switch_id);


