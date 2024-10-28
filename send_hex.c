// sender
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
#include "/home/pi3/libcsp/test/message.h"
#include <termios.h>
#include <errno.h>
#include <zlib.h>
/* This function must be provided in arch specific way */
int router_start(void);

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT 15

#define BITMASKL16 0xFFFF0000
#define BITMASKR16 0x0000FFFF

#ifndef SENDER_ID
#define SENDER_ID 1  // Default value if no value is passed during compilation
#endif

/* Commandline options */
static uint8_t server_address = 1;
static uint8_t client_address = 1;

static unsigned int successful_ping = 0;
uint16_t addr = 10;  // Address สำหรับ interface นี้ (คุณสามารถเปลี่ยนตามที่ต้องการ)

Message send_msg = {0};
Message receive_msg = {0};
int CRCflags;

uint16_t get_16bitLR(uint32_t val, int LR){
	uint32_t bitmask = 0;
	printf("LR : %d\n",LR);
	if(LR == 0){
		bitmask = BITMASKL16;
	}
	else if(LR == 1){
		bitmask = BITMASKR16;
	}
	else{
		return 0;
	}
	printf("bitmask : %u\n", bitmask);
	printf("val : %u\n", val);
	printf("val& : %u\n", val&bitmask);
	return (val & bitmask);
}

void convert_header_to_decimal(const char * hex_string, csp_packet_t * packet, uint16_t * destination) {
	size_t len = strlen(hex_string);
	size_t chunk_count = 0;  // Counter for the number of 32-bit chunks processed
	size_t packet_idx = 0;
	size_t i = 0;
	// for (size_t i = 0; i < 11; i += 2) {
	char temp[9] = {0};  // Buffer for 8 hex characters + null terminator
	char temp1[9] = {0};
	char temp2[9] = {0};
	char temp3[9] = {0};
	char temp4[9] = {0};
	char temp5[9] = {0};
	char temp6[9] = {0};
	char temp7[9] = {0};
	char temp8[9] = {0};
	char alltemp[50] = "";

	strncpy(temp, &hex_string[i], 4);
	// Convert the hex chunk to decimal considering little-endian order
	unsigned int decimal_value = (unsigned int)strtoul(temp, NULL, 16);
	// Print the decimal value
	//printf("32-bit chunk pri: %s -> Decimal: %u\n", temp, decimal_value);

	unsigned int pri = (decimal_value >> 14);
	pri = pri & 0X3;
	memcpy(&packet->id.pri, &pri, sizeof(unsigned int));
	unsigned int bitdst = decimal_value & 0X3FFF;
	memcpy(&packet->id.dst, &bitdst, sizeof(unsigned int));
	//printf("des : %u\n", packet->id.dst);

	*destination = packet->id.dst;

	if (*destination != SENDER_ID) {
		return;
	}
	// // Copy 8 hex characters (32 bits) from the hex string
	strncpy(temp1, &hex_string[i + 4], 4);
	// Convert the hex chunk to decimal considering little-endian order
	decimal_value = (unsigned int)strtoul(temp1, NULL, 16);
	// Print the decimal value
	//printf("32-bit chunk destination: %s -> Decimal: %u\n", temp1, decimal_value);

	pri = (decimal_value >> 2);
	pri = pri & 0X3FFF;
	memcpy(&packet->id.src, &pri, sizeof(unsigned int));
	unsigned int firstdport = decimal_value & 0X3;
	// memcpy(&packet->id.dport, &pri, sizeof(unsigned int));

	/// memcpy(&packet->id.dst, &decimal_value, sizeof(unsigned int));

	strncpy(temp2, &hex_string[i + 8], 4);
	decimal_value = (unsigned int)strtoul(temp2, NULL, 16);
	//printf("32-bit chunk source : %s -> Decimal: %u\n", temp2, decimal_value);

	unsigned int secdport = decimal_value >> 12;
	secdport = secdport & 0XF;
	uint32_t dport = firstdport << 4 | secdport;
	memcpy(&packet->id.dport, &dport, sizeof(unsigned int));

	pri = (decimal_value >> 6);
	pri = pri & 0X3F;
	memcpy(&packet->id.sport, &pri, sizeof(unsigned int));

	unsigned int flags = decimal_value & 0X3F;
	memcpy(&packet->id.flags, &flags, sizeof(unsigned int));

	uint8_t hmac_flags = (flags >> 3) & 1;
	//printf("hmac_flags : %u\n", hmac_flags);
	uint8_t xtea_flags = (flags >> 2) & 1;
	//printf("xtea_flags : %u\n", xtea_flags);
	uint8_t rdp_flags = (flags >> 1) & 1;
	//printf("rdp_flags : %u\n", rdp_flags);
	uint8_t crc_flags = flags & 1;
	//printf("crc_flags : %u\n", crc_flags);

	strncpy(temp3, &hex_string[i + 12], 8);
	decimal_value = (unsigned int)strtoul(temp3, NULL, 16);
	//printf("32-bit chunk type : %s -> Decimal: %u\n", temp3, decimal_value);
	memcpy(&receive_msg.type, &decimal_value, sizeof(unsigned int));

	// strncpy(temp, &hex_string[i+20], 4);
	// decimal_value = (unsigned int)strtoul(temp, NULL, 16);
	// printf("32-bit chunk: %s -> Decimal: %u\n", temp, decimal_value);
	// memcpy(&packet->id.flags, &decimal_value, sizeof(unsigned int));

	strncpy(temp4, &hex_string[i + 20], 8);
	decimal_value = (unsigned int)strtoul(temp4, NULL, 16);
	//printf("32-bit chunk mdid : %s -> Decimal: %u\n", temp4, decimal_value);
	memcpy(&receive_msg.mdid, &decimal_value, sizeof(unsigned int));

	strncpy(temp5, &hex_string[i + 28], 8);
	decimal_value = (unsigned int)strtoul(temp5, NULL, 16);
	//printf("32-bit chunk req_id : %s -> Decimal: %u\n", temp5, decimal_value);
	memcpy(&receive_msg.req_id, &decimal_value, sizeof(unsigned int));

	strncpy(temp6, &hex_string[i + 36], 8);
	decimal_value = (unsigned int)strtoul(temp6, NULL, 16);
	//printf("32-bit chunk param : %s -> Decimal: %u\n", temp6, decimal_value);
	memcpy(&receive_msg.param, &decimal_value, sizeof(unsigned int));

	strncpy(temp7, &hex_string[i + 44], 8);
	decimal_value = (unsigned int)strtoul(temp7, NULL, 16);
	//printf("32-bit chunk val : %s -> Decimal: %u\n", temp7, decimal_value);
	memcpy(&receive_msg.val, &decimal_value, sizeof(unsigned int));

	strncpy(temp8, &hex_string[i + 52], 8);
	decimal_value = (unsigned int)strtoul(temp8, NULL, 16);
	//printf("32-bit chunk CRC : %s -> Decimal: %u\n", temp8, decimal_value);
	if(decimal_value == 99999999){
		printf("Reject CRC from receiver\n");
	}
	//memcpy(&receive_msg.val, &decimal_value, sizeof(unsigned int));

	chunk_count++;  // Increment the chunk counter

	strcat(alltemp, temp);
	strcat(alltemp, temp1);
	strcat(alltemp, temp2);
	strcat(alltemp, temp3);
	strcat(alltemp, temp4);
	strcat(alltemp, temp5);
	strcat(alltemp, temp6);
	strcat(alltemp, temp7);

	//printf("Return packet : %s\n", alltemp);

	if (hmac_flags == 1) {
		printf("HMAC enable\n");
	}

	if (xtea_flags == 1) {
		printf("XTEA enable\n");
	}

	if (rdp_flags == 1) {
		printf("RDP enable\n");
	}

	unsigned long crc = crc32(0L, Z_NULL, 0);  // เริ่มต้นค่า CRC32
	//printf("CRC32: %lx\n", crc);
	if (crc_flags == 1) {
		printf("CRC enable\n");
		// คำนวณค่า CRC32 ของข้อมูล
		crc = crc32(crc, alltemp, strlen(alltemp));
		// แสดงผลค่า CRC32
		printf("CRC32: %lX\n", crc);  // แสดงผลค่าเป็นเลขฐาน 16 (hex)

		char crcString[20];
		sprintf(crcString, "%lX", crc); 

		if(strcmp(crcString, temp8) == 0){
			printf("CRC equal!!\n");
		}
		else{
			printf("CRC false!!\n");
		}
	}
}

void server(void) {
	while (1) {
		int uart0_filestream = open("/dev/serial0", O_RDONLY | O_NOCTTY);

		if (uart0_filestream == -1) {
			printf("Error - Unable to open UART.\n");
		}

		struct termios options;
		tcgetattr(uart0_filestream, &options);
		options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
		options.c_iflag = IGNPAR;
		options.c_oflag = 0;
		options.c_lflag = 0;
		tcflush(uart0_filestream, TCIFLUSH);
		tcsetattr(uart0_filestream, TCSANOW, &options);

		unsigned char rx_buffer[256];                         // Buffer สำหรับข้อมูลที่รับ
		char combined_buffer[512];                            // Buffer สำหรับรวมข้อมูล
		memset(combined_buffer, 0, sizeof(combined_buffer));  // เคลียร์ buffer

		for (int i = 0; i < 2; i++) {
			int rx_length = read(uart0_filestream, rx_buffer, sizeof(rx_buffer) - 1);

			if (rx_length > 0) {
				rx_buffer[rx_length] = '\0';  // Null terminator
				//printf("Bytes read: %d\n", rx_length);

				// รวมข้อมูลที่รับเข้ากับ combined_buffer
				strncat(combined_buffer, rx_buffer, sizeof(combined_buffer) - strlen(combined_buffer) - 1);

				//printf("Combined Received: %s\n", combined_buffer);  // แสดงข้อมูลที่รวม
			} else if (rx_length < 0) {
				printf("Error: %s\n", strerror(errno));
			}

			usleep(8000);
		}
		csp_packet_t * packet;
		packet->id.pri = 0;
		size_t receive_msg_size = sizeof(receive_msg);
		// packet->length = sizeof(receive_msg);
		//  ขนาดของ packet->data (ไม่สามารถรู้ได้เพราะเป็น pointer แต่ใช้ packet_length แทน)
		// printf("Size of receive_msg: %zu bytes\n", receive_msg_size);
		// printf("Size of packet->data (from packet_length): %d bytes\n", packet->length);
		printf("Return Packet : %s\n", combined_buffer);
		uint16_t destination;
		//csp_print("pri : %u\n", packet->id.pri);
		convert_header_to_decimal(combined_buffer, packet, &destination);
		if (destination != SENDER_ID) {
				printf("False destination : %u\n", destination);
				break;
		}
		// convert_hex_string_to_decimal(combined_buffer, packet);
		//printf("packet : %u\n", packet->data);
		// printf("packet : %u\n", packet->data[0]);
		// printf("packet : %u\n", packet->data[1]);
		// printf("packet : %u\n", packet->data[2]);
		// memcpy(&receive_msg, packet->data, packet->length);
		// csp_print("Type : %u\n", receive_msg.type);
		// csp_print("Receive ModuleID : %u\n", receive_msg.mdid);
		// csp_print("Receive TelemetryID : %u\n", receive_msg.req_id);
		// csp_print("pri : %u\n", packet->id.pri);
		// csp_print("dst: %u\n", destination);
		// csp_print("src: %u\n", packet->id.src);
		// csp_print("dport: %u\n", packet->id.dport);
		// csp_print("sport: %u\n", packet->id.sport);
		// csp_print("flags: %u\n", packet->id.flags);
		// if(destination == SENDER_ID){
		//     break;
		// }

		close(uart0_filestream);
		//printf("HI\n");
		if (receive_msg.type == TC_RETURN) {
			if (send_msg.mdid == 1 && send_msg.req_id == 1) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("Telecommand Val : %u\n", receive_msg.val);
				if(receive_msg.val == 0){
					printf("---- Reboot ----\n");
				}
				else if(receive_msg.val == 1){
					printf("---- Reboot in progress ----\n");
				}
				else if(receive_msg.val == 2){
					printf("---- Shutdown in progress ----\n");
				}
				printf("-------------------------------------------\n");
				break;;
			} else if (send_msg.mdid == 1 && send_msg.req_id == 2) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("Telecommand Val : %u\n", receive_msg.val);
				if(receive_msg.val == 0){
					printf("---- Shutdown ----\n");
				}
				else if(receive_msg.val == 1){
					printf("---- Reboot in progress ----\n");
				}
				else if(receive_msg.val == 2){
					printf("---- Shutdown in progress ----\n");
				}
				printf("-------------------------------------------\n");
				break;

			} else if (send_msg.mdid == 1 && send_msg.req_id == 3) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("Telecommand Val : %u\n", receive_msg.val);
				if(receive_msg.val == 1){
					printf("---- Cancel Reboot ----\n");
				}
				else if(receive_msg.val == 2){
					printf("---- Cancel Shutdown ----\n");
				}
				else if(receive_msg.val == 0){
					printf("---- Cancel Error - No shutdown/reboot now ----\n");
				}
				printf("-------------------------------------------\n");
				break;
			} else if (send_msg.mdid == 1 && send_msg.req_id == 4) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("---- New log ----\n");
				printf("-------------------------------------------\n");
				break;;
			} else if (send_msg.mdid == 1 && send_msg.req_id == 5) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("---- Edit number of file ----\n");
				printf("-------------------------------------------\n");
				break;
			} else if (send_msg.mdid == 1 && send_msg.req_id == 6) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("---- Edit size of file ----\n");
				printf("-------------------------------------------\n");
				break;
			} else if (send_msg.mdid == 1 && send_msg.req_id == 7) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("---- Edit period ----\n");
				printf("-------------------------------------------\n");
				break;
			}
			else if (send_msg.mdid == 3 && send_msg.req_id == 1) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("Telecommand Val : %u\n", receive_msg.val);
				printf("---- Open IMU ----\n");
				printf("-------------------------------------------\n");
				break;

			}
			else if (send_msg.mdid == 3 && send_msg.req_id == 2) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telecommand ID : %u\n", receive_msg.req_id);
				printf("Telecommand Val : %u\n", receive_msg.val);
				if (receive_msg.val == 0){
					printf("---- Not fix ----\n");
				}
				else if (receive_msg.val == 1){
					printf("---- Some error ----\n");
				}
				else if (receive_msg.val == 2){
					printf("---- Ready ----\n");
				}
				else if (receive_msg.val == 3){
					printf("---- Connect ----\n");
				}

				printf("-------------------------------------------\n");
				break;

			} 
		}
		if (receive_msg.type == TM_RETURN) {
			if (send_msg.mdid == 1 && send_msg.req_id == 1) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("CPU Temperature (Raw): %d C\n", receive_msg.val);
				float cpu_temp = receive_msg.val;
				cpu_temp *= 0.001;
				printf("CPU Temp: %.3f C\n", cpu_temp);
				printf("-------------------------------------------\n");
				receive_msg.type = TM_REQUEST;
			}

			else if (send_msg.mdid == 1 && send_msg.req_id == 2) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Total space: %u MB\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 3) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Used space: %u MB\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 4) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Available space: %u MB\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 5) {
				uint8_t ip_address[4];
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
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 6) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				float cpu_usage = receive_msg.val;
				cpu_usage *= 0.01;
				printf("Cpu usage : %.2f\n", cpu_usage);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 7) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				float cpu_peak = receive_msg.val;
				cpu_peak *= 0.01;
				printf("Cpu peak : %.2f\n", cpu_peak);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 9) {
				uint16_t ram[2];
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				ram[0] = (receive_msg.val >> 16);
				ram[1] = receive_msg.val;
				printf("Ram usage : %u/%u MB\n", ram[1], ram[0]);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 10) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Ram peak : %u MB\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 14) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Shutdown Time : %u\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 15) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Remaining shutdown time : %u\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 16) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("Shutdown type : %u\n", receive_msg.val);
				printf("-------------------------------------------\n");
			} else if (receive_msg.mdid == 1 && send_msg.req_id == 17) {
				printf("Type : %u\n", receive_msg.type);
				printf("Module ID : %u\n", receive_msg.mdid);
				printf("Telemetry ID : %u\n", receive_msg.req_id);
				printf("Telemetry Parameter : %u\n", receive_msg.param);
				printf("OBC Time : %u\n", receive_msg.val);
				printf("-------------------------------------------\n");
			}
		} else {
			printf("No Type\n");
			printf("-------------------------------------------\n");
		}
		return;
	}
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

void packet_to_hex(const unsigned char * packet, size_t packet_size, char * hex_string) {
	packet_size = packet_size + sizeof(csp_id_t);

	for (size_t i = 0; i < packet_size; i++) {
		sprintf(hex_string + (i * 2), "%02X", packet[i]);
	}
}

// void send_csp_packet_via_lora(const unsigned char *packet, size_t packet_size) {
//     // Calculate packet size (header + data)
//     packet_size = packet_size + sizeof(csp_id_t);

//     // Show the serialized packet before sending (Header + Data)
//     printf("\nSerialized CSP Packet (Hex): ");
//     for (size_t i = 0; i < packet_size; i++) {
//         printf("%02X", ((uint8_t *)packet)[i]);
//     }
//     printf("\n");

//     // Send the serialized data via LoRa (assuming LoRa is already initialized)
//     // LoRa.beginPacket();
//     // LoRa.write((uint8_t *)packet, packet_size);  // Send the full packet
//     // LoRa.endPacket();
// }

void convert_hex_to_packet(const char * hex_string, unsigned char * packet, size_t * packet_size) {
	size_t len = strlen(hex_string);
	size_t packet_idx = 0;

	// Iterate over the hex string in 8-character (32-bit) chunks
	for (size_t i = 0; i < len; i += 8) {
		char temp[5] = {0};  // Store 4 hex characters (16 bits) + null terminator

		// Copy only the first 4 hex characters (16 bits) and skip the next 4 (padding)
		strncpy(temp, &hex_string[i], 4);

		// Convert the hex string to an integer value and store it in the packet array
		packet[packet_idx++] = (unsigned char)strtol(temp, NULL, 16);
	}

	// Set the packet size based on the actual number of bytes filled
	*packet_size = packet_idx;
}

void reverse_byte_order(char * hex_chunk) {
	for (int i = 0; i < 4; i++) {
		// Swap the pairs of characters (e.g. "D2" <-> "04")
		char temp1 = hex_chunk[i * 2];
		char temp2 = hex_chunk[i * 2 + 1];
		hex_chunk[i * 2] = hex_chunk[(3 - i) * 2];
		hex_chunk[i * 2 + 1] = hex_chunk[(3 - i) * 2 + 1];
		hex_chunk[(3 - i) * 2] = temp1;
		hex_chunk[(3 - i) * 2 + 1] = temp2;
	}
}

unsigned int convert_hex_to_decimal_little_endian(const char * hex_chunk) {
	// Ensure that the input is 8 characters (32 bits)
	if (strlen(hex_chunk) != 8) {
		fprintf(stderr, "Invalid hex chunk length. Expected 8 characters (32 bits).\n");
		return 0;
	}

	// Create an array to hold the bytes in little-endian order
	char little_endian[9] = {0};  // 8 characters + null terminator

	// Rearrange the hex string from little-endian order
	little_endian[0] = hex_chunk[6];
	little_endian[1] = hex_chunk[7];
	little_endian[2] = hex_chunk[4];
	little_endian[3] = hex_chunk[5];
	little_endian[4] = hex_chunk[2];
	little_endian[5] = hex_chunk[3];
	little_endian[6] = hex_chunk[0];
	little_endian[7] = hex_chunk[1];

	// Convert the rearranged hex string to an unsigned integer
	return (unsigned int)strtoul(little_endian, NULL, 16);
}

void convert_hex_string_to_decimal(const char * hex_string, csp_packet_t * packet) {
	size_t len = strlen(hex_string);
	size_t chunk_count = 0;  // Counter for the number of 32-bit chunks processed
	size_t packet_idx = 0;

	// Iterate over the hex string in 8-character (32-bit) chunks
	for (size_t i = 0; i < len && chunk_count < 3; i += 8) {
		char temp[9] = {0};  // Buffer for 8 hex characters + null terminator

		// Copy 8 hex characters (32 bits) from the hex string
		strncpy(temp, &hex_string[i], 8);

		// Convert the hex chunk to decimal considering little-endian order
		unsigned int decimal_value = convert_hex_to_decimal_little_endian(temp);

		// Print the decimal value
		printf("32-bit chunk: %s -> Decimal: %u\n", temp, decimal_value);

		memcpy(&packet->data[packet_idx], &decimal_value, sizeof(unsigned int));
		packet_idx += sizeof(unsigned int);  // ขยับไปยังตำแหน่งถัดไปใน packet

		chunk_count++;  // Increment the chunk counter
	}
	printf("packet : %u\n", packet->data);
}

// void convert_hex_to_decimal_little_endian(const char *hex_string) {
//     size_t len = strlen(hex_string);

//     // Iterate over the hex string in 8-character (32-bit) chunks
//     for (size_t i = 0; i < len; i += 8) {
//         char temp[9] = {0};  // Store 8 hex characters (32 bits) + null terminator

//         // Copy 8 hex characters (32 bits) from the hex string
//         strncpy(temp, &hex_string[i], 8);

//         // Reverse the byte order for little-endian interpretation
//         reverse_byte_order(temp);

//         // Convert the 32-bit hex chunk to an unsigned integer
//         unsigned int decimal_value = (unsigned int)strtoul(temp, NULL, 16);

//         // Print the decimal value
//         printf("32-bit chunk (little-endian): %s -> Decimal: %u\n", temp, decimal_value);
//     }
// }

// void convert_hex_to_decimal(const char *hex_string) {
//     size_t len = strlen(hex_string);

//     // Iterate over the hex string in 8-character (32-bit) chunks
//     for (size_t i = 0; i < len; i += 8) {
//         char temp[9] = {0};  // Store 8 hex characters (32 bits) + null terminator

//         // Copy 8 hex characters (32 bits) from the hex string
//         strncpy(temp, &hex_string[i], 8);

//         // Convert the 32-bit hex chunk to an unsigned integer
//         unsigned int decimal_value = (unsigned int)strtoul(temp, NULL, 16);

//         // Print the decimal value
//         printf("32-bit chunk: %s -> Decimal: %u\n", temp, decimal_value);
//     }
// }

void hexfirst(unsigned int value, unsigned int * combinedValue) {
	unsigned int shiftedValue = value >> 16;  // Shift ค่าไป 16 บิต
	*combinedValue = shiftedValue & 0xFFFF;   // ใช้ bitmask เพื่อเก็บ 16 บิตสุดท้าย
}

void hexsec(unsigned int value, unsigned int * combinedValue) {
	*combinedValue = value & 0xFFFF;
}

void splitBits(uint32_t value, uint8_t * bitArray) {
	for (int i = 0; i < 32; i++) {
		bitArray[i] = (value >> (31 - i)) & 1;
	}
	for (int i = 0; i < 32; i++) {
		printf("%u", bitArray[i]);
	}
	printf("\n");
}

void toBinaryString(unsigned int num, char * binaryStr) {
	for (int i = 15; i >= 0; i--) {
		// กำหนดค่าบิตให้กับสตริง
		binaryStr[15 - i] = (num >> i) & 1 ? '1' : '0';
	}
	binaryStr[16] = '\0';  // เพิ่ม null terminator ที่ส่วนท้าย
}

int main(int argc, char * argv[]) {
	int ret = EXIT_SUCCESS;
	int type, mdid, req_id;
	// uint16_t header1[16];
	// uint16_t header2[16];
	// uint16_t header3[16];
	// aaa[0] --> aaa[15]

	uint16_t bitpacket[15];

	// typedef struct {
	// 	uint8_t bittype[32];
	// 	uint8_t bitmdid[32];
	// 	uint8_t bitreq_id[32];
	// 	uint8_t bitparam[32];
	// 	uint8_t bitval[32];
	// } bitmsg;

	//bitmsg sendbit = {0};
	csp_print("Initialising CSP\n");

	/* Init CSP */
	csp_init();

	/* Start router */
	//router_start();

	// csp_iface_t * kiss_iface = NULL;
	// csp_usart_conf_t uart_conf = {
	// 	.device = "/dev/serial0",
	// 	.baudrate = 115200, /* supported on all platforms */
	// 	.databits = 8,
	// 	.stopbits = 1,
	// 	.paritysetting = 0,
	// };
	// // open UART and ADD Interface
	// int result = csp_usart_open_and_add_kiss_interface(&uart_conf, CSP_IF_KISS_DEFAULT_NAME, addr, &kiss_iface);  // เพิ่ม addr
	// if (result != CSP_ERR_NONE) {
	// 	printf("Error adding KISS interface: %d\n", result);
	// 	return result;
	// } else {
	// 	printf("KISS interface added successfully\n");
	// }

	// kiss_iface->addr = client_address;
	// kiss_iface->is_default = 1;

	csp_print("Client started\n");

	// csp_print("Connection table\r\n");
	// csp_conn_print_table();

	// csp_print("Interfaces\r\n");
	// csp_iflist_print();

	while (1) {
		// int result = csp_ping(server_address, 1000, 100, CSP_O_NONE);
		// if (result < 0) {
		// 	csp_print("Ping failed for address %u: %d\n", server_address, result);
		// } else {
		// 	csp_print("Ping succeeded for address %u: %d [mS]\n", server_address, result);
		// }

		//        csp_reboot(server_address);
		//		    csp_print("reboot system request sent to address: %u\n", server_address);
		/* 1. Connect to host on 'server_address', port SERVER_PORT with regular UDP-like protocol and 1000 ms timeout */
		// csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, server_address, SERVER_PORT, 1000, CSP_O_NONE);
		// if (conn == NULL) {
		// 	/* Connect failed */
		// 	csp_print("Connection failed\n");
		// 	ret = EXIT_FAILURE;
		// 	break;
		// }

		do {
			printf("Enter Type ID : ");
			if (scanf("%d", &type) != 1) {
				printf("Invalid input.\n");
				while (getchar() != '\n');  // Clear the input buffer
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

		send_msg.type = (unsigned int)type;
		send_msg.mdid = (unsigned int)mdid;
		send_msg.req_id = (unsigned int)req_id;

		// for (int i = 0; i < 32; i++) {
		// sendbit.bittype[i] = (send_msg.type >> (31 - i)) & 1;  // แยกบิตทีละบิต (จากบิตที่มากไปน้อย)
		// }
		// // แสดงผลแต่ละบิตใน sendbit.bittype
		// for (int i = 0; i < 32; i++) {
		// 	printf("%u", sendbit.bittype[i]);
		// }
		// printf("\n");

		// for (int i = 0; i < 32; i++) {
		// sendbit.bitmdid[i] = (send_msg.mdid >> (31 - i)) & 1;  // แยกบิตทีละบิต (จากบิตที่มากไปน้อย)
		// }
		// // แสดงผลแต่ละบิตใน sendbit.bittype
		// for (int i = 0; i < 32; i++) {
		// 	printf("%u", sendbit.bitmdid[i]);
		// }
		// printf("\n");

		// for (int i = 0; i < 32; i++) {
		// sendbit.bitreq_id[i] = (send_msg.req_id >> (31 - i)) & 1;  // แยกบิตทีละบิต (จากบิตที่มากไปน้อย)
		// }
		// // แสดงผลแต่ละบิตใน sendbit.bittype
		// for (int i = 0; i < 32; i++) {
		// 	printf("%u", sendbit.bitreq_id[i]);
		// }
		// printf("\n");

		// for (int i = 0; i < 32; i++) {
		// sendbit.bitparam[i] = (send_msg.param >> (31 - i)) & 1;  // แยกบิตทีละบิต (จากบิตที่มากไปน้อย)
		// }
		// // แสดงผลแต่ละบิตใน sendbit.bittype
		// for (int i = 0; i < 32; i++) {
		// 	printf("%u", sendbit.bitparam[i]);
		// }
		// printf("\n");

		// for (int i = 0; i < 32; i++) {
		// sendbit.bitval[i] = (send_msg.val >> (31 - i)) & 1;  // แยกบิตทีละบิต (จากบิตที่มากไปน้อย)
		// }
		// // แสดงผลแต่ละบิตใน sendbit.bittype
		// for (int i = 0; i < 32; i++) {
		// 	printf("%u", sendbit.bitval[i]);
		// }
		// printf("\n");

		// send_msg.mdid;
		// send_msg.req_id;

		if (send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 1 || send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 2) {
			int delay;
			do {
				printf("Enter delay in seconds : ");
				scanf("%d", &delay);
				while (getchar() != '\n');
				if (delay < 0) {
					printf("Invalid input\n");
				}
			} while (delay < 0);
			send_msg.param = (unsigned int)delay;
		}

		if (send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 5) {
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
		if (send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 6) {
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

		if (send_msg.type == TC_REQUEST && send_msg.mdid == 1 && send_msg.req_id == 7) {
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
		//printf("type size : %u\n", sizeof(send_msg.type));

		// splitBits(send_msg.type, sendbit.bittype);
		// splitBits(send_msg.mdid, sendbit.bitmdid);
		// splitBits(send_msg.req_id, sendbit.bitreq_id);
		// splitBits(send_msg.param, sendbit.bitparam);
		// splitBits(send_msg.val, sendbit.bitval);

		// printf("type : %u\n", sendbit.bittype[31]);
		// printf("mdid : %u\n", sendbit.bitmdid[31]);
		// printf("req_id : %u\n", sendbit.bitreq_id[31]);
		// printf("param : %u\n", sendbit.bitparam[31]);
		// printf("val : %u\n", sendbit.bitval[31]);

		/* 2. Get packet buffer for message/data */
		csp_packet_t * packet = csp_buffer_get(0);
		if (packet == NULL) {
			/* Could not get buffer element */
			csp_print("Failed to get CSP buffer\n");
			ret = EXIT_FAILURE;
			break;
		}

		packet->id.pri = 2;     // Priority
		packet->id.dst = 3;     // Destination address
		packet->id.src = 1;     // Source address
		packet->id.dport = 5;   // Destination port
		packet->id.sport = 10;  // Source port
		packet->id.flags |= (CSP_FCRC32);

		//(CSP_FHMAC | CSP_FCRC32)

		// char binaryStr[17];  // สตริงเก็บเลขฐาน 2 ขนาด 17 (16 บิต + null terminator)
		// unsigned int binaryValue = 0;

		// toBinaryString(packet->id.dst, binaryStr); // แปลงเป็นเลขฐาน 2 และเก็บใน binaryStr
		// printf("Binary representation of packet->id.dst (%u): %s\n", packet->id.dst, binaryStr);

		// for (int i = 0; i < 16; i++) {
		// binaryValue <<= 1; // เลื่อนบิตไปทางซ้าย
		// if (packet->id.pri & (1 << (15 - i))) { // ถ้า bit ที่ i เป็น 1
		//     binaryValue |= 1; // ทำ OR เพื่อรวมบิต
		// }
		// }

		// printf("Original value (%u) in binary: ", packet->id.pri);
		// for (int i = 15; i >= 0; i--) {
		// 	printf("%u", (packet->id.pri >> i) & 1);
		// }
		// printf("\n");

		// packet->id.pri = binaryValue;

		// unsigned int shifthd1;     // ตัวแปรสำหรับเก็บค่าที่ถูก shift
		// unsigned int combinedValue;
		// // array สำหรับเก็บ 16 บิต

		// // Shift บิต 2 บิตของ dst ไปทางซ้าย 14 บิต
		// shifthd1 = packet->id.pri << 14;
		// combinedValue = shifthd1 | (packet->id.dst & 0x3FFF);  // ใช้ & กับ 0x3FFF เพื่อดึงเฉพาะ 14 บิต
		// // นำบิตแต่ละตัวจาก shiftedDst ไปใส่ใน array 16 บิต
		// for (int i = 0; i < 16; i++) {
		// 	packet[0] = (combinedValue >> (15 - i)) & 1;
		// }
		// printf("16-bit array: ");
		// for (int i = 0; i < 16; i++) {
		// 	printf("%d", packet[0]);
		// }
		// printf("\n");

		// unsigned int shiftedhd2;
		// unsigned int combinedValue2;

		// shiftedhd2 = packet->id.src << 2;
		// combinedValue = shiftedhd2 | (packet->id.dport & 0x3) >> 4;
		// for (int i = 0; i < 16; i++) {
		// 	packet[1] = (combinedValue >> (15 - i)) & 1;
		// }
		// printf("16-bit array: ");
		// for (int i = 0; i < 16; i++) {
		// 	printf("%d", packet[1]);
		// }
		// printf("\n");

		unsigned int shifthd1;  // ตัวแปรสำหรับเก็บค่าที่ถูก shift
		unsigned int combinedValue;
		// array สำหรับเก็บ 16 บิต

		// Shift บิต 2 บิตของ dst ไปทางซ้าย 14 บิต
		shifthd1 = packet->id.pri << 14;
		combinedValue = shifthd1 | (packet->id.dst & 0x3FFF);  // ใช้ & กับ 0x3FFF เพื่อดึงเฉพาะ 14 บิต
		bitpacket[0] = combinedValue;
		//printf("packet[0] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[0] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[0] as hexadecimal: 0x%04X\n", bitpacket[0]);

		unsigned int combinedValue2;
		unsigned int shiftedhd2;
		shiftedhd2 = packet->id.src << 2;
		combinedValue2 = shiftedhd2 | (packet->id.dport & 0x3) >> 4;
		bitpacket[1] = combinedValue;
		// เก็บค่า combinedValue ทั้งหมดใน packet[2] โดยตรง
		bitpacket[1] = combinedValue2;

		//printf("packet[1] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[1] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[1] as hexadecimal: 0x%04X\n", bitpacket[1]);

		unsigned int combinedValue3;
		unsigned int shiftedhd3;
		shiftedhd3 = packet->id.dport & 0xF;                                                    // เก็บ 4 บิตจาก dport
		unsigned int shiftedsport = packet->id.sport & 0x3F;                                    // เก็บ 6 บิตจาก sport
		combinedValue3 = (shiftedhd3 << 12) | (shiftedsport << 6) | (packet->id.flags & 0x3F);  // รวมบิต 16 บิต

		// เก็บค่า combinedValue ทั้งหมดใน packet[2] โดยตรง
		bitpacket[2] = combinedValue3;

		//printf("packet[2] as binary: ");
		for (int i = 15; i >= 0; i--) {
			//printf("%u", (bitpacket[2] >> i) & 1);  // แสดงบิตทีละบิต
			if (i == 0 && (bitpacket[2] >> i) & 1 == 1) {
				//printf("\nCRC enable\n");
				CRCflags = 1;
			}
		}
		//printf("\n");
		//printf("packet[2] as hexadecimal: 0x%04X\n", bitpacket[2]);

		unsigned int combinedValue4;
		hexfirst(send_msg.type, &combinedValue4);
		//get_16bitLR(send_msg.type, 0);
		//printf("Left : %u\n",get_16bitLR(send_msg.type, 0));

		// unsigned int shifttype;
		// shifttype = send_msg.type >>16;
		// unsigned int combinedValue4 = shifttype & 0xFFFF;
		bitpacket[3] = combinedValue4;

		//printf("packet[3] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[3] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[3] as hexadecimal: 0x%04X\n", bitpacket[3]);

		// shifttype = send_msg.type & 0xFFFF;

		hexsec(send_msg.type, &combinedValue4);

		bitpacket[4] = combinedValue4;
		//printf("packet[4] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[4] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[4] as hexadecimal: 0x%04X\n", bitpacket[4]);

		// unsigned int shiftmdid;
		// shiftmdid = send_msg.type >>16;
		// unsigned int combinedValue5 = shiftmdid & 0xFFFF;
		// bitpacket[5] = combinedValue5;

		// printf("packet[5] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	printf("%u", (bitpacket[5] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		// printf("\n");

		hexfirst(send_msg.mdid, &combinedValue4);
		bitpacket[5] = combinedValue4;

		//printf("packet[5] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[5] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[5] as hexadecimal: 0x%04X\n", bitpacket[5]);

		hexsec(send_msg.mdid, &combinedValue4);

		bitpacket[6] = combinedValue4;
		//printf("packet[6] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[6] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[6] as hexadecimal: 0x%04X\n", bitpacket[6]);

		hexfirst(send_msg.req_id, &combinedValue4);
		bitpacket[7] = combinedValue4;

		//printf("packet[7] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[7] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[7] as hexadecimal: 0x%04X\n", bitpacket[7]);

		hexsec(send_msg.req_id, &combinedValue4);

		bitpacket[8] = combinedValue4;
		//printf("packet[8] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[8] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[8] as hexadecimal: 0x%04X\n", bitpacket[8]);

		hexfirst(send_msg.param, &combinedValue4);
		bitpacket[9] = combinedValue4;
		//printf("Left : %u\n",get_16bitLR(send_msg.param, 0));
		//printf("packet[9] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[9] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[9] as hexadecimal: 0x%04X\n", bitpacket[9]);

		hexsec(send_msg.param, &combinedValue4);
		//printf("Right : %u\n",get_16bitLR(send_msg.param, 1));

		bitpacket[10] = combinedValue4;
		//printf("packet[10] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[10] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[9] as hexadecimal: 0x%04X\n", bitpacket[10]);
		hexfirst(send_msg.val, &combinedValue4);
		bitpacket[11] = combinedValue4;

		//printf("packet[11] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[11] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[11] as hexadecimal: 0x%04X\n", bitpacket[11]);
		hexsec(send_msg.val, &combinedValue4);

		bitpacket[12] = combinedValue4;
		//printf("packet[12] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	//printf("%u", (bitpacket[12] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		//printf("\n");
		//printf("packet[12] as hexadecimal: 0x%04X\n", bitpacket[12]);
		// shifttype = send_msg.type & 0xFFFF;
		// bitpacket[4] = shifttype;

		// printf("packet[4] as binary: ");
		// for (int i = 15; i >= 0; i--) {
		// 	printf("%u", (bitpacket[4] >> i) & 1);  // แสดงบิตทีละบิต
		// }
		// printf("\n");
		// unsigned long crc = crc32(0L, Z_NULL, 0);

		// // คำนวณค่า CRC32 ของข้อมูลตั้งแต่ bitpacket[0] ถึง bitpacket[12]
		// crc = crc32(crc, (const unsigned char *)bitpacket, 13 * sizeof(uint16_t));  // แคสต์ให้เป็น unsigned char*

		// // แสดงผลค่า CRC32
		// printf("CRC32: %lx\n", crc);  // แสดงผลเป็นเลขฐาน 16 (hexadecimal)

		char hexString[60];
		hexString[0] = '\0';  // เริ่มต้นสตริงให้ว่าง
		for (int i = 0; i < 13; i++) {
			char temp[10];                        // สตริงชั่วคราวเพื่อเก็บค่าฐานสิบหก
			sprintf(temp, "%04X", bitpacket[i]);  // แปลงเป็นฐานสิบหก (4 หลัก)
			strcat(hexString, temp);              // ต่อสตริง
		}

		//printf("Combined hexadecimal string: %s\n", hexString);

		unsigned long crc = crc32(0L, Z_NULL, 0);  // เริ่มต้นค่า CRC32
		//printf("CRC32: %lx\n", crc);
		if (CRCflags == 1) {
			//printf("\nCRC enable\n");
			// คำนวณค่า CRC32 ของข้อมูล
			crc = crc32(crc, hexString, strlen(hexString));
			// แสดงผลค่า CRC32
			//printf("CRC32: %lX\n", crc);  // แสดงผลค่าเป็นเลขฐาน 16 (hex)
			char crcString[20];
			sprintf(crcString, "%lX", crc);  // แปลงเป็นเลขฐาน 16 (hexadecimal)

			strcat(hexString, crcString);
			
		}
		

		// unsigned int shiftedhd3;
		// unsigned int combinedValue3;
		// //// แก้เลขให้ shift ให้ตรง
		// //shiftedhd3 = packet->id.dport << 2;
		// shiftedhd3 = packet->id.dport & 0xF;
		// unsigned int shiftedsport;
		// //shiftedsport = packet->id.sport << 2;

		// shiftedsport = packet->id.sport & 0x3F;
		// combinedValue = shiftedhd3 << 12 | (shiftedsport << 6) | packet->id.flags & 0x3F;
		// for (int i = 0; i < 16; i++) {
		// 	packet[2] = (combinedValue >> (15 - i)) & 1;
		// }
		// printf("16-bit array: ");
		// for (int i = 0; i < 16; i++) {
		// 	printf("%d", packet[0]);
		// }
		// printf("\n");

		/* 3. Copy data to packet */
		memcpy(packet->data, &send_msg, sizeof(send_msg));

		// for (int i = 0; i < sizeof(packet->data); i++) {
		// 	printf("%u", packet->data[i]);
		// }
		// memcmp(packet->id, &packet->id,sizeof(csp_id_t));
		//printf("packet : %u\n", packet->data);
		// csp_print("pri : %u\n", packet->id.pri);
		// csp_print("dst: %u\n", packet->id.dst);
		// csp_print("src: %u\n", packet->id.src);
		// csp_print("dport: %u\n", packet->id.dport);
		// csp_print("sport: %u\n", packet->id.sport);
		// csp_print("flags: %u\n", packet->id.flags);

		// Print the size of each field
		// printf("Size of pri: %lu\n", sizeof(id.pri));
		// printf("Size of flags: %lu\n", sizeof(id.flags));
		// printf("Size of src: %lu\n", sizeof(id.src));
		// printf("Size of dst: %lu\n", sizeof(id.dst));
		// printf("Size of dport: %lu\n", sizeof(id.dport));
		// printf("Size of sport: %lu\n", sizeof(id.sport));

		// Print the total size of the struct
		// printf("Size of csp_id_t: %lu\n", sizeof(csp_id_t));
		// printf("Size of sendbit: %u\n", sizeof(send_msg));

		/* 4. Set packet length */
		packet->length = sizeof(send_msg) + sizeof(csp_id_t);

		// for(int i=0 ;i<sizeof(packet->data);i++){
		//   printf("%u", packet->data[i]);
		// }

		// Send and display the serialized packet (including header)
		// send_csp_packet_via_lora(packet->data, packet->length);
		// printf("packet : %u\n", packet->data[0]);
		// printf("packet : %u\n", packet->data[1]);
		// printf("packet : %u\n", packet->data[2]);
		// char hex_string[packet->length * 2 + 1];  // ต้องรองรับ 2 เท่าของขนาด packet + null terminator
		// packet_to_hex(packet->data, packet->length, hex_string);
		// char id_hex[64];  // Buffer สำหรับเก็บค่า hex ของ packet->id
		// sprintf(id_hex, "%02X%02X%02X%02X%02X%02X",
		// 		packet->id.pri,
		// 		packet->id.dst,
		// 		packet->id.src,
		// 		packet->id.dport,
		// 		packet->id.sport,
		// 		packet->id.flags);

		// // 2. ต่อ header ที่แปลงแล้วกับ hex_string
		// strcat(id_hex, hex_string);
		// printf("Hex String: %s\n", hex_string);

		// convert_hex_string_to_decimal(hex_string, packet);
		// printf("packet : %u\n", packet->data);
		// memcpy(&receive_msg, packet->data, packet->length);
		// csp_print("Type : %u\n", receive_msg.type);
		// csp_print("Receive ModuleID : %u\n", receive_msg.mdid);
		// csp_print("Receive TelemetryID : %u\n", receive_msg.req_id);

		int uart0_filestream = open("/dev/serial0", O_WRONLY | O_NOCTTY);

		if (uart0_filestream == -1) {
			printf("Error - Unable to open UART.\n");
			return -1;
		}

		struct termios options;
		tcgetattr(uart0_filestream, &options);
		options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
		options.c_iflag = IGNPAR;
		options.c_oflag = 0;
		options.c_lflag = 0;
		tcflush(uart0_filestream, TCIFLUSH);
		tcsetattr(uart0_filestream, TCSANOW, &options);
		// size_t hex_length = packet->length * 2+1; // ขนาดของ hex string
		// size_t id_length = sizeof(id_hex);
		// int count = write(uart0_filestream, hex_string, hex_length);
		printf("Packet send : %s\n", hexString);
		int count = write(uart0_filestream, hexString, sizeof(hexString));
		//printf("ID + Hex String: %s\n", hexString);
		if (count < 0) {
			printf("UART TX error.\n");
		}

		if (tcdrain(uart0_filestream) != 0) {
			printf("Error draining UART.\n");
		}
		printf("-------------------------------------------\n");
		close(uart0_filestream);
		/* 5. Send packet */
		// csp_send(conn, packet);
		// csp_close(conn);
		server();
	}
	/* Wait for execution to end (ctrl+c) */
	return ret;
}
