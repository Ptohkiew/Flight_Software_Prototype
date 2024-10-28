//receiver

#include <csp/csp_debug.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include "/home/pi1/testproject/message.h"
#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <sys/resource.h>
#include <time.h>


/* These three functions must be provided in arch specific way */
int router_start(void);
int server_start(void);

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT		10 
#define SERVER_PORT2		20 

/* Commandline options */
static uint8_t server_address = 1;


/* Test mode, check that server & client can exchange packets */
static bool test_mode = false;
static unsigned int server_received = 0;
static unsigned int run_duration_in_sec = 3;
static unsigned int successful_ping = 0;

Message send_msg = {0};
Message receive_msg = {0}; 
mqd_t mqdes_dis, mqdes_tm, mqdes_tc,mqdes_obc;

enum DeviceType { 
	DEVICE_UNKNOWN,
	DEVICE_CAN,
	DEVICE_KISS,  
	DEVICE_ZMQ,
}; 

#define __maybe_unused __attribute__((__unused__))

void *msg_dis(void *arg) {
    mqdes_tm = mq_open("/mq_tm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    mqdes_tc = mq_open("/mq_tc", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    mqdes_obc = mq_open("/mq_obc", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
    
    mqd_t mqdes_type = mq_open("/mq_ttctype", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes); //send to aocs_dispatcher
	  mqd_t mq_return = mq_open("/mq_return_sender", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes); //return from aocs_dispatcher

    if (mqdes_dis == -1 || mqdes_tm == -1 || mqdes_tc == -1) {
        perror("mq_open"); 
        pthread_exit(NULL);
    } 

    //while (1) {   
        //Message send_msg = {0};
        //Message receive_msg = {0};  
        send_msg.type = receive_msg.type;
        send_msg.mdid = receive_msg.mdid;
        send_msg.req_id = receive_msg.req_id;
        if (send_msg.type == TM_REQUEST && send_msg.mdid == 1 && send_msg.req_id > 0 && send_msg.req_id <= 17) {
          printf("- REQUEST TM -\n");
          printf("Type : %u\n", send_msg.type);
          printf("Receive ModuleID : %u\n", send_msg.mdid);
          printf("Receive TelemetryID : %u\n", send_msg.req_id);
          printf("-------------------------------------------\n");
          if (mq_send(mqdes_tm, (char *)&send_msg, sizeof(send_msg), 1) == -1) {
              perror("mq_send");
              
          }
          if (mq_receive(mqdes_tm, (char *)&receive_msg, sizeof(receive_msg), NULL) == -1) {
              perror("mq_receive");
          }  
          
          if (receive_msg.type == TM_RETURN) {
            printf("- RETURN TM -\n");
            printf("Type : %u\n", receive_msg.type);
            printf("Respond ModuleID : %u\n", receive_msg.mdid);
            printf("Respond TelemetryID : %u\n", receive_msg.req_id);
            printf("-------------------------------------------\n");
          }  
          else {
            printf("- REQUEST NO TYPE -\n");
            printf("Type : %u\n", send_msg.type);
            printf("Receive ModuleID : %u\n", send_msg.mdid);
            printf("Receive TelemetryID : %u\n", send_msg.req_id);
            printf("No Type\n"); 
            printf("-------------------------------------------\n");
          }       
        }
        else if (send_msg.type == TM_REQUEST && send_msg.mdid == 2 && send_msg.req_id != 0) {
            printf("- REQUEST TM -\n");
            printf("Type : %u\n", send_msg.type);
            printf("Receive ModuleID : %u\n", send_msg.mdid);
            printf("Receive TelemetryID : %u\n", send_msg.req_id);
            printf("-------------------------------------------\n");
            if (mq_send(mqdes_type, (char *)&send_msg, sizeof(send_msg), 1) == -1) {
                perror("mq_send");
                 
            }
            if (mq_receive(mq_return, (char *)&receive_msg, sizeof(receive_msg), NULL) == -1) {
                perror("mq_receive");
                
            }
            if (receive_msg.type == TM_RETURN) {
              printf("- RETURN TM -\n");
              printf("Type : %u\n", receive_msg.type);
              printf("Respond ModuleID : %u\n", receive_msg.mdid);
              printf("Respond TelemetryID : %u\n", receive_msg.req_id);
              printf("-------------------------------------------\n");
            }         
        }
        
        else if (send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id > 0 && send_msg.req_id <= 7) {
            printf("- REQUEST TC -\n");
            printf("Type : %u\n", send_msg.type);
            printf("Receive ModuleID : %u\n", send_msg.mdid);
            printf("Receive TelecommandID : %u\n", send_msg.req_id);
            printf("-------------------------------------------\n");
            if (mq_send(mqdes_tc, (char *)&send_msg, sizeof(send_msg), 1) == -1) {
                perror("mq_send");
            }
            if (mq_receive(mqdes_tc, (char *)&receive_msg, sizeof(receive_msg), NULL) == -1) {
                perror("mq_receive");    
            }
                            
            if (receive_msg.type == TC_RETURN  && receive_msg.val == 1) {
              printf("- REJECT TC -\n");
              printf("Type : %u\n", receive_msg.type);
              printf("Respond ModuleID : %u\n", receive_msg.mdid);
              printf("Respond TelecommandID : %u\n", receive_msg.req_id);
              printf("-------------------------------------------\n");
            }  
                   
            else if (receive_msg.type == TC_RETURN ) {
              printf("- RETURN TC -\n");
              printf("Type : %u\n", receive_msg.type);
              printf("Respond ModuleID : %u\n", receive_msg.mdid);
              printf("Respond TelecommandID : %u\n", receive_msg.req_id);
              printf("-------------------------------------------\n");
            }  
        } 
        else {
            printf("- REQUEST NO TYPE -\n");
            printf("Type : %u\n", send_msg.type);
            printf("Receive ModuleID : %u\n", send_msg.mdid);
            printf("Receive TCID/TMID : %u\n", send_msg.req_id);
            printf("No Type\n"); 
            printf("-------------------------------------------\n");
        } 
    //}  

//    mq_close(mqdes_dis);   
//    mq_close(mqdes_tm);
//    mq_close(mqdes_tc);
//    mq_unlink("/mq_dispatch");
//    mq_unlink("/mq_tm");
//    mq_unlink("/mq_tc");
}

void *return_csp(void *arg){
    csp_conn_t * conn2 = csp_connect(CSP_PRIO_NORM, server_address, SERVER_PORT2, 1000, CSP_O_NONE);
    if (conn2 == NULL) {
        csp_print("Connection failed\n");
        return NULL;
    }
    
    csp_packet_t * packet2 = csp_buffer_get(sizeof(receive_msg));
    if (packet2 == NULL) {
        csp_print("Failed to get CSP buffer\n");
        csp_close(conn2);
        return NULL;
    }
    
    memcpy(packet2->data, &receive_msg, sizeof(receive_msg));
    packet2->length = sizeof(receive_msg); 
    int result = csp_ping(server_address, 5000, 100, CSP_O_NONE);
		csp_print("Ping address: %u, result %d [mS]\n", server_address, result);
        // Increment successful_ping if ping was successful
        if (result >= 0) {
            ++successful_ping;
        }
        csp_print("Connection table\r\n");
    csp_conn_print_table();

    csp_print("Interfaces\r\n");
    csp_iflist_print();
    csp_print("Sending packet...");
    csp_send(conn2, packet2);
    csp_print("Packet sent: %u\n", packet2->id);
    
    csp_buffer_free(packet2);
    csp_close(conn2);
    
    return NULL;
}
 
/* Server task - handles requests from clients */
void server(void) {

	csp_print("Server task started\n");

	/* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, etc. if enabled during compilation */
	csp_socket_t sock = {0};

	/* Bind socket to all ports, e.g. all incoming connections will be handled here */
	csp_bind(&sock, CSP_ANY);

	/* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
	csp_listen(&sock, 10);

	/* Wait for connections and then process packets on the connection */
	while (1) {

		/* Wait for a new connection, 10000 mS timeout */
		csp_conn_t *conn;
		if ((conn = csp_accept(&sock, 10000)) == NULL) {
			/* timeout */
			continue;
		}

		/* Read packets on connection, timout is 100 mS */
		csp_packet_t *packet;
		while ((packet = csp_read(conn, 50)) != NULL) {
			switch (csp_conn_dport(conn)) {
  			case SERVER_PORT:
  				/* Process packet here */
  				//csp_print("Packet received on SERVER_PORT: %s\n", (char *) packet->data);
          memcpy(&receive_msg, packet->data, packet->length);
          csp_print("Type : %u\n", receive_msg.type);
          csp_print("Receive ModuleID : %u\n", receive_msg.mdid);
          csp_print("Receive TelemetryID : %u\n", receive_msg.req_id);
          csp_print("Packet : %u\n",packet);
          msg_dis(NULL);
          return_csp(NULL);
          csp_print("Back");
  				csp_buffer_free(packet);
  				++server_received;                               
  				break; 
  
  			default:
  				/* Call the default CSP service handler, handle pings, buffer use, etc. */
  				csp_service_handler(packet);
  				break;
		  }
		}
 
		/* Close current connection */
		csp_close(conn);
	}

	return; 
}
/* End of server task */
 
static struct option long_options[] = {
    {"kiss-device", required_argument, 0, 'k'},
#if (CSP_HAVE_LIBSOCKETCAN)
	#define OPTION_c "c:"
    {"can-device", required_argument, 0, 'c'},
#else
	#define OPTION_c
#endif
#if (CSP_HAVE_LIBZMQ)
	#define OPTION_z "z:"
    {"zmq-device", required_argument, 0, 'z'},
#else
	#define OPTION_z
#endif
#if (CSP_USE_RTABLE)
	#define OPTION_R "R:"
    {"rtable", required_argument, 0, 'R'},
#else
	#define OPTION_R
#endif
    {"interface-address", required_argument, 0, 'a'},
    {"connect-to", required_argument, 0, 'C'},
    {"test-mode", no_argument, 0, 't'},
    {"test-mode-with-sec", required_argument, 0, 'T'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

void print_help() {
    csp_print("Usage: csp_server [options]\n");
	if (CSP_HAVE_LIBSOCKETCAN) {
		csp_print(" -c <can-device>  set CAN device\n");
	}
	if (1) {
		csp_print(" -k <kiss-device> set KISS device\n");
	}
	if (CSP_HAVE_LIBZMQ) {
		csp_print(" -z <zmq-device>  set ZeroMQ device\n");
	}
	if (CSP_USE_RTABLE) {
		csp_print(" -R <rtable>      set routing table\n");
	}
	if (1) {
		csp_print(" -a <address>     set interface address\n"
				  " -t               enable test mode\n"
				  " -T <dration>     enable test mode with running time in seconds\n"
				  " -h               print help\n");
	}
}

csp_iface_t * add_interface(enum DeviceType device_type, const char * device_name)
{
    csp_iface_t * default_iface = NULL;

    if (device_type == DEVICE_KISS) {
        csp_usart_conf_t conf = {
			.device = device_name,
            .baudrate = 115200, /* supported on all platforms */
            .databits = 8,
            .stopbits = 1,
            .paritysetting = 0,
		};
        int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME,  &default_iface);
        if (error != CSP_ERR_NONE) {
            csp_print("failed to add KISS interface [%s], error: %d\n", device_name, error);
            exit(1);
        }
		default_iface->addr = server_address;
        default_iface->is_default = 1;
    }

    if (CSP_HAVE_LIBSOCKETCAN && (device_type == DEVICE_CAN)) {
        int error = csp_can_socketcan_open_and_add_interface(device_name, CSP_IF_CAN_DEFAULT_NAME, server_address, 1000000, true, &default_iface);
        if (error != CSP_ERR_NONE) {
            csp_print("failed to add CAN interface [%s], error: %d\n", device_name, error);
            exit(1);
        }
        default_iface->is_default = 1;
    }

    if (CSP_HAVE_LIBZMQ && (device_type == DEVICE_ZMQ)) {
        int error = csp_zmqhub_init(server_address, device_name, 0, &default_iface);
        if (error != CSP_ERR_NONE) {
            csp_print("failed to add ZMQ interface [%s], error: %d\n", device_name, error);
            exit(1);
        }
        default_iface->is_default = 1;
    }

	return default_iface;
}

static int csp_pthread_create(void * (*routine)(void *)) {

	pthread_attr_t attributes;
	pthread_t handle; 
	int ret;

	if (pthread_attr_init(&attributes) != 0) {
		return CSP_ERR_NOMEM;
	}
	/* no need to join with thread to free its resources */
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

	ret = pthread_create(&handle, &attributes, routine, NULL);
	pthread_attr_destroy(&attributes);

	if (ret != 0) {
		return ret;
	}

	return CSP_ERR_NONE;
}

static void * task_router(void * param) {

	/* Here there be routing */
	while (1) {
		csp_route_work();
	}

	return NULL;
}

static void * task_server(void * param) {
	server();
	return NULL;
}


int router_start(void) {
	return csp_pthread_create(task_router);
}

int server_start(void) {
	return csp_pthread_create(task_server);
}




/* main - initialization of CSP and start of server task */
int main(int argc, char * argv[]) {

	const char * device_name = NULL;
	enum DeviceType device_type = DEVICE_UNKNOWN;
	const char * rtable __maybe_unused = NULL;
	csp_iface_t * default_iface;
    int opt;

	while ((opt = getopt_long(argc, argv, OPTION_c OPTION_z OPTION_R "k:a:tT:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
				device_name = optarg;
				device_type = DEVICE_CAN;
                break;
            case 'k':
				device_name = optarg;
				device_type = DEVICE_KISS;
                break;
            case 'z':
				device_name = optarg;
				device_type = DEVICE_ZMQ;
                break;
#if (CSP_USE_RTABLE)
            case 'R':
                rtable = optarg;
                break;
#endif
            case 'a':
                server_address = atoi(optarg);
                break;
            case 't':
                test_mode = true;
                break;
            case 'T':
                test_mode = true;
				run_duration_in_sec = atoi(optarg);
                break;
            case 'h':
				print_help();
				exit(EXIT_SUCCESS);
            case '?':
                // Invalid option or missing argument
				print_help();
                exit(EXIT_FAILURE);
        }
    }

	// If more than one of the interfaces are set, print a message and exit
	if (device_type == DEVICE_UNKNOWN) {
		csp_print("Only one of the interfaces can be set.\n");
        print_help();
        exit(EXIT_FAILURE);
    }

    csp_print("Initialising CSP\n");

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();

    /* Add interface(s) */
	default_iface = add_interface(device_type, device_name);

	/* Setup routing table */
    if (CSP_USE_RTABLE) {
		if (rtable) {
			int error = csp_rtable_load(rtable);
			if (error < 1) {
				csp_print("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
				exit(1);
			}
		} else if (default_iface) {
			csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
		}
	}

    //csp_print("Connection table\r\n");
    //csp_conn_print_table();

    csp_print("Interfaces\r\n");
    csp_iflist_print();

	if (CSP_USE_RTABLE) {
		csp_print("Route table\r\n");
		csp_rtable_print();
	}

    /* Start server thread */
    server_start();

    /* Wait for execution to end (ctrl+c) */
    while(1) { 
        sleep(run_duration_in_sec);

        if (test_mode) {
            /* Test mode, check that server & client can exchange packets */ 
            if (server_received < 5) {
                csp_print("Server received %u packets\n", server_received);
                exit(EXIT_FAILURE);
            }
            csp_print("Server received %u packets\n", server_received);
            exit(EXIT_SUCCESS);
        }
    }
    

    return 0;
}
