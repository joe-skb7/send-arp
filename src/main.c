/*
 * Author: Sam Protsenko <joe.skb7@gmail.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include "arp.h"

struct params {
	char *iface_name;
	char *target_ip;
};

static int sfd; /* socket file descriptor */
static struct params params;

static void print_usage(const char *app_name)
{
	fprintf(stderr, "Usage: %s -i iface -a target_ip\n", app_name);
}

static int parse_params(int argc, char *argv[], struct params *params)
{
	int opt;

	while ((opt = getopt(argc, argv, "i:a:")) != -1) {
		switch (opt) {
		case 'i':
			params->iface_name = strdup(optarg);
			break;
		case 'a':
			params->target_ip = strdup(optarg);
			break;
		default:
			fprintf(stderr, "Unknown parameter\n");
			return -1;
		}
	}

	if (params->iface_name == NULL || params->target_ip == NULL) {
		fprintf(stderr, "Wrong parameters\n");
		return -1;
	}

	return 0;
}

static void free_params(struct params *params)
{
	free(params->iface_name);
	free(params->target_ip);
}

static void sigint_handler(int signum)
{
	if (arp_socket_close(sfd) == -1)
		fprintf(stderr, "Exiting anyway\n");

	free_params(&params);

	exit(128 + SIGINT);
}

int main(int argc, char *argv[])
{
	int ret;
	struct arp_reply_data reply_data;

	ret = parse_params(argc, argv, &params);
	if (ret == -1) {
		print_usage(argv[0]);
		ret = EXIT_FAILURE;
		goto free_params;
	}

	sfd = arp_socket_open();
	if (sfd == -1) {
		ret = EXIT_FAILURE;
		goto free_params;
	}

	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		perror("Unable to set signal handler");
		goto close_fd;
	}

	ret = arp_request(sfd, params.iface_name, params.target_ip);
	if (ret == -1) {
		ret = EXIT_FAILURE;
		goto close_fd;
	}

	ret = arp_reply(sfd, &reply_data);
	if (ret == -1) {
		ret = EXIT_FAILURE;
		goto close_fd;
	}

	printf("Reply ip:  %s\n", reply_data.reply_ip);
	printf("Reply mac: %s\n", reply_data.reply_mac);

close_fd:
	ret = arp_socket_close(sfd);
	if (ret == -1)
		ret = EXIT_FAILURE;

free_params:
	free_params(&params);

	return ret;
}
