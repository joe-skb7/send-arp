#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <net/if.h>

#include <network.h>

/*
 *
 * Get interface index by interface name.
 *
 * if_name: interface name
 * fd: file descriptor for open socket
 *
 * Return: -1 on error; index on success.
 */
int get_if_index(const char *if_name, int fd)
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
int get_if_mac_addr(const char *if_name, int fd, unsigned char *mac)
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
int get_if_ip_addr(const char *if_name, int fd, unsigned char *ip)
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
