#ifndef NETWORK_H
#define NETWORK_H

int get_if_index(const char *if_name, int fd);
int get_if_mac_addr(const char *if_name, int fd, unsigned char *mac);
int get_if_ip_addr(const char *if_name, int fd, unsigned char *ip);

#endif /* NETWORK_H */
