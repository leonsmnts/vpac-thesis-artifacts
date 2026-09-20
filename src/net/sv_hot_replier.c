#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "../measurement/include/meas_common.h"
#include "../measurement/include/meas_sv.h"
#include "../protection/include/prot_common.h"
#include "../protection/include/prot_dispatch.h"
#include "./memory_lock.h"

#define SV_ETHERTYPE 0x88B5  /* custom experimental EtherType */

typedef struct {
    int sock;
    unsigned char packet[1514]; /* max Ethernet frame size */
    unsigned char my_mac[ETH_ALEN];
    meas_sv_context_t meas_ctx;
    //int packet_count;
} packet_process_data_t;

int init(int argc, char *argv[], packet_process_data_t *data) {
    if (argc != 2) {
        printf("Usage: %s <interface>\n", argv[0]);
        printf("Example: %s eth0\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];

    /* Initialize protection settings/state once. */
    prot_dispatch_init();

    /* Initialize measurement context. */
    meas_sv_context_init(&data->meas_ctx);

    /* Create raw AF_PACKET socket bound to custom EtherType. */
    int sock = socket(AF_PACKET, SOCK_RAW, htons(SV_ETHERTYPE));
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }
    data->sock = sock;

    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
                   interface, strlen(interface) + 1) < 0) {
        perror("setsockopt SO_BINDTODEVICE");
        close(sock);
        return EXIT_FAILURE;
    }

    /* Get interface MAC for info. */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        close(sock);
        return EXIT_FAILURE;
    }

    memcpy(data->my_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    printf("Hot-path replier listening on %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           interface, data->my_mac[0], data->my_mac[1], data->my_mac[2],
           data->my_mac[3], data->my_mac[4], data->my_mac[5]);
    return EXIT_SUCCESS;
}

/* 
 *  Returns 0 on success, 1 if packet ignored (too short), 2 if wrong EtherType, 3 if packet not to us, 4 if sendto failed, -1 on fatal error.
 */
int process_packet(packet_process_data_t *data) {
    struct sockaddr_ll socket_address;
    socklen_t saddr_len = sizeof(socket_address);

    int packet_len = recvfrom(data->sock, data->packet, sizeof(data->packet), 0,
                              (struct sockaddr *)&socket_address,
                              &saddr_len);
    if (packet_len < 0) {
        perror("recvfrom");
        return -1;
    }

    if (packet_len < (int)(sizeof(struct ether_header) + sizeof(sv_payload_t))) {
        /* Too short for our SV payload; ignore. */
        return 1;
    }

    struct ether_header *eh = (struct ether_header *)data->packet;

    /* Basic EtherType check (should match SV_ETHERTYPE). */
    if (ntohs(eh->ether_type) != SV_ETHERTYPE) {
        return 2;
    }

    static const unsigned char broadcast_mac[ETH_ALEN] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    if (memcmp(eh->ether_dhost, data->my_mac, ETH_ALEN) != 0 &&
        memcmp(eh->ether_dhost, broadcast_mac, ETH_ALEN) != 0) {
        return 3;
    }
    
    /* Pointer to our SV payload. In a real system one would worry about
        * alignment and endianness; for this testbed we assume both ends agree.
        */
    unsigned char *payload_ptr = data->packet + sizeof(struct ether_header);
    sv_payload_t sv_payload;
    memcpy(&sv_payload, payload_ptr, sizeof(sv_payload));

    /* Build measurement frame from payload via SV measurement context. */
    vpac_measurement_frame_t frame;
    meas_sv_process_sample(&data->meas_ctx, &sv_payload, &frame);

    /* Call all protection functions. For now we use a fixed dt. */
    protection_output_t prot_out;
    prot_dispatch_all(&frame, 250 /* microseconds */, &prot_out);

    /* prot_out.trip_mask/start_mask can be used here to:
        *   - log decisions,
        *   - set flags in the payload,
        *   - send separate trip messages, etc.
        * For now, we just echo the original frame back.
        */

    /* Proper MAC address swap for echo. */
    memcpy(eh->ether_dhost, eh->ether_shost, ETH_ALEN);       /* dest <- original src */
    memcpy(eh->ether_shost, data->my_mac, ETH_ALEN); /* src <- our MAC */


    /* Prepare socket address for sending. */
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_halen  = ETH_ALEN;
    memcpy(socket_address.sll_addr, eh->ether_dhost, ETH_ALEN);

    int result = sendto(data->sock, data->packet, packet_len, 0,
                        (struct sockaddr *)&socket_address,
                        sizeof(socket_address));
    if (result < 0) {
        perror("sendto");
        return 4;
    } /* else {
        data->packet_count++;
        // Print progress every 1000 packets
        if (data->packet_count % 1000 == 0) {
            printf("Echoed %d packets\n", data->packet_count);
        }
    } */

    return 0;
}

int
main(int argc, char *argv[])
{
    if (lock_process_memory() != 0) {
        return EXIT_FAILURE;
    }

    int ret;
    packet_process_data_t data = { 0 };

    ret = init(argc, argv, &data);
    if (ret != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    printf("Waiting for SV-like packets (EtherType 0x%04x)...\n", SV_ETHERTYPE);
    printf("(Press Ctrl+C to stop)\n\n");
    printf("READY\n\n"); // for automation scripts to detect readiness

    while (1) {
        ret = process_packet(&data);
        if (ret < 0) {
            break;
        }
    }

    close(data.sock);
    return EXIT_SUCCESS;
}
