//sender
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
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <stdio.h>
#include "/home/pi1/testproject/message.h"

/* This function must be provided in arch specific way */
int router_start(void);

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT		15
 
/* Commandline options */
static uint8_t server_address = 1; 
static uint8_t client_address = 1; 

static unsigned int successful_ping = 0;

Message send_msg = {0}; 
Message receive_msg = {0};
 
void server(void) { 

	csp_print("\n-- Server task started --\n");

	/* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, etc. if enabled during compilation */
	csp_socket_t sock = {0};

	/* Bind socket to all ports, e.g. all incoming connections will be handled here */
	csp_bind(&sock, CSP_ANY);

	/* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
	csp_listen(&sock, 10);

	/* Wait for connections and then process packets on the connection */
	while (1) {
    csp_conn_t *conn2;
    
    /* Wait for a new connection, 10000 mS timeout */
		if ((conn2 = csp_accept(&sock, 10000)) == NULL) {
      csp_print("Connection timeout\n"); 
			continue;
		}
    csp_packet_t *packet2;
		while ((packet2 = csp_read(conn2, 50)) != NULL) {
			switch (csp_conn_dport(conn2)) {    
        default: 
  				/* Process packet here */
          if (packet2->length > sizeof(receive_msg)) {
              csp_print("Packet size too large for receive_msg buffer\n");
              return;  
          }
          memcpy(&receive_msg, packet2->data, packet2->length); // if change to send_msg can use
//          csp_print("Receive Type ID : %u\n", receive_msg.type);
//          csp_print("Receive ModuleID : %u\n", receive_msg.mdid);
//          csp_print("Receive TelemetryID : %u\n", receive_msg.req_id);
//          csp_print("Receive Value : %u\n", receive_msg.val);
//          csp_print("------------------------------\n\n"); 
          if (receive_msg.type == TC_RETURN ){
              if(receive_msg.mdid == 1 && receive_msg.req_id == 1)
              {
                printf("Type : %u\n", receive_msg.type);
                printf("Module ID : %u\n", receive_msg.mdid); 
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- Reboot ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              } 
              else if(send_msg.mdid == 1 && send_msg.req_id == 2)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);  
                printf("---- Shutdown ----\n");                  
                printf("-------------------------------------------\n");
                continue;
                
              }
              else if(send_msg.mdid == 1 && send_msg.req_id == 3)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- Cancel ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              }
              else if(send_msg.mdid == 1 && send_msg.req_id == 4)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- New log ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              }
              else if(send_msg.mdid == 1 && send_msg.req_id == 5)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- Edit number of file ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              }
              else if(send_msg.mdid == 1 && send_msg.req_id == 6)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- Edit size of file ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              }
              else if(send_msg.mdid == 1 && send_msg.req_id == 7)
              {
                printf("Type : %u\n", receive_msg.type); 
                printf("Module ID : %u\n", receive_msg.mdid);
                printf("Telecommand ID : %u\n", receive_msg.req_id);
                printf("---- Edit period ----\n");                  
                printf("-------------------------------------------\n");
                continue;
              }
          }
          if(receive_msg.type == TM_RETURN){
            if(send_msg.mdid == 1 && send_msg.req_id == 1)
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("CPU Temperature: %d C\n", receive_msg.val);
              float cpu_temp  = receive_msg.val ;
              cpu_temp *= 0.001;             
              printf("CPU Temp: %.3f C\n", cpu_temp);                        
              printf("-------------------------------------------\n"); 
              receive_msg.type = TM_REQUEST ;
            }
  
            else if(send_msg.mdid == 1 && send_msg.req_id == 2)
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("Total space: %u MB\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 3) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param); 
              printf("Used space: %u MB\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 4) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("Available space: %u MB\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 5) 
            { 
              uint8_t  ip_address[4];
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              ip_address[0] = (receive_msg.val >> 24);
              ip_address[1] = (receive_msg.val >> 16);  
              ip_address[2] = (receive_msg.val >> 8);
              ip_address[3] = receive_msg.val;
              printf("IP Address : %hhu.%hhu.%hhu.%hhu\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
        
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 6) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              float cpu_usage = receive_msg.val;
              cpu_usage *= 0.01;
              printf("Cpu usage : %.2f %\n", cpu_usage);
              printf("-------------------------------------------\n");
            }  
            else if (receive_msg.mdid == 1 && send_msg.req_id == 7) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              float cpu_peak = receive_msg.val;
              cpu_peak *= 0.01;
              printf("Cpu peak : %.2f %\n", cpu_peak);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 9) 
            {
              uint16_t  ram[2];
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              ram[0] = (receive_msg.val >> 16);
              ram[1] = receive_msg.val;
              printf("Ram usage : %u/%u MB\n", ram[1], ram[0]);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 10) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param); 
              printf("Ram peak : %u MB\n", receive_msg.val);
              printf("-------------------------------------------\n");
            } 
            else if (receive_msg.mdid == 1 && send_msg.req_id == 14) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("Shutdown Time : %u\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 15) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid); 
              printf("Telemetry ID : %u\n", receive_msg.req_id); 
              printf("Telemetry Parameter : %u\n", receive_msg.param); 
              printf("Remaining shutdown time : %u\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 16) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("Shutdown type : %u\n", receive_msg.val);
              printf("-------------------------------------------\n");
            }
            else if (receive_msg.mdid == 1 && send_msg.req_id == 17) 
            {
              printf("Type : %u\n", receive_msg.type);
              printf("Module ID : %u\n", receive_msg.mdid);
              printf("Telemetry ID : %u\n", receive_msg.req_id);
              printf("Telemetry Parameter : %u\n", receive_msg.param);
              printf("OBC Time : %u\n", receive_msg.val);
              printf("-------------------------------------------\n");
            } 
          }
          else  
            { 
              printf("No Type\n"); 
              printf("-------------------------------------------\n");  
            }
  				csp_buffer_free(packet2);                               
  				break;   
		  }  
		}
   
		/* Close current connection */
		csp_close(conn2); 
   break;      
	} 
   
	return;   
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

int main(int argc, char * argv[]) {
    int ret = EXIT_SUCCESS;
    int type, mdid, req_id;
    
    csp_print("Initialising CSP\n");

    /* Init CSP */
    csp_init();

    /* Start router */
    router_start();
     
    csp_iface_t *kiss_iface = NULL;
    csp_usart_conf_t uart_conf = {
         .device =  "/dev/serial0",
         .baudrate = 115200, /* supported on all platforms */
         .databits = 8, 
         .stopbits = 1,
         .paritysetting = 0,
    };
    //open UART and ADD Interface
    int result = csp_usart_open_and_add_kiss_interface(&uart_conf, CSP_IF_KISS_DEFAULT_NAME, &kiss_iface);
    if (result != CSP_ERR_NONE) {
        printf("Error adding KISS interface: %d\n", result);
        return result;
    } else {
        printf("KISS interface added successfully\n");
    }    
    
    kiss_iface->addr = client_address;
    kiss_iface->is_default = 1;
    
    csp_print("Client started\n"); 
    
    csp_print("Connection table\r\n");  
    csp_conn_print_table();

    csp_print("Interfaces\r\n");
    csp_iflist_print(); 
    
    while (1) { 
        int result = csp_ping(server_address, 1000, 100, CSP_O_NONE);
        if (result < 0) {
            csp_print("Ping failed for address %u: %d\n", server_address, result);
        } 
        else {
            csp_print("Ping succeeded for address %u: %d [mS]\n", server_address, result);
        }    

//        csp_reboot(server_address);
//		    csp_print("reboot system request sent to address: %u\n", server_address);
        /* 1. Connect to host on 'server_address', port SERVER_PORT with regular UDP-like protocol and 1000 ms timeout */
    		csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, server_address, SERVER_PORT, 1000, CSP_O_NONE);
    		if (conn == NULL) {
      			/* Connect failed */ 
      			csp_print("Connection failed\n");
      			ret = EXIT_FAILURE;
      			break; 
    		}
             
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
        
        if(send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 1 || send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 2){
            int delay;
            do {
                printf("Enter delay in seconds : ");
                scanf("%d", &delay);
                while (getchar() != '\n');
                if (delay < 0) { 
                    printf("Invalid input\n");
                }
            } while (delay < 0);
            send_msg.param = (unsigned char)delay;
        }

        if(send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 5){
            int num_file;
            do {
                printf("Enter number of file in folder : ");
                scanf("%d", &num_file);
                while (getchar() != '\n');
                if (num_file < 0) {
                    printf("Invalid input\n");
                }
            } while (num_file < 0);
            printf("%d\n", num_file);
            send_msg.param = (unsigned int)num_file;
            printf("%u\n", send_msg.param);
        }
        if(send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 6){
            int file_size;
            do {
                printf("Enter size of file : ");
                scanf("%d", &file_size); 
                while (getchar() != '\n');
                if (file_size < 0) { 
                    printf("Invalid input\n");
                }
            } while (file_size < 0);
            send_msg.param = (unsigned int)file_size;
        }
        
        if(send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 7){
            int period;
            do {
                printf("Enter period : ");
                scanf("%d", &period);
                while (getchar() != '\n');
                if (period < 0) {
                    printf("Invalid input\n");
                }
            } while (period < 0);
            send_msg.param = (unsigned int)period;
        }
        
    		/* 2. Get packet buffer for message/data */
    		csp_packet_t * packet = csp_buffer_get(0);
    		if (packet == NULL) {
    			/* Could not get buffer element */
    			csp_print("Failed to get CSP buffer\n");
    			ret = EXIT_FAILURE; 
    			break;
    		} 
    
    		/* 3. Copy data to packet */
        memcpy(packet->data, &send_msg, sizeof(send_msg));    
    
    		/* 4. Set packet length */
    		packet->length = sizeof(send_msg); 
       
    		/* 5. Send packet */
    		csp_send(conn, packet);   
        csp_close(conn);
        server();
   	}
    /* Wait for execution to end (ctrl+c) */
    return ret;
}
