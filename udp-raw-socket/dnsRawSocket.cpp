//
// Created by vitekkor on 15.10.24.
//

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#define DNS_PORT 53
#define DNS_TYPE_A 1
#define DNS_TYPE_AAAA 28

struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t quest_count;
    uint16_t answ_count;
    uint16_t auth_rrs_count;
    uint16_t ad_rrs_count;
};

struct DNSQuestion {
    uint16_t type;
    uint16_t class_code;
};

static unsigned short compute_checksum(const unsigned short *addr, unsigned int count) {
    unsigned long sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    // if any bytes left, pad the bytes and add
    if (count > 0) {
        sum += ((*addr) & htons(0xFF00));
    }
    // Fold sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    // one's complement
    sum = ~sum;
    return static_cast<unsigned short>(sum);
}

void compute_ip_checksum(iphdr *iphdrp) {
    iphdrp->check = 0;
    iphdrp->check = compute_checksum(reinterpret_cast<unsigned short *>(iphdrp), iphdrp->ihl << 2);
}

/* set udp checksum: given IP header and UDP datagram */
void compute_udp_checksum(const iphdr *iphd, udphdr *udphd) {
    unsigned long sum = 0;
    unsigned short udpLen = htons(udphd->len);
    const unsigned short *ipPayload = reinterpret_cast<unsigned short *>(udphd);
    // the source ip
    sum += (iphd->saddr >> 16) & 0xFFFF;
    sum += (iphd->saddr) & 0xFFFF;
    // the dest ip
    sum += (iphd->daddr >> 16) & 0xFFFF;
    sum += (iphd->daddr) & 0xFFFF;
    // protocol and reserved: 17
    sum += htons(IPPROTO_UDP);
    // the length
    sum += udphd->len;

    // initialize checksum to 0
    udphd->check = 0;
    while (udpLen > 1) {
        sum += *ipPayload++;
        udpLen -= 2;
    }
    // if any bytes left, pad the bytes and add
    if (udpLen > 0) {
        sum += ((*ipPayload) & htons(0xFF00));
    }
    // Fold sum to 16 bits: add carrier to result
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    // one's complementn
    sum = ~sum;
    // set computation result
    udphd->check = (static_cast<unsigned short>(sum) == 0x0000) ? 0xFFFF : static_cast<unsigned short>(sum);
}

unsigned char change_to_dns_name_format0(unsigned char *dns, char *host) {
    int lock = 0;
    strcat(host, ".");

    for (int i = 0; i < strlen(host); i++) {
        if (host[i] == '.') {
            *dns++ = i - lock;
            for (; lock < i; lock++) {
                *dns++ = host[lock];
            }
            lock++;
        }
    }
    *dns++;
    *dns = '\0';
    return strlen(host) + 1;
}

void fill_ip_header(const char *source_addr, const char *dns_server_ip, iphdr *iph) {
    iph->version = 4;
    iph->ihl = 5;
    iph->tos = 0;
    iph->id = htons(std::rand() % 65535);
    iph->frag_off = 0;
    iph->ttl = 65;
    iph->protocol = IPPROTO_UDP;
    iph->saddr = inet_addr(source_addr);
    iph->daddr = inet_addr(dns_server_ip);
}

void fill_udp_header(udphdr *udph) {
    udph->source = htons(std::rand() % (65536 - 30000) + 30000);
    udph->dest = htons(DNS_PORT);
    udph->check = 0;
}

void fill_dns_header(DNSHeader *dns_header) {
    dns_header->id = htons(std::rand() % 65535);
    dns_header->flags = htons(0x0100); // standard query
    dns_header->quest_count = htons(1); // Questions: 1
    dns_header->answ_count = 0; // Answer RRs: 0
    dns_header->auth_rrs_count = 0; // Authority RRs: 0
    dns_header->ad_rrs_count = 0; // Additional RRs: 0
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <local_ip> <dns server ip> <domain> <record type>\n", argv[0]);
        return 1;
    }

    const char *source_addr = argv[1];
    const char *dns_server_ip = argv[2];
    const char *host = argv[3];
    char *domain = strdup(argv[3]);
    const char *record_type = argv[4];
    printf("Using domain server: \nName: %s\nRecord type: %s\n", dns_server_ip, record_type);

    std::srand(time(nullptr));

    unsigned char buf[65536] = {};
    iphdr *iph = reinterpret_cast<iphdr *>(buf);
    fill_ip_header(source_addr, dns_server_ip, iph);

    udphdr *udph = reinterpret_cast<udphdr *>(buf + sizeof(iphdr));
    fill_udp_header(udph);


    DNSHeader *dns_header = reinterpret_cast<DNSHeader *>(buf + sizeof(iphdr) + sizeof(udphdr));
    fill_dns_header(dns_header);

    unsigned char *qname = &buf[sizeof(iphdr) + sizeof(udphdr) + sizeof(DNSHeader)];

    const auto query_size = change_to_dns_name_format0(qname, domain);
    DNSQuestion *dns_question = reinterpret_cast<DNSQuestion *>(
        buf + sizeof(iphdr) + sizeof(udphdr) + sizeof(DNSHeader) + query_size
    );
    dns_question->type = strcmp(record_type, "A") == 0 ? htons(1) : htons(28);
    dns_question->class_code = htons(0x0001); // Class IN

    size_t udp_tot_len = sizeof(udphdr) + sizeof(DNSHeader) + sizeof(DNSQuestion) + query_size;
    udph->len = htons(udp_tot_len);

    size_t ip_tot_len = sizeof(iphdr) + udp_tot_len;
    iph->tot_len = htons(ip_tot_len);
    compute_ip_checksum(iph);
    compute_udp_checksum(iph, udph);
    unsigned long outgoing_packet_n = ip_tot_len;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DNS_PORT);
    dest.sin_addr.s_addr = inet_addr(dns_server_ip);

    printf("\nCreating socket...");
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    printf("\nCreating receive socket...");
    int recv_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recv_sockfd < 0) {
        std::cerr << "Failed to create receive socket" << std::endl;
        close(sockfd);
        return 1;
    }

    sockaddr_in recv_addr{};
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = udph->source;
    recv_addr.sin_addr.s_addr = inet_addr(source_addr);
    if (bind(recv_sockfd, reinterpret_cast<sockaddr *>(&recv_addr), sizeof(recv_addr)) < 0) {
        std::cerr << "Failed to bind receive socket" << std::endl;
        close(sockfd);
        close(recv_sockfd);
        return 1;
    }
    timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(recv_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    printf("\nSending Packet...");
    if (sendto(sockfd, buf, outgoing_packet_n, 0, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) < 0) {
        std::cerr << "Failed to send packet" << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "\nReceiving Packet..." << std::endl;
    sockaddr_in sender{};
    sender.sin_family = AF_INET;
    sender.sin_port = udph->source;
    sender.sin_addr.s_addr = inet_addr(source_addr);
    socklen_t sender_len = sizeof(sender);
    const unsigned short int incoming_packet_n =
            recvfrom(recv_sockfd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&sender), &sender_len);
    if (incoming_packet_n < 0) {
        std::cerr << "Failed to receive packet" << std::endl;
        close(sockfd);
        return 1;
    }

    close(sockfd);
    close(recv_sockfd);

    dns_header = reinterpret_cast<DNSHeader *>(buf);
    uint16_t answer_count = ntohs(dns_header->answ_count);

    std::cout << "Received " << answer_count << " answer(s):" << std::endl;
    unsigned char *answer_ptr = buf + sizeof(DNSHeader) + query_size + sizeof(DNSQuestion);
    for (uint16_t i = 0; i < answer_count; i++) {
        answer_ptr += 2;  // Skip name pointer

        const uint16_t type = ntohs(*reinterpret_cast<uint16_t *>(answer_ptr));
        answer_ptr += 2;

        answer_ptr += 2;  // Skip class pointer
        answer_ptr += 4; // skip ttl
        uint16_t rdlength = ntohs(*(uint16_t *)answer_ptr);
        answer_ptr += 2;  // Move to rdata
        if (type == DNS_TYPE_A) {
            in_addr in_add{};
            memcpy(&in_add.s_addr, answer_ptr, sizeof(in_add.s_addr));
            char *ipv4_str = inet_ntoa(in_add);
            std::cout << host << " has address: " << ipv4_str << std::endl;
        } else if (type == DNS_TYPE_AAAA) {
            char ipv6_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, answer_ptr, ipv6_str, INET6_ADDRSTRLEN);
            std::cout << host << " has address: " << ipv6_str << std::endl;
        } else {
            char ipv6_str[rdlength];
            memcpy(&ipv6_str, answer_ptr, rdlength);
            std::cout << host << " has address (type " << type << "): " << ipv6_str << std::endl;
        }
        answer_ptr += rdlength;
    }
    return 0;
}
