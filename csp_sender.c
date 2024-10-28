#include <csp/csp_debug.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <stdio.h>
#include "/home/pi1/testproject/message.h"

/* This function must be provided in arch specific way */
int router_start(void);

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT		10

/* Commandline options */
static uint8_t server_address = 1;
static uint8_t client_address = 2;

/* Test mode, check that server & client can exchange packets */
static bool test_mode = false;
static unsigned int successful_ping = 0;
static unsigned int run_duration_in_sec = 3;

Message send_msg = {0};
Message receive_msg = {0};

enum DeviceType {
	DEVICE_UNKNOWN, 
	DEVICE_CAN,
	DEVICE_KISS,
	DEVICE_ZMQ,
};

#define MAX_INPUT_LENGTH 100 

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
		default_iface->addr = client_address;
        default_iface->is_default = 1;
    }

	if (CSP_HAVE_LIBSOCKETCAN && (device_type == DEVICE_CAN)) {
		int error = csp_can_socketcan_open_and_add_interface(device_name, CSP_IF_CAN_DEFAULT_NAME, client_address, 1000000, true, &default_iface);
        if (error != CSP_ERR_NONE) {
			csp_print("failed to add CAN interface [%s], error: %d\n", device_name, error);
            exit(1);
        }
        default_iface->is_default = 1;
    }

	if (CSP_HAVE_LIBZMQ && (device_type == DEVICE_ZMQ)) {
        int error = csp_zmqhub_init(client_address, device_name, 0, &default_iface);
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

int router_start(void) {
	return csp_pthread_create(task_router);
}

/* main - initialization of CSP and start of client task */
int main(int argc, char * argv[]) {

	const char * device_name = NULL;
	enum DeviceType device_type = DEVICE_UNKNOWN;
	csp_iface_t * default_iface;
	struct timespec start_time;
	unsigned int count;
	int ret = EXIT_SUCCESS;
    int opt;

	while ((opt = getopt_long(argc, argv, OPTION_c OPTION_z OPTION_R "k:a:C:tT:h", long_options, NULL)) != -1) {
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
                client_address = atoi(optarg);
                break;
            case 'C':
                server_address = atoi(optarg);
                break;
            case 't':
                test_mode = true;
                break;
            case 'T':
                test_mode = true;
                run_duration_in_sec = atoi(optarg);
                break;
        }
    }
    
    // Unless one of the interfaces are set, print a message and exit
	if (device_type == DEVICE_UNKNOWN) {
		csp_print("One and only one of the interfaces can be set.\n");
        exit(EXIT_FAILURE);
    }

    csp_print("Initialising CSP\n");

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();

    /* Add interface(s) */
	default_iface = add_interface(device_type, device_name);
 	csp_print("Client started\n");
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	count = 'A';

	while (1) {
		struct timespec current_time;

		usleep(test_mode ? 200000 : 1000000);

		/* Send ping to server, timeout 5000 mS, ping size 100 bytes */
		int result = csp_ping(server_address, 5000, 100, CSP_O_NONE);
		csp_print("Ping address: %u, result %d [mS]\n", server_address, result);
        // Increment successful_ping if ping was successful
        if (result >= 0) {
            ++successful_ping;
        }

		/* Send reboot request to server, the server has no actual implementation of csp_sys_reboot() and fails to reboot */
		csp_reboot(server_address);
		csp_print("reboot system request sent to address: %u\n", server_address);

		/* Send data packet (string) to server */

		/* 1. Connect to host on 'server_address', port SERVER_PORT with regular UDP-like protocol and 1000 ms timeout */
		csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, server_address, SERVER_PORT, 1000, CSP_O_NONE);
		if (conn == NULL) {
			/* Connect failed */
			csp_print("Connection failed\n");
			ret = EXIT_FAILURE;
			break;
		}
    
//    char input[MAX_INPUT_LENGTH];
//    printf("Enter a message: ");
//    fgets(input, MAX_INPUT_LENGTH, stdin);
    
    int type, mdid, req_id;
    do {
        printf("Enter Type ID : "); 
        if (scanf("%d", &type) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n'); // Clear the input buffer
            continue; 
        }
        
        printf("Enter MD ID : ");
        if (scanf("%d", &mdid) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n'); 
            continue; 
        }
        
        printf("Enter TTC ID : ");
        if (scanf("%d", &req_id) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n'); 
            continue; 
        }
        
        while (getchar() != '\n'); 

        if (type < 0 || mdid < 0 || req_id < 0) {
            printf("Invalid input.\n");
        }
          
    } while (type < 0 || mdid < 0 || req_id < 0);
    
    send_msg.type = (unsigned char)type;
    send_msg.mdid = (unsigned char)mdid;
    send_msg.req_id = (unsigned char)req_id;


		/* 2. Get packet buffer for message/data */
		csp_packet_t * packet = csp_buffer_get(0);
		if (packet == NULL) {
			/* Could not get buffer element */
			csp_print("Failed to get CSP buffer\n");
			ret = EXIT_FAILURE;
			break;
		} 

		/* 3. Copy data to packet */
//		memcpy(packet->data, "Hello world ", 12);
//		memcpy(packet->data + 12, &count, 1);
//		memset(packet->data + 13, 0, 1);
//		count++;
      memcpy(packet->data, &send_msg, sizeof(send_msg));
csp_print("Sending packet with length: %u\n", sizeof(send_msg)); // ขนาดของ struct ที่ส่ง

//     size_t input_length = strlen(input);
//     memcpy(packet->data, input, input_length);
    

		/* 4. Set packet length */
		packet->length = sizeof(send_msg); /* include the 0 termination */

		/* 5. Send packet */
		csp_send(conn, packet);

		/* 6. Close connection */
		csp_close(conn);

	}

    /* Wait for execution to end (ctrl+c) */
    return ret;
}