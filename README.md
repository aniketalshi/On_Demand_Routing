Network programming Assignment 3:
=================================================

Team members :
        1. Aniket Alshi    - 109114640
        2. Swapnil Dinkar  - 109157070

Implementation of On demand routing protocol 
==================================================
    
    To implement:

    1.  An On-Demand shortest-hop Routing (ODR) protocol for networks of fixed but arbitrary
        and unknown connectivity, using PF_PACKET sockets. The implementation is based on 
        (a simplified version of) the AODV algorithm.
        
    2.  Time client and server applications that send requests and replies to each other 
        across the network using ODR. An API you will implement using Unix domain datagram 
        sockets enables applications to communicate with the ODR mechanism running locally 
        at their nodes.

Instructions on running project
================================

    ./odr_and : Runs odr on a given node, accepts staleness as cmd line parameter but 
                assumes 5 secs as default if none provided by user.

    ./client_and : Runs client on a given node. Prompts user to enter name of machine
                   to request time from.

    ./server_and : Runs server on given node. 

Types of Packets
================

    1. __RREQ
            This packet is broadcast on the network when a new route is to be found.

    2. __RREP
            This packet is sent as a response to the __RREQ packet.

    3. __DATA
            This is the actual data packet that will be transimitted once a new route is
            discovered.


Routing Table Structure
=======================

    A sample routing table looks like this :
    ================================ ODR Routing Table ===================================================
    ---------------------------------------------------------------------------------------------------
    |  destination ip |             next hop |  ifno |  hops |  b_id |                      timestamp |
    ---------------------------------------------------------------------------------------------------
    |  130.245.156.27 |    00:0c:29:64:e3:de |     4 |     1 |    12 |     2014-11-24 07:21:12.131901 |
    |  130.245.156.22 |    00:0c:29:d9:08:f6 |     3 |     1 |    11 |     2014-11-24 07:21:12.124178 |
    ---------------------------------------------------------------------------------------------------

    Routing table stores :
    1. Destination canonical ip address.
    2. Mac address of next hop to sent the packet on.
    3. Interface index of our machine from where to send packet to.
    4. Num of hops to destination.
    5. Broadcast id.
    6. Timestamp of when entry was made to calcuate staleness.

    There are two apis implemented to communicate with peer process using UNIX Domain socket :
        1. msg_send () :  sends the message to ODR process.
           parameters  :  socket fd 
                          destination ip
                          destination port
                          payload
                          route discovery flag
                          sunpath file process is bound to
        
        2. msg_recv () :  Receiving message from ODR process.
           parameters  :  socket fd 
                          message buf
                          destination ip
                          destination port
                          message struct
    
    Policy to Update in Routing table:
        1. With every packet, we get we look in routing table for that src in packet
           if we can update an entry. If entry is with higher broadcast id or lower 
           hopcount, we update it.
        2. We check if entry has not become stale.
        3. If staleness param is set, we remove the entry for dest in table
        4. We also check if the 'Route Discovery Flag' is not set.
    

    ODR Header struct:
    =================
    typedef struct odr_frame_struct {
       int  frame_type;                  // type of the frame
       int  broadcast_id;                // broadcast id for rrep
       int  hop_count;                   // hop count to dest
       int  payload_len;                 // length of data payload
       int  route_disc_flag;             // Route discovery flag
       int  src_port;                    // source port no
       int  dst_port;                    // Dest port no
       char dest_ip [INET_ADDRSTRLEN];   // Dest canonical ip
       char src_ip  [INET_ADDRSTRLEN];   // source canonical ip
       char payload [PAYLOAD_SIZE];      // data payload
    } odr_frame_t;

    |<------14--------->|<----60----->|<--1440-->|
    ----------------------------------------------
    | Ethernet header   | ODR Header  | Payload  |
    ----------------------------------------------



