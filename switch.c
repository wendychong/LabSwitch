 /*
  * switch.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "net.h"
#include "man.h"
#include "host.h"
#include "switch.h"
#include "packet.h"

#define MAX_FILE_BUFFER 1000
#define MAX_MSG_LENGTH 100
#define MAX_DIR_NAME 100
#define MAX_FILE_NAME 100
#define PKT_PAYLOAD_MAX 100
#define TENMILLISEC 10000   /* 10 millisecond sleep */

/*
 *  Main
 */

void switch_main(int switch_id)
{

/* State */
char dir[MAX_DIR_NAME];
int dir_valid = 0;

char man_msg[MAN_MSG_LENGTH];
char man_reply_msg[MAN_MSG_LENGTH];
char man_cmd;

struct net_port *node_port_list;
struct net_port **node_port;  // Array of pointers to node ports
int node_port_num;            // Number of node ports

int ping_reply_received;

int i, k, n;
int dst;
char name[MAX_FILE_NAME];
char string[PKT_PAYLOAD_MAX+1];

FILE *fp;

struct packet *in_packet; /* Incoming packet */
struct packet *new_packet;

struct net_port *p;
struct host_job *new_job;
struct host_job *new_job2;

struct job_queue job_q;

/*
 * Create an array node_port[ ] to store the network link ports
 * at the switch.  The number of ports is node_port_num
 */
node_port_list = net_get_port_list(switch_id);

	/*  Count the number of network link ports */
node_port_num = 0;
for (p=node_port_list; p!=NULL; p=p->next) {
	node_port_num++;
}
	/* Create memory space for the array */
node_port = (struct net_port **)
	malloc(node_port_num*sizeof(struct net_port *));

	/* Load ports into the array */
p = node_port_list;
for (k = 0; k < node_port_num; k++) {
	node_port[k] = p;
	p = p->next;
}

/* Initialize the job queue */
job_q_init(&job_q);

while(1) {

	/*
	 * Get packets from incoming links and translate to jobs
  	 * Put jobs in job queue
 	 */

	for (k = 0; k < node_port_num; k++) { /* Scan all ports */

		in_packet = (struct packet *) malloc(sizeof(struct packet));
		n = packet_recv(node_port[k], in_packet);

		if ((n > 0) && ((int) in_packet->dst == switch_id)) {
			new_job = (struct host_job *)
				malloc(sizeof(struct host_job));
			new_job->in_port_index = k;
			new_job->packet = in_packet;

			switch(in_packet->type) {
				/* Consider the packet type */

				/*
				 * The next two packet types are
				 * the ping request and ping reply
				 */
				case (char) PKT_PING_REQ:
					new_job->type = JOB_PING_SEND_REPLY;
					job_q_add(&job_q, new_job);
					break;

				case (char) PKT_PING_REPLY:
					ping_reply_received = 1;
					free(in_packet);
					free(new_job);
					break;

				/*
				 * The next two packet types
				 * are for the upload file operation.
				 *
				 * The first type is the start packet
				 * which includes the file name in
				 * the payload.
				 *
				 * The second type is the end packet
				 * which carries the content of the file
				 * in its payload
				 */

				case (char) PKT_FILE_UPLOAD_START:
					new_job->type
						= JOB_FILE_UPLOAD_RECV_START;
					job_q_add(&job_q, new_job);
					break;

				case (char) PKT_FILE_UPLOAD_END:
					new_job->type
						= JOB_FILE_UPLOAD_RECV_END;
					job_q_add(&job_q, new_job);
					break;
				default:
					free(in_packet);
					free(new_job);
			}
		}
		else {
			free(in_packet);
		}
	}

	/*
 	 * Execute one job in the job queue
 	 */

	if (job_q_num(&job_q) > 0) {

		/* Get a new job from the job queue */
		new_job = job_q_remove(&job_q);

	}


	/* The switch goes to sleep for 10 ms */
	usleep(TENMILLISEC);

} /* End of while loop */

}




