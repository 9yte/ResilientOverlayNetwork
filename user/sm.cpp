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

#include "sm.h"
#include <sstream>
#include "interface.h"
#include "frame.h"
#include "my_structs.h"
#include <vector>
#include <netinet/in.h>
#include <map>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <pthread.h>
#include "sr_protocol.h"
#include <arpa/inet.h>
#include <ctime>
#include <chrono>
#include <iomanip>

#define MAX_SERVER_NUMBERS 1000
#define ICMP_SIZE (sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct icmp))
#define UDP_SIZE (sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct sr_udp))

using namespace std;
using namespace chrono;

int number_of_servers;
int number_of_all_servers;
int src_port = 8000;

map <uint16_t, time_point<system_clock> > sent_times;

server_information server_infoes[MAX_SERVER_NUMBERS];

ip_info ip_gateway;
byte mac_gateway[6];

pthread_mutex_t port_for_nat_mutex;

map <uint32_t, int> ip_to_id;
map <uint32_t, uint8_t*> ip_to_mac;
map <uint16_t, uint32_t> port_to_ip;
map <uint16_t, uint16_t> my_port_to_src_port;


SimulatedMachine::SimulatedMachine (const ClientFramework *cf, int count) :
Machine (cf, count) {
    // The machine instantiated.
    // Interfaces are not valid at this point.
}

SimulatedMachine::~SimulatedMachine () {
    // destructor...
}

void SimulatedMachine::initialize () {
    // TODO: Initialize your program here; interfaces are valid now.
    port_for_nat_mutex = PTHREAD_MUTEX_INITIALIZER;
    string info = getCustomInformation();
    stringstream ss(info);
    string ip, macAddr, numbers;
    ss >> ip >> macAddr >> numbers;
    number_of_servers = atoi(numbers.c_str());
    number_of_all_servers = number_of_servers;
    ip_gateway.str = ip;
    ip_gateway.uint = ip_into_uint(ip);
    mac_into_byte(macAddr, mac_gateway);
    
    string server_temp;
    for (int i = 0; i < number_of_servers; i++) {
        ss >> server_temp;
        initialize_server_information(i, server_temp);
    }
}

/**
 * This method is called from the main thread.
 * Also ownership of the data of the frame is not with you.
 * If you need it, make a copy for yourself.
 *
 * You can also send frames using:
 * <code>
 *     bool synchronized sendFrame (Frame frame, int ifaceIndex) const;
 * </code>
 * which accepts one frame and the interface index (counting from 0) and
 * sends the frame on that interface.
 * The Frame class used here, encapsulates any kind of network frame.
 * <code>
 *     class Frame {
 *     public:
 *       uint32 length;
 *       byte *data;
 *
 *       Frame (uint32 _length, byte *_data);
 *       virtual ~Frame ();
 *     };
 * </code>
 */
void SimulatedMachine::processFrame (Frame frame, int ifaceIndex) {
    // TODO: process the raw frame; frame.data points to the frame's byte stream
    time_point<system_clock> ping_received_time = system_clock::now();
    cerr << "Frame received at iface " << ifaceIndex << " with length " << frame.length << endl;
    
    if (!is_valid_packet(frame)){
        return;
    }
    
    byte *frame_data = new byte[frame.length];
    memcpy(frame_data, frame.data, frame.length);
    
    struct sr_ethernet_hdr *eth = (struct sr_ethernet_hdr*)frame_data;
    struct ip *ip = (struct ip*)(frame_data + sizeof(struct sr_ethernet_hdr));
    
    uint8_t protocol = ip->ip_p;
    if (protocol == IPPROTO_ICMP) {
        struct icmp *i = (struct icmp*)(frame_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
        receive_ping_packet(ntohl(ip->ip_src.s_addr), ping_received_time,ntohs(i->sequence_id));
    }
    else if (protocol == IPPROTO_UDP) {
        struct sr_udp *udp = (struct sr_udp*)(frame_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
        int advertise_size = frame.length - UDP_SIZE;
        byte *advertise_data = frame_data + UDP_SIZE;
        
        uint16_t port = ntohs(udp->port_dst);
        if (port == 5000) { // if packet is advertisment of another node in network
            receive_advertise_data(advertise_data, advertise_size, ntohl(ip->ip_src.s_addr), eth->ether_shost);
        }
        else if (port == 1000 || port == 2000){
            int length = frame.length;
            byte *new_data = new byte[length];
            struct sr_udp *udp1 = (struct sr_udp*)(new_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
            struct sr_ethernet_hdr *eth1 = (struct sr_ethernet_hdr*)new_data;
            struct ip *ip1 = (struct ip*)(new_data + sizeof(struct sr_ethernet_hdr));
            
            memcpy(new_data, frame_data, length);
            memcpy(eth1->ether_shost, iface[1].mac, Interface::MAC_ADDRESS_LENGTH);
            memcpy(eth1->ether_dhost, mac_gateway, Interface::MAC_ADDRESS_LENGTH);
            
            pthread_mutex_lock(&port_for_nat_mutex);
            port_to_ip[src_port] = (ip1->ip_src.s_addr);
            my_port_to_src_port[src_port] = (udp1 -> port_src);
            udp1->port_src = htons(src_port);
            src_port++;
            pthread_mutex_unlock(&port_for_nat_mutex);
            
            ip1->ip_src.s_addr = htonl(iface[1].getIp());
            ip1->ip_sum = 0;
            ip1->ip_sum = check_sum_ip((uint16_t *)ip1, ip1->ip_hl);
            sendFrame(*new Frame(length, new_data), 1);
            if (port == 1000) {// if this is a dsa packet
                cout << "DSA packet forwarded to " << ipUint_into_str(ntohl(ip->ip_dst.s_addr)) << endl;
            }
            else if (port == 2000) {// if this is a lsa packet
                cout << "LSA packet forwarded to " << ipUint_into_str(ntohl(ip->ip_dst.s_addr)) << endl;
            }
        }
        else if(port >= 8000 && port < src_port){
            if (port_to_ip.count(ntohs(udp->port_dst)) == 1){
                uint16_t main_port = ntohs(my_port_to_src_port[port]);
                uint32_t main_ip = ntohl(port_to_ip[port]);
                
                int length = frame.length;
                byte *new_data = new byte[length];
                memcpy(new_data, frame_data, length);
                struct sr_udp *udp1 = (struct sr_udp*)(new_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
                struct sr_ethernet_hdr *eth1 = (struct sr_ethernet_hdr*)new_data;
                struct ip *ip1 = (struct ip*)(new_data + sizeof(struct sr_ethernet_hdr));
                memcpy(eth1->ether_shost, iface[0].mac, Interface::MAC_ADDRESS_LENGTH);
                memcpy(eth1->ether_dhost, ip_to_mac[main_ip], Interface::MAC_ADDRESS_LENGTH);
                udp1->port_dst = htons(main_port);
                ip1->ip_dst.s_addr = htonl(main_ip);
                ip1->ip_sum = 0;
                ip1->ip_sum = check_sum_ip((uint16_t *)ip1, ip1->ip_hl);
                sendFrame(*new Frame(length, new_data), 0);
                
                uint16_t port = ntohs(udp -> port_src);
                if(port == 1000){
                    cout << "DSA forwarded packet reply received from " << ipUint_into_str(ntohl(ip->ip_src.s_addr))<< endl;
                }
                else if(port == 2000){
                    cout << "LSA forwarded packet reply received from " << ipUint_into_str(ntohl(ip->ip_src.s_addr))<< endl;
                }
            }
            else{
                uint16_t port_dst = ntohs(udp -> port_dst);
                time_point<system_clock> received_time = system_clock::now();
                uint16_t time = duration_cast<milliseconds> (received_time - sent_times[port_dst]).count();
                
                uint16_t port = ntohs(udp -> port_src);
                if(port == 1000){
                    cout << "DSA packet " << port_dst << " reply received in " << time << "ms" << endl;
                }
                else if(port == 2000){
                    cout << "LSA packet " << port_dst << " reply received in " << time << "ms" << endl;
                }
            }
        }
    }
}

/**
 * This method will be run from an independent thread. Use it if needed or simply return.
 * Returning from this method will not finish the execution of the program.
 */
void SimulatedMachine::run () {
    string command;
    
    while(true){
        cin >> command;
        if(command == "ping"){
            check_connections_is_alive();
            for(int i = 0; i < number_of_servers; i++){
                send_icmp_packet(&server_infoes[i]);
            }
        }
        else if(command == "stats"){
            for (int i = 0; i < number_of_servers;  i++) {
                print_server_information(i);
            }
        }
        else if(command == "advertise"){
            advertise_data();
            fill_forwarding_table();
            reset_learning_table();
        }
        else if(command == "dtable"){
            print_delay_table();
        }
        else if(command == "ltable"){
            print_loss_table();
        }
        else if(command == "dsa"){
            string ip_dst;
            cin >> ip_dst;
            if (is_server_valid(ip_dst))
                send_dsa_packet(ip_dst);
        }
        else if(command == "lsa"){
            string ip_dst;
            cin >> ip_dst;
            if (is_server_valid(ip_dst))
                send_lsa_packet(ip_dst);
        }
    }
}

/**
 * You could ignore this method if you are not interested on custom arguments.
 */
void SimulatedMachine::parseArguments (int argc, char *argv[]) {
    // TODO: parse arguments which are passed via --args
}

bool SimulatedMachine::is_server_valid(string server_ip) {
    for (int i = 0; i < number_of_all_servers; i++) {
        if (server_infoes[i].ip.str == server_ip)
            return true;
    }
    return false;
}

bool SimulatedMachine::is_valid_packet(Frame frame) {
    byte *frame_data = frame.data;
    int length = frame.length;
    if (length < sizeof(struct sr_ethernet_hdr))
        return false;
    struct sr_ethernet_hdr *eth = (struct sr_ethernet_hdr*)frame_data;
    if (ntohs(eth->ether_type) != ETHERTYPE_IP)
        return false;
    if (length < sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_ethernet_hdr))
        return false;
    struct ip *ip = (struct ip*)(frame_data + sizeof(struct sr_ethernet_hdr));
    if (length != ntohs(ip->ip_len) + sizeof(struct sr_ethernet_hdr))
        return false;
    if (ip->ip_v != 4)
        return false;
    if (check_sum_ip((uint16_t *) ip, ip->ip_hl) != 0)
        return false;
    if (!(ip->ip_p == IPPROTO_UDP || ip->ip_p == IPPROTO_ICMP))
        return false;
    if (ip->ip_p == IPPROTO_UDP){
        if(length < UDP_SIZE)
            return false;
        struct sr_udp *udp = (struct sr_udp*)(frame_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
        if (ntohs(udp->length) != ntohs(ip->ip_len) - 4 * (ip->ip_hl))
            return false;
    }
    else if (ip->ip_p == IPPROTO_ICMP){
        if(length < ICMP_SIZE)
            return false;
        struct icmp *i = (struct icmp*)(frame_data + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
        if (i->type != 0 || i->code != 0 || ntohl(i->payload_data) != 0x12345678)
            return false;
    }
    return true;
}

/**
 * check that both mac address given is equal or not equal!
 */
bool SimulatedMachine::is_equal_mac(uint8_t *mac1, uint8_t *mac2) {
    for (int i = 0; i < 6; i++) {
        if(*(mac1 + i) != *(mac2 + i))
            return false;
    }
    return true;
}

/**
 * this method print routing information based on minimum delay(RTT)!
 */
void SimulatedMachine::print_delay_table() {
    for (int i = 0; i < number_of_all_servers; i++) {
        cout << server_infoes[i].ip.str << " " << server_infoes[i].forwarding_table.ip_next_hop_rtt.str << " " << server_infoes[i].learning_table.ip_next_hop_rtt.str << " ";
        if (server_infoes[i].learning_table.rtt_time == numeric_limits<uint16_t>::max())
            cout << "INF" << endl;
        else
            cout << setprecision (2) << fixed << server_infoes[i].learning_table.rtt_time << endl;
    }
}

/**
 * this method print routing information based on minimum loss rate(loss)!
 */
void SimulatedMachine::print_loss_table() {
    for (int i = 0; i < number_of_all_servers; i++) {
        cout << server_infoes[i].ip.str << " " << server_infoes[i].forwarding_table.ip_next_hop_lost.str << " " << server_infoes[i].learning_table.ip_next_hop_lost.str << " ";
        if (server_infoes[i].learning_table.lost_rate == 1)
            cout << "1.00" << endl;
        else
            cout << server_infoes[i].learning_table.lost_rate << endl;
    }
}

/**
 * this method copy learning table into forwarding table!
 */
void SimulatedMachine::fill_forwarding_table() {
    for (int i = 0; i < number_of_all_servers; i++) {
        server_infoes[i].forwarding_table.ip_next_hop_lost.uint = server_infoes[i].learning_table.ip_next_hop_lost.uint;
        server_infoes[i].forwarding_table.ip_next_hop_lost.str = server_infoes[i].learning_table.ip_next_hop_lost.str;
        server_infoes[i].forwarding_table.ip_next_hop_rtt.uint = server_infoes[i].learning_table.ip_next_hop_rtt.uint;
        server_infoes[i].forwarding_table.ip_next_hop_rtt.str = server_infoes[i].learning_table.ip_next_hop_rtt.str;
    }
}

/**
 * this method reset learning_table to host own (through gateway) paths again!
 */
void SimulatedMachine::reset_learning_table() {
    for (int i = 0; i < number_of_all_servers; i++) {
        server_infoes[i].learning_table.ip_next_hop_lost.uint = ip_gateway.uint;
        server_infoes[i].learning_table.ip_next_hop_lost.str = ip_gateway.str;
        server_infoes[i].learning_table.rtt_time = server_infoes[i].rtt_time;
        server_infoes[i].learning_table.ip_next_hop_rtt.uint = ip_gateway.uint;
        server_infoes[i].learning_table.ip_next_hop_rtt.str = ip_gateway.str;
        server_infoes[i].learning_table.lost_rate = server_infoes[i].ping_lost_rate;
    }
}

/**
 * this method store mac address into ip_to_mac!
 */
void SimulatedMachine::store_mac_addr_to_map(uint32_t ip, uint8_t *mac_addr){
    ip_to_mac[ip] = new uint8_t[6];
    for (int i = 0; i < 6; i++) {
        memcpy(ip_to_mac[ip] + i, (mac_addr + i), 1);
    }
}

/**
 * this method advertise the data to other nodes in network!
 */
void SimulatedMachine::advertise_data() {
    byte *advertise_data =  new byte[12 * number_of_servers + 4];
    int offset = 0;
    for (int i = 0; i < number_of_servers; i++) {
        if(server_infoes[i].rtt_time != numeric_limits<uint16_t>::max()){
            uint32_t ip_uint = htonl(server_infoes[i].ip.uint);
            uint32_t rtt_time = htonl(server_infoes[i].rtt_time);
            uint16_t ping_sent = htons(server_infoes[i].ping_sent);
            uint16_t ping_receive = htons(server_infoes[i].ping_received);
            memcpy(advertise_data + offset, &ip_uint , 4);
            memcpy(advertise_data + offset + 4, &rtt_time , 4);
            memcpy(advertise_data + offset + 8, &ping_sent , 2);
            memcpy(advertise_data + offset + 10, &ping_receive, 2);
            offset += 12;
        }
        
    }
    uint32_t padding = 0;
    memcpy(advertise_data + offset, &padding, sizeof(padding));
    offset += 4;
    
    byte mac_broadcast[6];
    mac_into_byte("FF:FF:FF:FF:FF:FF", mac_broadcast);
    uint32_t ip_broadcast = ip_into_uint("255.255.255.255");
    
    send_advertise_packet(ip_broadcast, mac_broadcast, advertise_data, offset);
}


/**
 * we call this method when a ping request received successfully and we update server information
 */
void SimulatedMachine::receive_ping_packet(uint32_t ip, time_point<system_clock> ping_received_time, int sequence_id){
    for (int i = 0; i < number_of_servers; i++) {
        if (server_infoes[i].ip.uint == ip && sequence_id + 1 == server_infoes[i].sequence_id) {
            uint16_t ping_time = duration_cast<milliseconds> (ping_received_time - server_infoes[i].ping_send_time).count();
            if (server_infoes[i].rtt_time == numeric_limits<uint16_t>::max()) {// if this is the first ping request after the connection established again!!!
                server_infoes[i].rtt_time = ping_time;
                server_infoes[i].ping_sent++;
            }
            else{
                server_infoes[i].rtt_time = (server_infoes[i].rtt_time + ping_time)/2;
            }
            server_infoes[i].ping_received ++;
            server_infoes[i].is_received_last_ping = true;
            server_infoes[i].ping_lost_numbers = 0;
            server_infoes[i].ping_lost_rate = 1 - (double)(((double)(server_infoes[i].ping_received))/((double)server_infoes[i].ping_sent));
            update_learning_table(server_infoes[i], ip_gateway);
            
        }
    }
}

/**
 * this method receive advertise data from another node and updates itself information!
 */
void SimulatedMachine::receive_advertise_data(byte *advertise_data, int advertise_size, uint32_t ip_src, uint8_t *mac_shost) {
    for (int i = 0; i < advertise_size - 4; i += 12) {
        
        double lost_rate_to_server;
        uint32_t ip_server;
        uint32_t rtt_to_server;
        uint16_t sent_to_server;
        uint16_t received_to_server;
        
        memcpy(&ip_server, advertise_data + i, 4);
        memcpy(&rtt_to_server, advertise_data + i + 4, 4);
        memcpy(&sent_to_server, advertise_data + i + 8, 2);
        memcpy(&received_to_server, advertise_data + i + 10, 2);
        lost_rate_to_server = 1 - (double)((double)received_to_server/(double)sent_to_server);
        
        server_information server_info;
        server_info.rtt_time = ntohl(rtt_to_server);
        server_info.ping_lost_rate =lost_rate_to_server;
        server_info.ping_sent = ntohs(sent_to_server);
        server_info.ping_received = ntohs(received_to_server);
        server_info.ip.uint = ntohl(ip_server);
        server_info.ip.str = ipUint_into_str(ntohl(ip_server));
        
        struct ip_info host_node_ip;
        host_node_ip.uint = ip_src;
        host_node_ip.str = ipUint_into_str(ip_src);
        
        update_learning_table(server_info, host_node_ip);
        
    }
    store_mac_addr_to_map(ip_src, mac_shost);
}

int SimulatedMachine::get_index_by_ip(uint32_t ip) {
    for (int i = 0; i < number_of_all_servers; i++) {
        if(server_infoes[i].ip.uint == ip){
            return i;
        }
    }
    return -1;
}

void SimulatedMachine::insert_into_server_infoes(server_information *sv_in) {
    server_infoes[number_of_all_servers].ip.uint = sv_in->ip.uint;
    server_infoes[number_of_all_servers].ip.str = sv_in->ip.str;
    server_infoes[number_of_all_servers].is_received_last_ping = false;
    server_infoes[number_of_all_servers].ping_lost_rate = 1;
    server_infoes[number_of_all_servers].learning_table.lost_rate = 1;
    server_infoes[number_of_all_servers].rtt_time = numeric_limits<uint16_t>::max();
    server_infoes[number_of_all_servers].learning_table.rtt_time = numeric_limits<uint16_t>::max();
    number_of_all_servers++;
}

/**
 *  this method update struct learning_table of struct server_information!
 */
void SimulatedMachine::update_learning_table(server_information server_info, struct ip_info ip){
    pthread_mutex_lock(& server_info.mutex);
    int server_id = get_index_by_ip(server_info.ip.uint);
    if(server_id == -1){
        server_id = number_of_all_servers;
        insert_into_server_infoes(&server_info);
    }
    server_infoes[server_id].learning_table.is_active = true;
    if(ip.uint == server_infoes[server_id].learning_table.ip_next_hop_rtt.uint | server_infoes[server_id].learning_table.rtt_time > server_info.rtt_time){
        server_infoes[server_id].learning_table.ip_next_hop_rtt.uint = ip.uint;
        server_infoes[server_id].learning_table.ip_next_hop_rtt.str = ip.str;
        server_infoes[server_id].learning_table.rtt_time = server_info.rtt_time;
    }
    if(ip.uint == server_infoes[server_id].learning_table.ip_next_hop_lost.uint | server_infoes[server_id].learning_table.lost_rate > server_info.ping_lost_rate){
        server_infoes[server_id].learning_table.ip_next_hop_lost.uint = ip.uint;
        server_infoes[server_id].learning_table.ip_next_hop_lost.str = ip.str;
        server_infoes[server_id].learning_table.lost_rate = server_info.ping_lost_rate;
    }
    pthread_mutex_unlock(& server_info.mutex);
}

/**
 * this method check that connection to servers is active or not, when three consecutive ping is fault, it means the conncetion to the server has been down...
 */
void SimulatedMachine::check_connections_is_alive(){
    for (int i = 0; i < number_of_servers; i++){
        if (!server_infoes[i].is_received_last_ping) {
            if (server_infoes[i].ping_lost_numbers == 2) {// the connection is down!
                server_infoes[i].ping_lost_rate = 1;
                server_infoes[i].rtt_time = numeric_limits<uint16_t>::max();
            }
            server_infoes[i].ping_lost_numbers++;
        }
    }
}

/**
 * this method prints informations of server[id]
 */
void SimulatedMachine::print_server_information(int id) {
    cout << server_infoes[id].ip.str << " ";
    if (server_infoes[id].rtt_time == numeric_limits<uint16_t>::max()) {
        cout << "INF 1.00" << endl;
    }
    else{
        cout << (int)server_infoes[id].rtt_time << " ";
        cout << setprecision (2) << fixed << server_infoes[id].ping_lost_rate;
        cout << " (" << server_infoes[id].ping_sent << " " << server_infoes[id].ping_received << ")" << endl;
    }
}

void SimulatedMachine::initialize_server_information(int id, string server_id){
    routing_info r;
    r.ip_next_hop_rtt.uint = ip_gateway.uint;
    r.ip_next_hop_rtt.str = ip_gateway.str;
    r.ip_next_hop_lost.uint = ip_gateway.uint;
    r.ip_next_hop_lost.str = ip_gateway.str;
    r.rtt_time = numeric_limits<uint16_t>::max();
    r.lost_rate = 1;
    r.is_active = false;
    server_infoes[id].mutex = PTHREAD_MUTEX_INITIALIZER;
    server_infoes[id].forwarding_table = r;
    server_infoes[id].learning_table = r;
    server_infoes[id].ip.uint = ip_into_uint(server_id);
    ip_to_id[server_infoes[id].ip.uint] = id;
    server_infoes[id].ip.str = server_id;
    server_infoes[id].ping_lost_rate = 1;
    server_infoes[id].ping_received = 0;
    server_infoes[id].ping_sent = 0;
    server_infoes[id].rtt_time = numeric_limits<uint16_t>::max();
    server_infoes[id].sequence_id = 0;
    server_infoes[id].ping_lost_numbers = 0;
}

void SimulatedMachine::mac_into_byte(string mac, byte mac_addr[6]) {
    sscanf(mac.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X", &mac_addr[0], &mac_addr[1], &mac_addr[2], &mac_addr[3], &mac_addr[4], &mac_addr[5]);
}


/**
 * This method set checksum field in struct ip!
 */
uint16_t SimulatedMachine::check_sum_ip(uint16_t *ip_temp, int header_length) {
    uint32_t sum = 0;
    int length = header_length * 2;
    for (int i = 0; i < length; i++) {
        sum += ip_temp[i];
    }
    uint16_t reminder = sum % (1 << 16);// calculating carry
    uint16_t q = sum / (1 << 16);
    return ((q + reminder) ^ (0XFFFF));// adding carry to sum
}

/**
 * This method set checksum field in struct icmp!
 */
uint16_t SimulatedMachine::check_sum_icmp(uint16_t *temp) {// copy from net! :D
    
    uint16_t* buf = temp;
    uint16_t nbytes = 12;
    uint32_t sum = 0;
    for (; nbytes > 1; nbytes -= 2)
        sum += *buf++;
    
    if (nbytes == 1)
        sum += *(unsigned char*) buf;
    
    sum  = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return ~sum;
}

/**
 * this method set icmp header of ping request packet!
 */
void SimulatedMachine::set_ip_header(struct ip *ip, uint32_t dest_ip, uint16_t ip_protocol, uint32_t advertise_data_size, bool is_dsa_lsa, bool is_self) {
    ip->ip_v = 4;//version
    ip->ip_hl = 5;//header length
    ip->ip_tos = 0;
    ip->ip_id = htons(0);
    ip->ip_off = htons(0);
    ip->ip_ttl = 64;
    ip->ip_p = (ip_protocol);
    ip->ip_dst.s_addr = htonl(dest_ip);
    if(is_dsa_lsa) {
        if(is_self)
            ip->ip_src.s_addr = htonl(iface[1].getIp());
        else{
            ip->ip_src.s_addr = htonl(iface[0].getIp());
        }
        ip->ip_len = htons(UDP_SIZE - sizeof(struct sr_ethernet_hdr) + advertise_data_size);
    }
    else if(ip_protocol == IPPROTO_ICMP){
        ip->ip_src.s_addr = htonl(iface[1].getIp());
        ip->ip_len = htons(sizeof(struct ip) + sizeof(struct icmp));
    }
    else if(ip_protocol == IPPROTO_UDP){
        ip->ip_src.s_addr = htonl(iface[0].getIp());
        ip->ip_len = htons(UDP_SIZE - sizeof(struct sr_ethernet_hdr) + advertise_data_size);
    }
    else
        cerr << "error in ip layer!" << endl;
    ip->ip_sum = 0;
    ip->ip_sum = check_sum_ip((uint16_t *)ip, ip->ip_hl);
}

/**
 * this method set ip header of ping request packet!
 */
void SimulatedMachine::set_icmp_header_in_ping_packet(struct icmp *icmp, uint16_t sequence_id) {
    icmp->type = 8;
    icmp->code = 0;
    icmp->identifier = 0;
    icmp->sequence_id = htons(sequence_id);
    icmp->payload_data = htonl(0x12345678);
    icmp->checksum = 0;
    icmp->checksum = check_sum_icmp((uint16_t *)icmp);
}

/**
 * this method update informations of server that we ping it!!!
 */
void SimulatedMachine::update_server_information(server_information *server_info){
    server_info->sequence_id++;
    server_info->ping_send_time = system_clock::now();
    if(server_info->rtt_time != numeric_limits<uint16_t>::max()){
        server_info->ping_sent++;
        server_info->ping_lost_rate = 1 - (double)((double)server_info->ping_received/(double)server_info->ping_sent);
    }
    if(server_info->learning_table.ip_next_hop_lost.uint == ip_gateway.uint)
        server_info->learning_table.lost_rate = server_info->ping_lost_rate;
    server_info->is_received_last_ping = false;
}

void SimulatedMachine::send_icmp_packet(struct server_information *server_info){
    
    byte *data = new byte [ICMP_SIZE];
    uint8_t offset = 0;
    
    // ethernet layer
    struct sr_ethernet_hdr *eth = (struct sr_ethernet_hdr*)(data + offset);
    memcpy(eth->ether_shost, iface[1].mac, Interface::MAC_ADDRESS_LENGTH);
    memcpy(eth->ether_dhost, mac_gateway, Interface::MAC_ADDRESS_LENGTH);
    eth->ether_type = htons(ETHERTYPE_IP);
    
    // ip layer
    offset += sizeof(struct sr_ethernet_hdr);
    struct ip *ip = (struct ip*)(data + offset);
    set_ip_header(ip, server_info->ip.uint, IPPROTO_ICMP, 0, false, false);
    
    // icmp layer
    offset += sizeof(struct ip);
    struct icmp *icmp = (struct icmp*)(data + offset);
    set_icmp_header_in_ping_packet(icmp, server_info->sequence_id);
    
    // updating server_info struct, i.e increasing sequence_id, numeber of ping request sent and ...
    update_server_information(server_info);
    
    // creating frame
    Frame frame (ICMP_SIZE, data);
    //sending frame
    sendFrame(frame, 1);
}

void SimulatedMachine::send_advertise_packet(uint32_t ip_dst, byte mac_dst[6], byte *advertise_data, uint32_t advertise_data_size){
    
    byte *frame_data = new byte[UDP_SIZE + advertise_data_size];
    int offset = 0;
    
    // ethernet layer
    struct sr_ethernet_hdr *eth = (struct sr_ethernet_hdr* )(frame_data + offset);
    eth->ether_type = htons(ETHERTYPE_IP);
    memcpy(eth->ether_dhost, mac_dst , Interface::MAC_ADDRESS_LENGTH);
    memcpy(eth->ether_shost, iface[0].mac, Interface::MAC_ADDRESS_LENGTH);
    
    // ip layer
    offset += sizeof(struct sr_ethernet_hdr);
    struct ip *ip = (struct ip*) (frame_data + offset);
    set_ip_header(ip, ip_dst, IPPROTO_UDP, advertise_data_size, false, false);
    
    // udp layer
    offset += sizeof(struct ip);
    struct sr_udp *udp = (struct sr_udp*) (frame_data + offset);
    udp->length = htons(sizeof(struct sr_udp) + advertise_data_size);
    udp->port_dst = htons(5000);
    udp->port_src = htons(5000);
    udp->udp_sum = 0;
    
    // set advertise data!
    memcpy(frame_data + UDP_SIZE, advertise_data, advertise_data_size);
    
    Frame frame (advertise_data_size + UDP_SIZE, frame_data);
    sendFrame(frame, 0);
}

void SimulatedMachine::send_lsa_packet(string ip_dst) {
    int server_id = ip_to_id[ip_into_uint(ip_dst)];
    string next_hop_ip_str = server_infoes[server_id].forwarding_table.ip_next_hop_lost.str;
    uint32_t next_hop_ip_uint = server_infoes[server_id].forwarding_table.ip_next_hop_lost.uint;
    send_udp_packet(ip_into_uint(ip_dst), next_hop_ip_uint, false);
    cout << "LSA packet " << (src_port - 1) << " destined for " << ip_dst << " sent to " << next_hop_ip_str << endl;
}

void SimulatedMachine::send_dsa_packet(string ip_dst) {
    int server_id = ip_to_id[ip_into_uint(ip_dst)];
    string next_hop_ip_str = server_infoes[server_id].forwarding_table.ip_next_hop_rtt.str;
    uint32_t next_hop_ip_uint = server_infoes[server_id].forwarding_table.ip_next_hop_rtt.uint;
    send_udp_packet(ip_into_uint(ip_dst), next_hop_ip_uint, true);
    cout << "DSA packet " << (src_port - 1) << " destined for " << ip_dst << " sent to " << next_hop_ip_str << endl;
}

void SimulatedMachine::send_udp_packet(uint32_t ip_dst, uint32_t next_hop, bool is_dsa){
    byte *frame_data = new byte[UDP_SIZE + 4];
    int offset = 0;
    // ethernet layer
    struct sr_ethernet_hdr *eth = (struct sr_ethernet_hdr* )(frame_data + offset);
    eth->ether_type = htons(ETHERTYPE_IP);
    
    // ip layer
    offset += sizeof(struct sr_ethernet_hdr);
    struct ip *ip = (struct ip*) (frame_data + offset);
    if (next_hop == ip_gateway.uint)
        set_ip_header(ip, ip_dst, IPPROTO_UDP, 4, true, true);
    else
        set_ip_header(ip, ip_dst, IPPROTO_UDP, 4, true, false);
    // udp layer
    offset += sizeof(struct ip);
    struct sr_udp *udp = (struct sr_udp*) (frame_data + offset);
    udp->length = htons(sizeof(struct sr_udp) + 4);
    if(is_dsa)
        udp->port_dst = htons(1000);
    else
        udp->port_dst = htons(2000);
    pthread_mutex_lock(&port_for_nat_mutex);
    udp->port_src = htons(src_port);
    udp->udp_sum = 0;
    src_port++;
    pthread_mutex_unlock(&port_for_nat_mutex);
    
    // set payload data!
    uint32_t payload_data = htonl(0x12345678);
    memcpy(frame_data + UDP_SIZE, &payload_data, 4);
    
    if(next_hop == ip_gateway.uint){// send to gateway
        // ethernet layer
        memcpy(eth->ether_shost, iface[1].mac , Interface::MAC_ADDRESS_LENGTH);
        memcpy(eth->ether_dhost, mac_gateway, Interface::MAC_ADDRESS_LENGTH);
        Frame frame (4 + UDP_SIZE, frame_data);
        sendFrame(frame, 1);
    }
    else{// send to peer
        memcpy(eth->ether_shost, iface[0].mac , Interface::MAC_ADDRESS_LENGTH);
        memcpy(eth->ether_dhost, ip_to_mac[next_hop], Interface::MAC_ADDRESS_LENGTH);
        Frame frame (4 + UDP_SIZE, frame_data);
        sendFrame(frame, 0);
    }
    sent_times[src_port - 1] = system_clock::now();
}

/**
 * this method convert ip string to uint ip!
 */
uint32_t SimulatedMachine::ip_into_uint(string ip_str) {
    uint32_t sum = 0;
    uint32_t part = 0;
    size_t length = ip_str.size();
    for(int i = 0; i < length; i++){
        if(ip_str[i] == '.'){
            sum += part;
            part = 0;
            sum *= 256;
        }
        else{
            part *= 10;
            part += ip_str[i]-'0';
        }
    }
    return (sum + part);
}

/**
 * this method convert ip uint to string ip!
 */
string SimulatedMachine::ipUint_into_str(uint32_t ip_uint) {
    string ip_str[4];
    int i = 3;
    while(ip_uint != 0){
        uint32_t part = ip_uint % 256;
        ip_str[i] = "";
        do{
            ip_str[i] = char('0' + part % 10) + ip_str[i];
            part /= 10;
        }while(part != 0);
        ip_uint /= 256;
        i--;
    }
    return ip_str[0] + "." + ip_str[1] + "." + ip_str[2] + "." + ip_str[3];
}
