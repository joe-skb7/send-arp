#ifndef ARP_H
#define ARP_H

int arp_socket_open(void);
int arp_socket_close(int fd);
int arp_request(int fd, const char *if_name, const char *target_ip);
int arp_reply(int fd);

#endif /* ARP_H */
