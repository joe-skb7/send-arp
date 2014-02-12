#ifndef ARP_H
#define ARP_H

#define IP_STRLEN	16
#define MAC_STRLEN	18

struct arp_reply_data {
	char reply_ip[IP_STRLEN];
	char reply_mac[MAC_STRLEN];
};

int arp_socket_open(void);
int arp_socket_close(int fd);
int arp_request(int fd, const char *if_name, const char *target_ip);
int arp_reply(int fd, struct arp_reply_data *data);

#endif /* ARP_H */
