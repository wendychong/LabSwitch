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

#define TENMILLISEC 10000   /* 10 millisecond sleep */
#define MAX_FWD_LENGTH 100

enum bool {FALSE, TRUE};

/*
 *  Main
 */

void switch_main(int switch_id)
{

	/* State */

	struct net_port *node_port_list;
	struct net_port **node_port;  // Array of pointers to node ports
	int node_port_num;            // Number of node ports

	int ping_reply_received;

	int i, k, n;
	int dst;

	FILE *fp;

	struct packet *in_packet; /* Incoming packet */
	struct packet *new_packet;

	struct net_port *p;
	struct host_job *new_job;
	struct host_job *new_job2;

	struct job_queue job_q;

	struct switch_fwd fwd_table[MAX_FWD_LENGTH];

	for (int i=0; i<MAX_FWD_LENGTH; i++)
		fwd_table[i].valid = 0;

	/*
	 * Create an array node_port[ ] to store the network link ports
	 * at the switch.  The number of ports is node_port_num
	 */
	node_port_list = net_get_port_list(switch_id);

		/*  Count the number of network link ports */
	node_port_num = 0;
	for (p = node_port_list; p != NULL; p = p->next) {
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

	int tree_root = switch_id, tree_root_dist = 0, tree_parent = -1;
	int tree_wait = -1, tree_wait_max = 10;
	int tree_port[node_port_num];

	for (i=0; i<node_port_num; i++) {
		tree_port[k] = FALSE;
	}

	/* Initialize the job queue */
	job_q_init(&job_q);

	int flag;

	// printf("\ts%d: online\n", switch_id);

	while(1)
	{

		/*
		 * Get packets from incoming links and send them back out
	  	 * Put jobs in job queue
	 	 */

		flag = 0;

		if ((tree_wait = (tree_wait + 1) % tree_wait_max) == 0) {
			new_packet = (struct packet *)
							malloc(sizeof(struct packet));
					new_packet->src = (char) switch_id;
					new_packet->dst = (char) -1;
					new_packet->type = (char) PKT_TREE;
			sprintf(new_packet->payload, "%d %d %c %c", tree_root, tree_root_dist, 'S', (tree_parent == k ? 'Y' : 'N' ));

			new_packet->length = strlen(new_packet->payload)+1;

			new_job = (struct host_job *)
				malloc(sizeof(struct host_job));

			new_job->type = JOB_TREE_SEND;
			new_job->packet = new_packet;
			job_q_add(&job_q, new_job);
		}

		for (k = 0; k < node_port_num; k++) { /* Scan all ports */

			in_packet = (struct packet *) malloc(sizeof(struct packet));
			n = packet_recv(node_port[k], in_packet);

			if (n > 0) {
				new_job = (struct host_job *)
					malloc(sizeof(struct host_job));
				new_job->in_port_index = k;
				new_job->packet = in_packet;
				if (new_job->packet->type == PKT_TREE)
				{
					new_job->type = JOB_TREE_UPDATE;
				}
				else
				{
					printf("\ts%d: get p%d h%d\n", switch_id, k, new_job->packet->src);
				}
				job_q_add(&job_q, new_job);

				//printf("\ts%d: get on p%d: h%d ~ h%d\n", switch_id, k, new_job->packet->src, new_job->packet->dst);

				flag = 1;
			}
			else {
				free(in_packet);
			}
		}

		/*if (flag)
			printf("\ts%d: flag tripped\n",switch_id);*/

		/*
	 	 * Execute one job in the job queue
	 	 */

		flag = 0;

		if (job_q_num(&job_q) > 0) {

			// printf("\ts%d: jobs available\n", switch_id);
			/* Get a new job from the job queue */
			new_job = job_q_remove(&job_q);

			int vport = -1; //can't go inside switch
			/*printf("\ts%d: routing h%d ~ h%d\n",
					switch_id, new_job->packet->src, new_job->packet->dst);*/

			switch (new_job->type)
			{
				case JOB_TREE_SEND:
					//printf("\ts%d: doing job \"%s\": \"%s\"\n", switch_id, "tree send", new_job->packet->payload);
					for (k=0; k<node_port_num; k++)
					{
						packet_send(node_port[k], new_job->packet);
						//printf("\ts%d: sent p%d\n", switch_id, k);
					}

					break;

				case JOB_TREE_UPDATE:
				{
					int packet_root, packet_root_dist;
					char packet_root_c, packet_sender_type, packet_sender_child;
					sscanf(new_job->packet->payload, "%d %d %c %c", &packet_root, &packet_root_dist, &packet_sender_type, &packet_sender_child);
					//packet_root = (int) packet_root_c;
					k = new_job->in_port_index;
					//printf("\ts%d: doing job \"%s\": \"%d %d %c %c\"\n", switch_id, "tree update", packet_root, packet_root_dist, packet_sender_type, packet_sender_child);
					// Update localRootID, localRootDist, and localParent
					if (packet_sender_type == 'S')
					{
						//update root, parent, distance
						if (packet_root < tree_root)
						{
							tree_root = packet_root;
							tree_parent = new_job->in_port_index;
							tree_root_dist = packet_root_dist + 1;
							printf("\ts%d: new root s%d new parent s%d(%d) dist %d\n", switch_id, tree_root, tree_parent, k, tree_root_dist);
						}
						else if (packet_root == tree_root)
						{
							if (tree_root_dist > packet_root_dist + 1)
							{
								tree_parent = k;
								tree_root_dist = packet_root_dist + 1;
								printf("\ts%d: new root s%d dist %d\n", switch_id, tree_root, tree_root_dist);
							}
						}
						//update ports
						//printf("\ts%d: parent %d vs %d\n", switch_id, tree_parent, k);

						if (tree_parent == k)
						{
							if (!tree_port[k])
								printf("\ts%d: added p%d to tree (is parent)\n", switch_id, k);
							tree_port[k] = TRUE;
						}
						else if (packet_sender_child == 'Y')
						{
							if (!tree_port[k])
								printf("\ts%d: added p%d to tree (is child)\n", switch_id, k);
							tree_port[k] = TRUE;
						}
						else
						{
							/*if (tree_port[k])
								printf("\ts%d: removed p%d from tree (is nothing)\n", switch_id, k);*/
							tree_port[k] = FALSE;
						}
					}
					else if (packet_sender_type == 'H')
					{
						if (!tree_port[k])
							printf("\ts%d: added p%d to tree (is host)\n", switch_id, k);
						tree_port[k] = TRUE;
					}
					else
					{
						/*if (tree_port[k])
							printf("\ts%d: removed p%d from tree (is unknown)\n", switch_id, k);*/
						tree_port[k] = FALSE;
					}
					break;
				}
				default:
					for (i=0; i<MAX_FWD_LENGTH; i++)
					{
						//scan for valid match
						if (fwd_table[i].valid && fwd_table[i].dst == new_job->packet->src)
						{
							vport = fwd_table[i].port;
							/*printf("\ts%d: found h%d on p%d\n",
								switch_id, fwd_table[i].dst, fwd_table[i].port);*/
							break;
						}
					}
					if (vport == -1) //need to add an entry
					{
						for (i=0; i<MAX_FWD_LENGTH; i++)
						{
							if (!fwd_table[i].valid)
							{
								fwd_table[i].valid = 1;
								fwd_table[i].dst = new_job->packet->src;
								fwd_table[i].port = new_job->in_port_index;
								vport = fwd_table[i].port;
								/*printf("\ts%d: added h%d on p%d\n",
									switch_id, fwd_table[i].dst, fwd_table[i].port);*/
								break;
							}
						}
					}

					vport = -1;
					for (i=0; i<MAX_FWD_LENGTH; i++)
					{
						//scan for valid match
						if (fwd_table[i].valid && fwd_table[i].dst == new_job->packet->dst)
						{
							vport = fwd_table[i].port;
							/*printf("\tS-%d: FOUND %d/%d/%d\n",
								switch_id, i, fwd_table[i].dst, fwd_table[i].port);*/
							break;
						}
					}

					if (vport > -1)
					{
						packet_send(node_port[vport], new_job->packet);
						printf("\ts%d: sent p%d h%d\n", switch_id, vport, new_job->packet->dst);
					}
					else
					{
						for (k=0; k<node_port_num; k++)
						{
							if (k == new_job->in_port_index)
							{
								printf("\ts%d: skipping in-port p%d\n", switch_id, k);
								continue;
							}
							if (!tree_port[k])
							{
								printf("\ts%d: skipping non-tree p%d: %d\n", switch_id, k, tree_port[k]);
								continue;
							}

							packet_send(node_port[k], new_job->packet);
							printf("\ts%d: sent p%d h%d\n", switch_id, k, new_job->packet->dst);
						}
					}
					break;
			}
			free(new_job->packet);
			free(new_job);
		}

		/*if (flag)
			printf("\ts%d: flag tripped\n",switch_id);*/

		/* The switch goes to sleep for 10 ms */
		usleep(TENMILLISEC);

	} /* End of while loop */

}




