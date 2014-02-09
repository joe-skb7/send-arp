#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/ether.h>
#include "arp.h"

/* Length of MAC address string */
#define ETHER_ADDRSTRLEN (ETH_ALEN * 2 + (ETH_ALEN - 1) + 1)

/*
 *
 * Get interface index by interface name.
 *
 * if_name: interface name
 * fd: file descriptor for open socket
 *
 * Return: -1 on error; index on success.
 */
static int get_if_index(const char *if_name, int fd)
{
	struct ifreq ifr;
	size_t if_name_len = strlen(if_name);

	if (if_name_len >= sizeof(ifr.ifr_name)) {
		fprintf(stderr, "Error: Interface name is too long\n");
		return -1;
	}

	memcpy(ifr.ifr_name, if_name, if_name_len);
	ifr.ifr_name[if_name_len] = 0;

	if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
		perror("Failed to retrieve interface index");
		return -1;
	}

	return ifr.ifr_ifindex;
}

/*
 * Get MAC address for interface specified.
 *
 * if_name: interface name
 * fd: file descriptor for open socket
 * mac: pointer to allocated array of 6 elements
 *
 * Return: -1 on error; 0 on success.
 */
static int get_if_mac_addr(const char *if_name, int fd, unsigned char *mac)
{
	struct ifreq ifr;
	size_t if_name_len = strlen(if_name);

	if (if_name_len >= sizeof(ifr.ifr_name)) {
		fprintf(stderr, "Error: Interface name is too long\n");
		return -1;
	}

	memcpy(ifr.ifr_name, if_name, if_name_len);
	ifr.ifr_name[if_name_len] = 0;

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		perror("Failed to obtain HW address");
		return -1;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		fprintf(stderr, "Error: Not an Ethernet interface\n");
		return -1;
	}

	memcpy(mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
}

/*
 * Get IP address for interface specified.
 *
 * if_name: interface name
 * fd: file descriptor for open socket
 * ip: pointer to allocated array of 4 elements
 *
 * Return: -1 on error; 0 on success.
 */
static int get_if_ip_addr(const char *if_name, int fd, unsigned char *ip)
{
	struct ifreq ifr;
	size_t if_name_len = strlen(if_name);
	struct sockaddr_in *addr;

	if (if_name_len >= sizeof(ifr.ifr_name)) {
		fprintf(stderr, "Error: Interface name is too long\n");
		return -1;
	}

	memcpy(ifr.ifr_name, if_name, if_name_len);
	ifr.ifr_name[if_name_len] = 0;

	if (ioctl(fd, SIOCGIFADDR, &ifr) == -1) {
		perror("Failed to obtain IP address");
		return -1;
	}

	/* Skip first 2 bytes in ifr.ifr_addr.sa_data (port number) */
	addr = (struct sockaddr_in *)&ifr.ifr_addr;

	memcpy(ip, &addr->sin_addr.s_addr, 4);

	return 0;
}

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
 */
int arp_reply(int fd)
{
	struct ether_arp reply;

	for (;;) {
		char reply_ip_str[INET_ADDRSTRLEN];
		char reply_mac_str[ETHER_ADDRSTRLEN];
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

		inet_ntop(AF_INET, reply.arp_spa, reply_ip_str,
				INET_ADDRSTRLEN);
		ether_ntoa_r((struct ether_addr *)reply.arp_sha,
				reply_mac_str);

		printf("Reply ip:  %s\n", reply_ip_str);
		printf("Reply mac: %s\n", reply_mac_str);

		break;
	}

	return 0;
}