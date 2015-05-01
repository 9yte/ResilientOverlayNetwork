//                   In the name of GOD
/**
 * Partov is a simulation engine, supporting emulation as well,
 * making it possible to create virtual networks.
 *
 * Copyright © 2009-2014 Behnam Momeni.
 *
 * This file is part of the Partov.
 *
 * Partov is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Partov is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Partov.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _S_M_H_
#define _S_M_H_

#include "machine.h"
#include <chrono>
using namespace std;

class SimulatedMachine : public Machine {
public:
    SimulatedMachine (const ClientFramework *cf, int count);
    virtual ~SimulatedMachine ();
    
    void mac_into_byte(string mac, byte mac_addr[6]);
    uint32_t ip_into_uint(string ip);
    string ipUint_into_str(uint32_t ip_uint);
    uint16_t check_sum_ip(uint16_t *ip_temp, int header_length);
    void send_icmp_packet(struct server_information *server_info);
    uint16_t check_sum_icmp(uint16_t *icmp_temp);
    void check_connections_is_alive();
    void set_ip_header(struct ip *ip, uint32_t dest_ip, uint16_t ip_protocol, uint32_t advertise_data_size, bool is_dsa_lsa, bool is_self);
    void set_icmp_header_in_ping_packet(struct icmp *icmp, uint16_t sequence_id);
    void update_server_information(server_information *server_info);
    void print_server_information(int id);
    void initialize_server_information(int id, string server_id);
    void update_learning_table(server_information server_info, struct ip_info ip);
    void receive_ping_packet(uint32_t ip, std::chrono::time_point<std::chrono::system_clock> ping_received_time, int sequence_id);
    void send_advertise_packet(uint32_t ip_dst, byte mac_dst[6], byte *data_in, uint32_t data_size);
    void advertise_data();
    void store_mac_addr_to_map(uint32_t ip, uint8_t mac_addr[6]);
    void fill_forwarding_table();
    void reset_learning_table();
    void print_delay_table();
    void print_loss_table();
    void receive_advertise_data(byte *advertise_data, int advertise_size, uint32_t ip_src, uint8_t *mac_shost);
    void send_dsa_packet(string ip_dst);
    void send_lsa_packet(string ip_dst);
    void send_udp_packet(uint32_t ip_dst, uint32_t next_hop, bool is_dsa);
    bool is_equal_mac(uint8_t *mac1, uint8_t *mac2);
    int get_index_by_ip(uint32_t ip);
    void insert_into_server_infoes(server_information *sv_in);
    bool is_valid_packet(Frame frame);
    bool is_server_valid(string server_ip);
    
    virtual void initialize ();
    virtual void run ();
    virtual void processFrame (Frame frame, int ifaceIndex);
    
    static void parseArguments (int argc, char *argv[]);
};

#endif /* sm.h */

