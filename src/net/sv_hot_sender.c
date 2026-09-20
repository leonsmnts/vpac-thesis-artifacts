#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>

// #include "../timing.h"
#include "../measurement/include/meas_common.h"
#include "./memory_lock.h"

#define SV_ETHERTYPE 0x88B5
#define L2_SEND_FRAME_RATE_US 250

typedef struct {
    int sock;
    int ifindex;

    unsigned char src_mac[ETH_ALEN];
    unsigned char dest_mac[ETH_ALEN];

    struct sockaddr_ll socket_address;

    unsigned char packet[1514];  // Max Ethernet frame
    int packet_len;

    sv_payload_t *payload;

    uint32_t iterations;
//    perf_timer_t *busy_timer;
} packet_send_data_t;

static void
busy_wait_us(int64_t delay_us)
{
    if (delay_us <= 0) {
        return;
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    const int64_t start_ns =
        (int64_t)start.tv_sec * 1000000000LL + start.tv_nsec;
    const int64_t target_ns = start_ns + delay_us * 1000LL;

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t now_ns =
            (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
        if (now_ns >= target_ns) {
            break;
        }
    }
}

static double
do_busy_wait(void)
{
    busy_wait_us(L2_SEND_FRAME_RATE_US);
    return 0.0;
}

static int
parse_mac_address(const char *mac_str, unsigned char out_mac[ETH_ALEN])
{
    if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &out_mac[0], &out_mac[1], &out_mac[2],
               &out_mac[3], &out_mac[4], &out_mac[5]) != 6) {
        return -1;
    }
    return 0;
}

static void
initialize_payload_template(sv_payload_t *payload)
{
    if (!payload) {
        return;
    }

    payload->frame_id = 0;

    payload->sender_timestamp_ns = 0;

    payload->local_current[0] = 100.0f;
    payload->local_current[1] = 110.0f;
    payload->local_current[2] = 120.0f;
    payload->local_current[3] =  50.0f;

    payload->local_voltage[0] = 10.0f;
    payload->local_voltage[1] = 10.0f;
    payload->local_voltage[2] = 10.0f;
    payload->local_voltage[3] =  0.0f;

    payload->remote_current[0] = 80.0f;
    payload->remote_current[1] = 90.0f;
    payload->remote_current[2] = 100.0f;
    payload->remote_current[3] =  0.0f;
}

static int
init(int argc, char *argv[], packet_send_data_t *data)
{
    if (argc != 4) {
        printf("Usage: %s <interface> <dest_mac> <iterations>\n", argv[0]);
        printf("Example: %s eth0 52:54:00:12:34:56 1000\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];
    const char *dest_mac_str = argv[2];
    data->iterations = (uint32_t)atoi(argv[3]);

    if (parse_mac_address(dest_mac_str, data->dest_mac) != 0) {
        fprintf(stderr, "Invalid dest MAC format: %s\n", dest_mac_str);
        return EXIT_FAILURE;
    }

    /* data->busy_timer = perf_timer_create(do_busy_wait,
                                         (int)data->iterations,
                                         "hot_sender_busy_wait.csv");
    if (!data->busy_timer) {
        fprintf(stderr, "Failed to create perf timer\n");
        return EXIT_FAILURE;
    }
    */

    data->sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (data->sock < 0) {
        perror("socket");
        //perf_timer_free(data->busy_timer);
        return EXIT_FAILURE;
    }

    // ============ GET INTERFACE INDEX ============
    // ifreq = interface request structure (kernel ABI for network interface queries)
    struct ifreq ifr = { 0 };
    // Copy interface name (e.g. "eth0")
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // SIOCGIFINDEX = "Socket I/O Control Get InterFace INDEX"
    // Kernel fills in ifr.ifr_ifindex with the interface ID
    if (ioctl(data->sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(data->sock);
        //perf_timer_free(data->busy_timer);
        return EXIT_FAILURE;
    }
    data->ifindex = ifr.ifr_ifindex;

    // SIOCGIFHWADDR = "Socket I/O Control Get InterFace HardWAre ADdRess"
    // Kernel fills in ifr.ifr_hwaddr.sa_data with 6-byte MAC
    if (ioctl(data->sock, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        close(data->sock);
        //perf_timer_free(data->busy_timer);
        return EXIT_FAILURE;
    }
    memcpy(data->src_mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           data->src_mac[0], data->src_mac[1], data->src_mac[2],
           data->src_mac[3], data->src_mac[4], data->src_mac[5]);
    printf("Dest MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           data->dest_mac[0], data->dest_mac[1], data->dest_mac[2],
           data->dest_mac[3], data->dest_mac[4], data->dest_mac[5]);

    memset(&data->socket_address, 0, sizeof(data->socket_address));
    data->socket_address.sll_family = AF_PACKET;
    data->socket_address.sll_ifindex = data->ifindex;
    data->socket_address.sll_halen = ETH_ALEN;
    memcpy(data->socket_address.sll_addr, data->dest_mac, ETH_ALEN);

    // ============ PREPARE ETHERNET FRAME ============
    // Ethernet frame structure:
    // [dest MAC 6B] [src MAC 6B] [EtherType 2B] [Payload nB] [CRC 4B]
    struct ether_header *eh = (struct ether_header *)data->packet;
    memcpy(eh->ether_dhost, data->dest_mac, ETH_ALEN);
    memcpy(eh->ether_shost, data->src_mac, ETH_ALEN);
    eh->ether_type = htons(SV_ETHERTYPE);

    data->payload =
        (sv_payload_t *)(data->packet + sizeof(struct ether_header));
    initialize_payload_template(data->payload);

    data->packet_len = (int)(sizeof(struct ether_header) + sizeof(sv_payload_t));

    return EXIT_SUCCESS;
}

static int
update_payload_for_iteration(sv_payload_t *payload, uint32_t iteration)
{
    struct timespec ts;

    float delta = (float)(iteration % 10);

    payload->frame_id = iteration;

    payload->local_current[0] = 100.0f + delta;
    payload->local_current[1] = 110.0f + delta;
    payload->local_current[2] = 120.0f + delta;
    payload->local_current[3] =  50.0f;

    payload->local_voltage[0] = 10.0f;
    payload->local_voltage[1] = 10.5f;
    payload->local_voltage[2] = 11.0f;
    payload->local_voltage[3] =  0.0f;

    payload->remote_current[0] = 80.0f + 0.5f * delta;
    payload->remote_current[1] = 90.0f + 0.5f * delta;
    payload->remote_current[2] = 100.0f + 0.5f * delta;
    payload->remote_current[3] = 0.0f;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        perror("clock_gettime");
        return 1;
    }
    
    payload->sender_timestamp_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

    return 0;
}

static int
send_one_packet(packet_send_data_t *data, uint32_t iteration)
{
    if (update_payload_for_iteration(data->payload, iteration) != 0) {
        return 1;
    }

    // ============ SEND PACKET ============
    // sendto(socket, data, length, flags, destination_address, address_length)
    int result = sendto(data->sock,
                        data->packet,
                        data->packet_len,
                        0,
                        (struct sockaddr *)&data->socket_address,
                        sizeof(data->socket_address));
    if (result < 0) {
        perror("sendto");
        return 2;
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    if (lock_process_memory() != 0) {
        return EXIT_FAILURE;
    }

    int ret;
    packet_send_data_t data = { 0 };

    ret = init(argc, argv, &data);
    if (ret != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    printf("\nSending %d packets...\n", data.iterations);
    printf("Packet size: %d bytes (Ethernet header + %zu byte SV payload)\n", data.packet_len, sizeof(sv_payload_t));

    for (uint32_t i = 0; i < data.iterations; i++) {
        if (send_one_packet(&data, i) != 0) {
            break;
        }

        // Print progress every 10000 packets
        if ((i + 1) % 10000 == 0) {
            printf("Sent %u packets...\n", i + 1);
        }

        // do not time execution of wait anymore, just wait for time
        do_busy_wait();
        /* perf_timer_measure(data.busy_timer); */
    }

    /*
    perf_timer_stats(data.busy_timer, "busy_wait_us(250)");
    perf_timer_save(data.busy_timer);
    perf_timer_free(data.busy_timer);
    */

    close(data.sock);
    printf("Done.\n");
    return EXIT_SUCCESS;
}
