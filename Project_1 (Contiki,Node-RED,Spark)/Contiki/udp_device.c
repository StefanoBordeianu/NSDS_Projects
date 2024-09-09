#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "heapmem.h"
#include <stdlib.h>
#include "sys/log.h"
#include <stddef.h>

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define OP_DISCOVER 10
#define OP_DISCOVER_REPLY 11
#define OP_SEND_NEIGH 12
#define OP_GET_NEW_GROUP 13
#define OP_REMOVE_GROUP 14

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	5678
#define UDP_SERVER_PORT	5678

static char pub_topic[128];
static char client_id[128];
static char broker_ip[128];
static char broker_port[128];


struct neigh_node{
      void* next;
      uip_ipaddr_t addr;
};

struct group_node{
      void* next;
      struct neigh_node* list;
};

struct msg_template{
      int operation;
      int arr_len;
      uip_ipaddr_t data[20];
};

struct mqtt_register_msg{
      uip_ipaddr_t addr;
      int age;
      char nationality [20];
}

struct mqtt_cardinality{
      int array_len;
      uip_ipaddr_t group[20];
      int is_removing;
      uip_ipaddr_t addr;
}

struct mqtt_group{
      int array_len;
      uip_ipaddr_t group[20];
}

static uip_ipaddr_t self_ip;
static int numb_of_neighbors;
static struct simple_udp_connection udp_conn;
static struct ctimer neighbors_collection_timer;
static int neighbors_collection_timer_set;
static struct mqtt_connection conn;
clock_time_t mqtt_interval = 10*CLOCK_SECOND;
static int age;
static char* nationality;
static int registered;

static struct neigh_node* neighbors;
static struct group_node* groups;

#define DISCOVER_INTERVAL     (20 * CLOCK_SECOND)
#define WAIT_NEIGH_INTERVAL  (5 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
PROCESS(startup, "startup");
AUTOSTART_PROCESSES(&startup);

/*---------------------------------------------------------------------------*/
/* check if a node with the given address is present
 * uses uip_ipaddr_cmp() function provided by contiki to compare addresses   */
static int is_in_neighbors(uip_ipaddr_t* addr,struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* check if a node with the given address is already present, if not add a
 * new node with the given address                                           */
static void add_to_neighbors(uip_ipaddr_t* addr,struct neigh_node* list){}

/* check if a node with the given address is present, if it is remove the
 * node with the given address                                               */
static void remove_from_neighbor_list(uip_ipaddr_t* addr,struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* free every node of the list passed as parameter                   */
static void delete_list(struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* return the size of the list passed as parameter                   */
static int list_size(struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* returns a copy of the list passed                                         */
static struct neigh_node* cpy_list(struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* Add the group passed as parameter to the list of groups                   */
static void add_group(struct neigh_node* group){}

/*---------------------------------------------------------------------------*/
/* remove the group passed as parameter to the list of groups                   */
static void remove_group(struct group_node* group){}

/*---------------------------------------------------------------------------*/
/* returns true if a is a subset of b, false otherwise        */
static int is_subset(struct neigh_node* a,struct neigh_node* b){}

/*---------------------------------------------------------------------------*/
/* returns true if 'to_check' is a subset of any of the groups, false otherwise*/
static int is_subgroup(struct neigh_node* to_check){}

/*---------------------------------------------------------------------------*/
/* Check if an address in in a list                                          */
static int is_in_list(struct neigh_node* list,uip_ipaddr_t* addr){}

/*---------------------------------------------------------------------------*/
/* find group in the group local list                                        */
static struct group_node* find_group(struct neigh_node* tofind){}

/*---------------------------------------------------------------------------*/
/* Returns true if everyone from the group is missing from the neighbor list */
static int everyone_missing(struct group_node* group){}

/*---------------------------------------------------------------------------*/
/* serializes the lists given as first parameter to an array. 
 * Second parameter is a pointer that will be filled with the address of the array
 * Third parameter is a pointer that will be filled with the lenght of the array */
static void list_to_arr(struct neigh_node* list,uip_ipaddr_t* res, int* len){}

/*---------------------------------------------------------------------------*/
/* Takes an array and its size as parameters   
 * returns a list constructed with the addresses contained in the array*/
static struct neigh_node* array_to_list(uip_ipaddr_t* arr,int size){} 

/*---------------------------------------------------------------------------*/
/* Returns true the node if the address passed as first parameter is leader 
 * of the group passed as parameter. Leader is the node with bigger address. */
static int is_leader(uip_ipaddr_t* addr,struct neigh_node* list){}

/*---------------------------------------------------------------------------*/
/* Returns true if this node is leader of the group passed as parameter
 * Leader is the node with bigger address.                                   */
static int im_leader(struct neigh_node* list){
      return is_leader(self_ip, list);
}

/*---------------------------------------------------------------------------*/

static void udp_rx_callback(struct simple_udp_connection *c,
      const uip_ipaddr_t *sender_addr,
      uint16_t sender_port,
      const uip_ipaddr_t *receiver_addr,
      uint16_t receiver_port,
      const uint8_t *data,
      uint16_t datalen){

            static struct msg_template* msg;
            static struct msg_template msg_tosend;
            static int size_tosend;

            msg = (struct msg_template*)data;

            switch (msg->operation) {
                  case OP_DISCOVER:
                        /* We are in the case where a neighbor is checking for the surrounding nodes
                         * in this case we want to add the node to our neighbors and reply with our IP */
                         
                        add_to_neighbors(&msg->data[0]);
                        uip_ipaddr_copy(&msg_tosend.data[0], &msg->data[0]);
                        msg_tosend.arr_len = 1;
                        msg_tosend.operation = OP_DISCOVER_REPLY;
                        size_tosend = sizeof(struct msg_template);
                        simple_udp_sendto(&udp_conn, (char*)msg, size_tosend , sender_addr);
                        add_to_neighbors(msg->data);

                        break;

                  case OP_DISCOVER_REPLY:
                        /* We are in the case where a neighbor is responding to a discover request
                         * in this case we want to add the node to our neighbors and setup a timer,
                         * after the timer expires we will trigger send_neigh_list() 
                         * We use a callback timer for this */
                        add_to_neighbors(sender_addr);
                        if(!neighbors_collection_timer_set){
                              ctimer_set(&neighbors_collection_timer, WAIT_NEIGH_INTERVAL-CLOCK_SECOND+(random_rand()%(2*CLOCK_SECOND)),
                                    send_neigh_list, NULL);
                              neighbors_collection_timer_set = 1;
                        }
                        break;

                  case OP_SEND_NEIGH:
                        /* We are in the case where we have received a list of the neighbors of 
                         * another node. We need to check for groups to match or to create.
                         * */
                        match_groups(msg,sender_addr);
                        find_possible_groups(msg,sender_addr);
                        break;

                  case OP_GET_NEW_GROUP:
                        group_to_add = array_to_list(msg->data,msg->arr_len);
                        add_group(group_to_add);
                        break;

                  case OP_REMOVE_GROUP:
                        int deleted = 0;
                        group_to_find = array_to_list(&msg->data[1],msg->arr_len-1);
                        g = find_group(group_to_find);
                        if(g==NULL)
                              break;                                                //group was already deleted
                        remove_from_neighbor_list(g, msg->data);

                        
                        if(group_size(g)<2 || is_subgroup(g)){
                              if(im_leader(g)){
                                    add_to_neighbors(g->list,msg->data)               //add back the removed node to notify the backend
                                    notify_backend_group_deletion(g);
                              }
                              deleted = 1;
                              remove_group(g);
                        }
                        else{
                              if(im_leader(g))
                                    notify_backend_new_cardinality(g,msg->data,1);
                        
                        if(uip_ipaddr_cmp(msg->data,self_ip) && !deleted)
                              remove_group(g)
                        break;

            }
      }
}


/*---------------------------------------------------------------------------*/
/* Notify the backend about a change in cardinality of a group*/
static void notify_backend_register(struct neigh_node* list,uip_ipaddr_t* addr,int remove){
      struct mqtt_register_msg to_send;

      strcpy(&to_send.nationality,nationality);
      tosend.age = age;
      uip_ipaddr_copy(&to_send.addr, addr);
      tosend.is_removing = remove;

      snprintf(topic, BUFFER_SIZE, NODE_INSERT_TOPIC);

      mqtt_publish(&conn, NULL, topic, (uint8_t *)to_send,
               sizeof(to_send), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
}

/*---------------------------------------------------------------------------*/
/* Notify the backend about a change in cardinality of a group*/
static void notify_backend_new_cardinality(struct neigh_node* list,uip_ipaddr_t* addr,int remove){
      struct mqtt_cardinality to_send;

      list_to_arr(list, to_send.group, &to_send.arr_len);
      uip_ipaddr_copy(&to_send.addr, addr);
      tosend.is_removing = remove;

      snprintf(topic, BUFFER_SIZE, MQTT_CARDINALITY_TOPIC);

      mqtt_publish(&conn, NULL, topic, (uint8_t *)to_send,
               sizeof(to_send), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
}

/*---------------------------------------------------------------------------*/
/* Notify the backend about the creation of a new group */
static int notify_backend_group_creation(struct neigh_node* list){
      struct mqtt_group to_send;
      list_to_arr(list, to_send.group, &to_send.arr_len);

      snprintf(topic, BUFFER_SIZE, MQTT_CREATE_TOPIC);

      mqtt_publish(&conn, NULL, topic, (uint8_t *)to_send,
               sizeof(to_send), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
}

/*---------------------------------------------------------------------------*/
/* Notify the backend about the creation of a new group */
static int notify_backend_group_deletion(struct neigh_node* list){
      struct mqtt_group to_send;
      list_to_arr(list, to_send.group, &to_send.arr_len);

      snprintf(topic, BUFFER_SIZE, MQTT_DELETE_TOPIC);

      mqtt_publish(&conn, NULL, topic, (uint8_t *)to_send,
               sizeof(to_send), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
}

/*****************************************************************************/
/* Send the new group to the node, need to serialize first */
static void notify_node_group_creation(uip_ipaddr_t addr){
      static struct msg_template msg_tosend;

      msg_tosend.operation = OP_GET_NEW_GROUP;
      list_to_arr(neighbors, msg_tosend.data, &msg_tosend.arr_len);

      if(NETSTACK_ROUTING.node_is_reachable()) {      
            simple_udp_sendto(&udp_conn, (char*) &msg_tosend, sizeof(struct msg_template), &addr);
      }
}

/*****************************************************************************/
/* Send the current neighbors list to all the neighbors, need to serialize first */
static void send_neigh_list(){
      uip_ipaddr_t addr;
      static struct msg_template msg_tosend;

      msg_tosend.operation = OP_SEND_NEIGH;
      list_to_arr(neighbors, msg_tosend.data, &msg_tosend.arr_len);

      check_missing();
      if(NETSTACK_ROUTING.node_is_reachable()) {
            uip_create_linklocal_allnodes_mcast(&addr);
            simple_udp_sendto(&udp_conn, (char*) &msg_tosend, sizeof(struct msg_template), &addr);
      }
      neighbors_collection_timer_set = 0;  
}

/*****************************************************************************/
/* Look if any of this node's groups are contained within the list of nodes 
 * passed as input. If that's the case update the group and include the new node
 * If leader, notify backend and the new node                                */
static void match_groups(struct msg_template msg, uip_ipaddr_t* s_add){
      struct neigh_node* neigh_neighbors = array_to_list(msg->data,msg->arr_len);

      for each group_node g in groups
            if(is_subset(g->list,neigh_neighbors)){
                  if(is_in_neighbors(g->list, s_add))
                        continue;
                  if(im_leader(g->list)){
                        notify_node_group_creation(g->list);
                        notify_backend_new_cardinality(g->list,s_add,0);  
                  }
                  add_to_neighbors(g->list, s_add);
            }
}

/*****************************************************************************/
/* Find possible new groups to be created and check if they are actually new 
 * groups, if so create them                                                 */
static void find_possible_groups(struct msg_template msg, uip_ipaddr_t* s_add){
      struct neigh_node* l = heapmem_alloc(sizeof(struct neigh_node));
      struct neigh_node* neigh_neighbors = array_to_list(msg->data,msg->arr_len);
      add_to_neighbors(l,self_ip);
      add_to_neighbors(l,s_addr);
      
      for each neigh_node n in neighbors
            if(uip_ip4addr_cmp(n->addr, s_add))
                  continue;
            if(is_in_list(neigh_neighbors, n->addr)){
                  add_to_neighbors(l,n);
                  if(!is_subgroup(l)){
                        add_group(cpy_list(l));
                        remove_from_neighbor_list(l, n->addr);
                        if(im_leader(l)){
                              notify_node_group_creation(l);
                              add_to_neighbors(l,n->addr);
                              notify_backend_group_creation(l);
                        }
                  }      
                  remove_from_neighbor_list(l, n->addr);
            }
            
}

/*****************************************************************************/
/* check if we are part of any groups, if yes ping the neighbors and check if 
 * there is someone missing from the groups and notify the backend if leader
 * otherwise start the discovery process.                                    */
static void discover_neighbors(){
      uip_ipaddr_t addr;
      static struct msg_template msg_tosend;

      uip_gethostaddr(&self_ip);
      msg_tosend.operation = OP_DISCOVER;
      msg_tosend.arr_len = 1;
      uip_ipaddr_copy(&msg_tosend.data[0], &self_ip);
      delete_list(neighbors);
      neighbors = heapmem_alloc(sizeof(struct neigh_node));

      if(NETSTACK_ROUTING.node_is_reachable()) {
            uip_create_linklocal_allnodes_mcast(&addr);
            simple_udp_sendto(&udp_conn, (char*) &msg_tosend, sizeof(struct msg_template), &addr);
      }
}

/*****************************************************************************/
/* Notifies all the members of a group that a node is missing*/
static void notify_nodes_missing(struct neigh_node* list,uip_ipaddr_t* addr){
      struct msg_template msg_tosend;

      msg_tosend.operation = OP_REMOVE_GROUP;
      uip_ipaddr_copy(&msg_tosend.data[0], &addr);
      list_to_arr(neighbors, &msg_tosend.data[1], &msg_tosend.arr_len);
      msg_tosend.arr_len++;

      for each node n in list
            simple_udp_sendto(&udp_conn, (char*) &msg_tosend, sizeof(struct msg_template), &n->addr);

}

/*****************************************************************************/
/* check if any node in any group is missing from the new neighbor list 
 * If someone is missing update the group and check if the group still has 
 * the minimum number of members and it is not a subset of some other group 
 * if leader notify backend      */
static void check_missing(){
      for each group_node g in groups
            if(!everyone_missing(g)){
                  for each neigh_node n in g 
                        if(!is_in_list(neighbors,n->addr)){
                              remove_from_neighbor_list(g->list, n->addr);
                              if(group_size(g)<2 || is_subgroup(g)){
                                    if(im_leader(g))
                                          add_to_neighbors(g->list,n->addr)               //add back the removed node to notify the backend
                                          notify_backend_group_deletion(g);
                                    remove_group(g);
                              }
                              else{
                                    if(im_leader(g))
                                          notify_backend_new_cardinality(g->list,n->addr,1);
                              }

                        }
            }
}

/*****************************************************************************/
/*MQTT callback function*/
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data){
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_INFO("Successfully connected\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    LOG_INFO("Published a message\n");
    break;
  }
  default:
    LOG_WARN("Error!: %i\n", event);
    break;
  }
}

/*****************************************************************************/
/* Connect to MQTT broker                                                    */
static void connect_to_broker(void){

      mqtt_connect(&conn, broker_ip, broker_port, mqtt_interval * 2);
}

/*****************************************************************************/
PROCESS_THREAD(startup, ev, data)
{
      static struct etimer periodic_timer;

      PROCESS_BEGIN();
      registered = 0;
      /* Initialize UDP connection */
      simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
            UDP_SERVER_PORT, udp_rx_callback);
      uip_ipaddr_copy(&self_ip, &uip_hostaddr);

      snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf.org_id, conf.type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

      mqtt_register(&conn, &startup, client_id, mqtt_event, 128);
      notify_backend_register();

      neighbors = heapmem_alloc(sizeof(struct neigh_node));
      groups = heapmem_alloc(sizeof(struct group_node));

      
      etimer_set(&periodic_timer, random_rand() % DISCOVER_INTERVAL);
      while(1) {
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

            registered
            discover_neighbors();
            etimer_set(&periodic_timer, DISCOVER_INTERVAL - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
      }

      PROCESS_END();

}