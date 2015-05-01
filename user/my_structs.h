//
//  my_structs.h
//  PA2_NETWORK
//
//  Created by Hojjat Aghakhani on 9/29/1393 AP.
//  Copyright (c) 1393 Hojjat Aghakhani. All rights reserved.
//

#ifndef PA2_NETWORK_my_structs_h
#define PA2_NETWORK_my_structs_h

#include <pthread.h>
struct ip_info{
    uint32_t uint;
    string str;
};

struct routing_info{
    ip_info ip_next_hop_rtt;
    ip_info ip_next_hop_lost;
    uint16_t rtt_time;
    double lost_rate;
    bool is_active;
};

struct server_information{
    ip_info ip;
    double ping_lost_rate;
    uint16_t ping_sent;
    uint16_t ping_received;
    uint16_t rtt_time;
    uint16_t sequence_id;
    uint16_t ping_lost_numbers;
    std::chrono::time_point<std::chrono::system_clock> ping_send_time;
    bool is_received_last_ping;// show that last ping request is successfully or not!
    
    routing_info forwarding_table;
    routing_info learning_table;
    pthread_mutex_t mutex;
};

struct icmp{// according to http://en.wikipedia.org/wiki/Internet_Control_Message_Protocol and requirements!
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t sequence_id;
    uint16_t identifier;
    uint32_t payload_data;
    
};

#endif
