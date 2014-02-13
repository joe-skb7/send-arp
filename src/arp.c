#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <netinet/ether.h>

#include <arp.h>
#include <network.h>

/* Length of MAC address string */
#define ETHER_ADDRSTRLEN (ETH_ALEN * 2 + (ETH_ALEN - 1) + 1)

/*
 * Fill "addr" structure with destination address.
 */
static void make_dest_addr(struct sockaddr_ll *addr, int ifindex)
{
	const unsigned char ether_broadcast_addr[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	memset(addr, 0, sizeof(*addr));

	addr->sll_family = AF_PACKET;
	addr->sll_ifindex = ifindex;
	addr->sll_halen = ETHER_ADDR_LEN;
	addr->sll_protocol = htons(ETH_P_ARP);
	memcpy(addr->sll_addr, ether_broadcast_addr, ETHER_ADDR_LEN);
}

/*
 * Construct Ethernet frame payload: ARP request.
 *
 * fd: file descriptor for open socket
 * req: pointer to allocated ARP packet struct.
 * if_name: interface name
 * target_ip: IP address where to send (e.g. "192.168.1.1")
 *
 * "tpa" -- IP address of target.
 * "tha" -- will contain MAC address of target.
 */
static int make_arp_payload(int fd, struct ether_arp *req,
		const char *if_name, const char *target_ip)
{
	struct in_addr target_ip_addr = {0};
	int ret;
	const unsigned char ether_broadcast_addr[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	req->arp_hrd = htons(ARPHRD_ETHER);
	req->arp_pro = htons(ETH_P_IP);
	req->arp_hln = ETHER_ADDR_LEN;
	req->arp_pln = sizeof(in_addr_t);
	req->arp_op = htons(ARPOP_REQUEST);

	memset(&req->arp_tha, 0, sizeof(req->arp_tha));

	if (!inet_aton(target_ip, &target_ip_addr)) {
		fprintf(stderr, "Error: %s is not a valid IP address\n",
				target_ip);
		return -1;
	}

	memcpy(&req->arp_tpa, &target_ip_addr.s_addr, sizeof(req->arp_tpa));

	ret = get_if_mac_addr(if_name, fd, req->arp_sha);
	if (ret == -1)
		return -1;

	ret = get_if_ip_addr(if_name, fd, req->arp_spa);
	if (ret == -1)
		return -1;

	memcpy(req->arp_tha, ether_broadcast_addr, ETHER_ADDR_LEN);

	return 0;
}

/*
 * Open socket for ARP packets.
 *
 * Return: socket file descriptor or -1 for error.
 */
int arp_socket_open(void)
{
	int fd;

	fd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
	if (fd == -1) {
		perror("Failed to create socket");
		return -1;
	}

	return fd;
}

int arp_socket_close(int fd)
{
	if (close(fd) == -1) {
		perror("Failed to close socket fd");
		return -1;
	}

	return 0;
}

/*
 * Send ARP request to "if_name" interface.
 *
 * fd: file descriptor for open socket
 * if_name: interface name (e.g. "wlan0", "eth0", etc.)
 * target_ip: IP address where to send (e.g. "192.168.1.1")
 */
int arp_request(int fd, const char *if_name, const char *target_ip)
{
	int ifindex;
	struct sockaddr_ll addr;
	struct ether_arp req;
	int ret;

	ifindex = get_if_index(if_name, fd);
	if (ifindex == -1)
		return -1;

	make_dest_addr(&addr, ifindex);

	ret = make_arp_payload(fd, &req, if_name, target_ip);
	if (ret == -1)
		return -1;

	ret = sendto(fd, &req, sizeof(req), 0, (struct sockaddr*)&addr,
			sizeof(addr));
	if (ret == -1) {
		perror("Unable to send ARP request");
		return -1;
	}

	return 0;
}

/*
 * Wait for ARP reply packet and process it.
 *
 * fd: file descriptor for open socket
 * data: pointer to allocated structure; will contain reply result
 *
 * Return: 0 on success, -1 on error
 */
int arp_reply(int fd, struct arp_reply_data *data)
{
	struct ether_arp reply;

	for (;;) {
		int ret;

		ret = recv(fd, &reply, sizeof(reply), 0);

		if (ret == -1) {
			perror("Error while recv()");
			return -1;
		}

		if (ret == 0) {
			printf("recv() = 0\n");
			return -1;
		}

		if (ntohs(reply.arp_op) != ARPOP_REPLY) {
			printf("### not arp reply; skip\n");
			continue;
		}

		inet_ntop(AF_INET, reply.arp_spa, data->reply_ip,
				INET_ADDRSTRLEN);
		ether_ntoa_r((struct ether_addr *)reply.arp_sha,
				data->reply_mac);

		break;
	}

	return 0;
}
