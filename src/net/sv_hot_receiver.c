#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/socket.h>

#include "../measurement/include/meas_common.h"
#include "./memory_lock.h"

#define SV_ETHERTYPE 0x88B5
#define MAX_ETH_FRAME_SIZE 1514

typedef struct {
    int sock;
    int expected_packets;
    const char *csv_file;

    uint64_t *send_ts_ns;
    uint64_t *recv_ts_ns;
    int      *is_missing;

    unsigned char packet[MAX_ETH_FRAME_SIZE];
} receiver_data_t;

static int
init_receiver(int argc, char *argv[], receiver_data_t *data)
{
    if (argc < 3 || argc > 4) {
        printf("Usage: %s <interface> <expected_packets> [output_csv]\n", argv[0]);
        printf("Example: %s eth0 1000\n", argv[0]);
        printf("Example: %s eth0 1000 hot_results.csv\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];
    data->expected_packets = atoi(argv[2]);
    data->csv_file = (argc == 4) ? argv[3] : "latencies.csv";

    if (data->expected_packets <= 0) {
        fprintf(stderr, "expected_packets must be > 0\n");
        return EXIT_FAILURE;
    }

    data->sock = socket(AF_PACKET, SOCK_RAW, htons(SV_ETHERTYPE));
    if (data->sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (setsockopt(data->sock, SOL_SOCKET, SO_BINDTODEVICE,
                   interface, strlen(interface) + 1) < 0) {
        perror("setsockopt SO_BINDTODEVICE");
        close(data->sock);
        return EXIT_FAILURE;
    }

#ifdef PACKET_IGNORE_OUTGOING
    {
        int ignore_outgoing = 1;
        if (setsockopt(data->sock, SOL_PACKET, PACKET_IGNORE_OUTGOING,
                       &ignore_outgoing, sizeof(ignore_outgoing)) < 0) {
            perror("setsockopt PACKET_IGNORE_OUTGOING");
        } else {
            printf("[+] PACKET_IGNORE_OUTGOING set successfully\n");
        }
    }
#endif

    struct ifreq ifr = { 0 };
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(data->sock, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl SIOCGIFHWADDR");
        close(data->sock);
        return EXIT_FAILURE;
    }

    unsigned char *my_mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    printf("Listening on %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           interface,
           my_mac[0], my_mac[1], my_mac[2],
           my_mac[3], my_mac[4], my_mac[5]);

    data->send_ts_ns = malloc(sizeof(uint64_t) * (size_t)data->expected_packets);
    data->recv_ts_ns = malloc(sizeof(uint64_t) * (size_t)data->expected_packets);
    data->is_missing = malloc(sizeof(int) * (size_t)data->expected_packets);

    if (!data->send_ts_ns || !data->recv_ts_ns || !data->is_missing) {
        fprintf(stderr, "malloc failed\n");
        free(data->send_ts_ns);
        free(data->recv_ts_ns);
        free(data->is_missing);
        close(data->sock);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < data->expected_packets; ++i) {
        data->send_ts_ns[i] = 0;
        data->recv_ts_ns[i] = 0;
        data->is_missing[i] = 1;
    }

    return EXIT_SUCCESS;
}

static int
receive_packets(receiver_data_t *data, int *out_received_count)
{
    int packet_count = 0;

    while (packet_count < data->expected_packets) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(data->sock, &readfds);

        int nfds = (data->sock > STDIN_FILENO ? data->sock : STDIN_FILENO) + 1;
        int select_result = select(nfds, &readfds, NULL, NULL, NULL);

        if (select_result < 0) {
            perror("select");
            return -1;
        }

        // Check if stdin is ready (user pressed Enter)
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buf[16];
            (void)read(STDIN_FILENO, buf, sizeof(buf));
            printf("\nEarly termination detected. Saving received %d packets...\n",
                   packet_count);
            break;
        }

        if (!FD_ISSET(data->sock, &readfds)) {
            continue;
        }

        struct sockaddr_ll socket_address;
        socklen_t saddr_len = sizeof(socket_address);

        int packet_len = recvfrom(data->sock,
                                  data->packet,
                                  sizeof(data->packet),
                                  0,
                                  (struct sockaddr *)&socket_address,
                                  &saddr_len);
        if (packet_len < 0) {
            perror("recvfrom");
            return -1;
        }

        if (socket_address.sll_pkttype == PACKET_OUTGOING) {
            continue;
        }

        if (packet_len < (int)(sizeof(struct ether_header) + sizeof(sv_payload_t))) {
            fprintf(stderr, "Warning: packet too short for sv_payload_t\n");
            continue;
        }

        struct ether_header *eh = (struct ether_header *)data->packet;
        if (ntohs(eh->ether_type) != SV_ETHERTYPE) {
            continue;
        }

        sv_payload_t *payload =
            (sv_payload_t *)(data->packet + sizeof(struct ether_header));

        if (payload->frame_id >= (uint32_t)data->expected_packets) {
            fprintf(stderr,
                    "Warning: received frame_id %u exceeds expected_packets %d\n",
                    payload->frame_id, data->expected_packets);
            continue;
        }

        struct timespec ts_now;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) != 0) {
            perror("clock_gettime");
            return -1;
        }

        uint64_t now_ns =
            (uint64_t)ts_now.tv_sec * 1000000000ULL + (uint64_t)ts_now.tv_nsec;

        uint32_t frame_id = payload->frame_id;

        if (data->is_missing[frame_id] == 0) {
            continue;
        }

        data->send_ts_ns[frame_id] = payload->sender_timestamp_ns;
        data->recv_ts_ns[frame_id] = now_ns;
        data->is_missing[frame_id] = 0;
        packet_count++;

        /*
        if (packet_count % 1000 == 0) {
            printf("Received %d packets (last frame_id: %u)\n",packet_count, frame_id);
        }
        */
    }

    *out_received_count = packet_count;
    return 0;
}

static void
write_results_and_print_stats(const receiver_data_t *data, int received_count)
{
    FILE *csv_fp = fopen(data->csv_file, "w");
    if (csv_fp) {
        fprintf(csv_fp,
                "frame_id,send_timestamp_ns,recv_timestamp_ns,latency_ns,is_missing\n");
    } else {
        fprintf(stderr, "Warning: could not open CSV file %s for writing\n",
                data->csv_file);
    }

    if (received_count == 0) {
        if (csv_fp) {
            fclose(csv_fp);
            printf("CSV data written to: %s\n", data->csv_file);
        }
        printf("\nNo packets received.\n");
        return;
    }

    uint64_t min_rtt_ns = UINT64_MAX;
    uint64_t max_rtt_ns = 0;
    long double sum_rtt_ns = 0.0L;
    int valid_count = 0;

    for (int i = 0; i < data->expected_packets; ++i) {
        if (data->is_missing[i]) {
            if (csv_fp) {
                fprintf(csv_fp, "%d,NaN,NaN,NaN,1\n", i);
            }
            continue;
        }

        uint64_t rtt_ns = data->recv_ts_ns[i] - data->send_ts_ns[i];

        if (rtt_ns < min_rtt_ns) {
            min_rtt_ns = rtt_ns;
        }
        if (rtt_ns > max_rtt_ns) {
            max_rtt_ns = rtt_ns;
        }
        sum_rtt_ns += (long double)rtt_ns;
        valid_count++;

        if (csv_fp) {
            fprintf(csv_fp,
                    "%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",0\n",
                    i,
                    data->send_ts_ns[i],
                    data->recv_ts_ns[i],
                    rtt_ns);
        }
    }

    if (csv_fp) {
        fclose(csv_fp);
        printf("CSV data written to: %s\n", data->csv_file);
    }

    if (valid_count > 0) {
        long double avg_rtt_ns = sum_rtt_ns / (long double)valid_count;
        long double avg_rtt_us = avg_rtt_ns / 1000.0L;

        printf("\nRTT stats: received=%d missing=%d total=%d\n",
               valid_count, data->expected_packets - valid_count, data->expected_packets);
        printf("  min=%.3Lf us avg=%.3Lf us max=%.3Lf us\n",
               (long double)min_rtt_ns / 1000.0L,
               avg_rtt_us,
               (long double)max_rtt_ns / 1000.0L);
    }
}

int
main(int argc, char *argv[])
{
    if (lock_process_memory() != 0) {
        return EXIT_FAILURE;
    }

    int ret;
    receiver_data_t data = { 0 };

    ret = init_receiver(argc, argv, &data);
    if (ret != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    printf("Waiting for %d hot packets (EtherType 0x%04x)...\n",
           data.expected_packets, SV_ETHERTYPE);
    printf("(Press Enter to stop early and save received packets)\n\n");
    printf("READY\n\n"); // for automation scripts to detect readiness

    int received_count = 0;
    if (receive_packets(&data, &received_count) != 0) {
        free(data.send_ts_ns);
        free(data.recv_ts_ns);
        free(data.is_missing);
        close(data.sock);
        return EXIT_FAILURE;
    }

    write_results_and_print_stats(&data, received_count);

    free(data.send_ts_ns);
    free(data.recv_ts_ns);
    free(data.is_missing);
    close(data.sock);

    return EXIT_SUCCESS;
}
