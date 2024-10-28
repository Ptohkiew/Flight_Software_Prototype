// receiver
#include <csp/csp_debug.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <pthread.h>
#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include "/home/pi1/testproject/message.h"
#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/resource.h>
#include <time.h>
#include <termios.h>
#include <errno.h>
#include <zlib.h>

/* These three functions must be provided in arch specific way */
int router_start(void);
int server_start(void);

/* Server port, the port the server listens on for incoming connections from the client. */
#define SERVER_PORT 15

#ifndef RECEIVE_ID
#define RECEIVE_ID 3  // Default value if no value is passed during compilation
#endif

/* Commandline options */
static uint8_t server_address = 1;

/* Test mode, check that server & client can exchange packets */
static bool test_mode = false;
static unsigned int server_received = 0;
static unsigned int run_duration_in_sec = 3;
static unsigned int successful_ping = 0;

Message send_msg = {0};
Message receive_msg = {0};
mqd_t mqdes_dis, mqdes_tm, mqdes_tc, mqdes_obc;
int CRCflags;

csp_usart_conf_t uart_conf = {
	.device = "/dev/serial0",
	.baudrate = 115200, /* supported on all platforms */
	.databits = 8,
	.stopbits = 1,
	.paritysetting = 0,
};

csp_iface_t * kiss_iface = NULL;

// void *return_csp(void *arg){
//     csp_conn_t * conn2 = csp_connect(CSP_PRIO_NORM, server_address, SERVER_PORT, 1000, CSP_O_NONE);

//     if (conn2 == NULL) {
//         csp_print("Connection failed\n");
//         return NULL;
//     }

//     csp_packet_t * packet2 = csp_buffer_get(0);
//     if (packet2 == NULL) {
//         csp_print("Failed to get CSP buffer\n");
//         csp_close(conn2);
//         return NULL;
//     }

//     memcpy(packet2->data, &receive_msg, sizeof(receive_msg));
// //    int result = csp_ping(server_address, 5000, 100, CSP_O_NONE);
// //		csp_print("Ping address: %u, result %d [mS]\n", server_address, result);
// //        // Increment successful_ping if ping was successful
// //        if (result >= 0) {
// //            ++successful_ping;
// //        }
// //        csp_print("Connection table\r\n");
// //    csp_conn_print_table();
// //
// //    csp_print("Interfaces\r\n");
// //    csp_iflist_print();

//     //   駤   packet->length      ҡѺ  Ҵ ͧ send_msg
//     packet2->length = sizeof(receive_msg);

//     //   Ǩ ͺ packet->length   ѧ  駤
//     //csp_print("Packet length set to: %u\n", packet2->length);

//     csp_send(conn2, packet2);

//     csp_buffer_free(packet2);
//     csp_close(conn2);

//     return NULL;
// }

void packet_to_hex(const unsigned char * packet, size_t packet_size, char * hex_string) {
	packet_size = packet_size + sizeof(csp_id_t);

	for (size_t i = 0; i < packet_size; i++) {
		sprintf(hex_string + (i * 2), "%02X", packet[i]);
	}
}

void hexfirst(unsigned int value, unsigned int * combinedValue) {
	unsigned int shiftedValue = value >> 16;  // Shift ค่าไป 16 บิต
	*combinedValue = shiftedValue & 0xFFFF;   // ใช้ bitmask เพื่อเก็บ 16 บิตสุดท้าย
}

void hexsec(unsigned int value, unsigned int * combinedValue) {
	*combinedValue = value & 0xFFFF;
}

void * return_crc_false(uint16_t * crc_check) {
	uint16_t bitpacket2[15];
	csp_packet_t * packet2 = csp_buffer_get(0);
	//printf("Size of packet->data (from packet_length): %d bytes\n", packet2->length);
	if (packet2 == NULL) {
		csp_print("Failed to get CSP buffer\n");
		return NULL;
	}
	packet2->id.pri = 2;     // Priority
	packet2->id.dst = 1;     // Destination address
	packet2->id.src = 3;     // Source address
	packet2->id.dport = 5;   // Destination port
	packet2->id.sport = 10;  // Source port
	packet2->id.flags |= (CSP_FHMAC | CSP_FCRC32);

	unsigned int shifthd1;  // ตัวแปรสำหรับเก็บค่าที่ถูก shift
	unsigned int combinedValue;
	// array สำหรับเก็บ 16 บิต

	// Shift บิต 2 บิตของ dst ไปทางซ้าย 14 บิต
	shifthd1 = packet2->id.pri << 14;
	combinedValue = shifthd1 | (packet2->id.dst & 0x3FFF);  // ใช้ & กับ 0x3FFF เพื่อดึงเฉพาะ 14 บิต
	bitpacket2[0] = combinedValue;
	//printf("packet2[0] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[0] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[0] as hexadecimal: 0x%04X\n", bitpacket2[0]);

	unsigned int combinedValue2;
	unsigned int shiftedhd2;
	shiftedhd2 = packet2->id.src << 2;
	combinedValue2 = shiftedhd2 | (packet2->id.dport & 0x3) >> 4;
	bitpacket2[1] = combinedValue;
	// เก็บค่า combinedValue ทั้งหมดใน packet2[2] โดยตรง
	bitpacket2[1] = combinedValue2;

	//printf("packet2[1] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[1] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	// printf("\n");
	//printf("packet2[1] as hexadecimal: 0x%04X\n", bitpacket2[1]);

	unsigned int combinedValue3;
	unsigned int shiftedhd3;
	shiftedhd3 = packet2->id.dport & 0xF;                                                    // เก็บ 4 บิตจาก dport
	unsigned int shiftedsport = packet2->id.sport & 0x3F;                                    // เก็บ 6 บิตจาก sport
	combinedValue3 = (shiftedhd3 << 12) | (shiftedsport << 6) | (packet2->id.flags & 0x3F);  // รวมบิต 16 บิต

	// เก็บค่า combinedValue ทั้งหมดใน packet2[2] โดยตรง
	bitpacket2[2] = combinedValue3;

	//printf("packet[2] as binary: ");
		for (int i = 15; i >= 0; i--) {
			//printf("%u", (bitpacket2[2] >> i) & 1);  // แสดงบิตทีละบิต
			if (i == 0 && (bitpacket2[2] >> i) & 1 == 1) {
				//printf("\nCRC enable\n");
				CRCflags = 1;
			}
		}
	//printf("packet2[2] as hexadecimal: 0x%04X\n", bitpacket2[2]);

	unsigned int combinedValue4;
	hexfirst(send_msg.type, &combinedValue4);

	bitpacket2[3] = combinedValue4;

	//printf("packet2[3] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[3] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[3] as hexadecimal: 0x%04X\n", bitpacket2[3]);

	// shifttype = send_msg.type & 0xFFFF;

	hexsec(send_msg.type, &combinedValue4);

	bitpacket2[4] = combinedValue4;
	//printf("packet2[4] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[4] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[4] as hexadecimal: 0x%04X\n", bitpacket2[4]);

	hexfirst(send_msg.mdid, &combinedValue4);
	bitpacket2[5] = combinedValue4;

	//printf("packet2[5] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[5] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[5] as hexadecimal: 0x%04X\n", bitpacket2[5]);

	hexsec(send_msg.mdid, &combinedValue4);

	bitpacket2[6] = combinedValue4;
	//printf("packet2[6] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[6] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[6] as hexadecimal: 0x%04X\n", bitpacket2[6]);

	hexfirst(send_msg.req_id, &combinedValue4);
	bitpacket2[7] = combinedValue4;

	//printf("packet2[7] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[7] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[7] as hexadecimal: 0x%04X\n", bitpacket2[7]);

	hexsec(send_msg.req_id, &combinedValue4);

	bitpacket2[8] = combinedValue4;
	//printf("packet2[8] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[8] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[8] as hexadecimal: 0x%04X\n", bitpacket2[8]);

	hexfirst(send_msg.param, &combinedValue4);
	bitpacket2[9] = combinedValue4;

	//printf("packet2[9] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[9] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[9] as hexadecimal: 0x%04X\n", bitpacket2[9]);

	hexsec(send_msg.param, &combinedValue4);

	bitpacket2[10] = combinedValue4;
	//printf("packet2[10] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[10] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[9] as hexadecimal: 0x%04X\n", bitpacket2[10]);
	hexfirst(send_msg.val, &combinedValue4);
	bitpacket2[11] = combinedValue4;

	//printf("packet2[11] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[11] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[11] as hexadecimal: 0x%04X\n", bitpacket2[11]);
	hexsec(send_msg.val, &combinedValue4);

	bitpacket2[12] = combinedValue4;
	//printf("packet2[12] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[12] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[12] as hexadecimal: 0x%04X\n", bitpacket2[12]);

	char hexString[60];
	hexString[0] = '\0';  // เริ่มต้นสตริงให้ว่าง
	for (int i = 0; i < 13; i++) {
		char temp[10];                         // สตริงชั่วคราวเพื่อเก็บค่าฐานสิบหก
		sprintf(temp, "%04X", bitpacket2[i]);  // แปลงเป็นฐานสิบหก (4 หลัก)
		strcat(hexString, temp);               // ต่อสตริง
	}

	//printf("Combined hexadecimal string: %s\n", hexString);


	unsigned long crc = crc32(0L, Z_NULL, 0);  // เริ่มต้นค่า CRC32
		//printf("CRC32: %lx\n", crc);
		if (CRCflags == 1) {
			printf("\nCRC enable\n");
			// คำนวณค่า CRC32 ของข้อมูล
			crc = 99999999;
			// แสดงผลค่า CRC32
			printf("CRC32: %lX\n", crc);  // แสดงผลค่าเป็นเลขฐาน 16 (hex)
		}
		char crcString[20];
		sprintf(crcString, "%lX", crc);  // แปลงเป็นเลขฐาน 16 (hexadecimal)

		strcat(hexString, crcString);
		//printf("Combined hexadecimal + CRC32 : %s\n", hexString);
	/* 3. Copy data to packet2 */
	memcpy(packet2->data, &send_msg, sizeof(send_msg));

	// for(int i=0 ;i<sizeof(packet2->data);i++){
	//   printf("%u", packet2->data[i]);
	// }
	// memcmp(packet2->id, &packet2->id,sizeof(csp_id_t));
	// printf("packet2 : %u\n", packet2->data);
	// csp_print("pri : %u\n", packet2->id.pri);
	// csp_print("dst: %u\n", packet2->id.dst);
	// csp_print("src: %u\n", packet2->id.src);
	// csp_print("dport: %u\n", packet2->id.dport);
	// csp_print("sport: %u\n", packet2->id.sport);
	// csp_print("flags: %u\n", packet2->id.flags);

	// printf("Size of csp_id_t: %lu\n", sizeof(csp_id_t));
	// printf("Size of sendbit: %u\n", sizeof(send_msg));

	/* 4. Set packet2 length */
	packet2->length = sizeof(send_msg) + sizeof(csp_id_t);

	// char hex_string[packet2->length * 2 + 1];  // ต้องรองรับ 2 เท่าของขนาด packet2 + null terminator
	// packet_to_hex(packet2->data, packet2->length, hex_string);
	//  char id_hex[64]; // Buffer สำหรับเก็บค่า hex ของ packet->id
	//  sprintf(id_hex, "%02X%02X%02X%02X%02X%02X",
	//     packet2->id.pri,
	//     packet2->id.dst,
	// 	packet2->id.src,
	//     packet2->id.dport,
	//     packet2->id.sport,
	// 	packet2->id.flags);

	// 2. ต่อ header ที่แปลงแล้วกับ hex_string
	// strcat(id_hex, hex_string);
	// printf("Hex String: %s\n", hex_string);

	int uart0_filestream = open("/dev/serial0", O_WRONLY | O_NOCTTY);

	if (uart0_filestream == -1) {
		printf("Error - Unable to open UART.\n");
		return NULL;
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
	int count = write(uart0_filestream, hexString, sizeof(hexString));

	// printf("ID + Hex String: %u\n", sizeof(hexString));
	// printf("ID + Hex String: %s\n", hexString);
	if (count < 0) {
		printf("UART TX error.\n");
	}

	if (tcdrain(uart0_filestream) != 0) {
		printf("Error draining UART.\n");
	}

	close(uart0_filestream);
}

void * return_csp(void * arg) {
	uint16_t bitpacket2[15];
	csp_packet_t * packet2 = csp_buffer_get(0);
	//printf("Size of packet->data (from packet_length): %d bytes\n", packet2->length);
	if (packet2 == NULL) {
		csp_print("Failed to get CSP buffer\n");
		return NULL;
	}
	packet2->id.pri = 2;     // Priority
	packet2->id.dst = 1;     // Destination address
	packet2->id.src = 3;     // Source address
	packet2->id.dport = 5;   // Destination port
	packet2->id.sport = 10;  // Source port
	packet2->id.flags |= (CSP_FHMAC | CSP_FCRC32);

	unsigned int shifthd1;  // ตัวแปรสำหรับเก็บค่าที่ถูก shift
	unsigned int combinedValue;
	// array สำหรับเก็บ 16 บิต

	// Shift บิต 2 บิตของ dst ไปทางซ้าย 14 บิต
	shifthd1 = packet2->id.pri << 14;
	combinedValue = shifthd1 | (packet2->id.dst & 0x3FFF);  // ใช้ & กับ 0x3FFF เพื่อดึงเฉพาะ 14 บิต
	bitpacket2[0] = combinedValue;
	//printf("packet2[0] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[0] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[0] as hexadecimal: 0x%04X\n", bitpacket2[0]);

	unsigned int combinedValue2;
	unsigned int shiftedhd2;
	shiftedhd2 = packet2->id.src << 2;
	combinedValue2 = shiftedhd2 | (packet2->id.dport & 0x3) >> 4;
	bitpacket2[1] = combinedValue;
	// เก็บค่า combinedValue ทั้งหมดใน packet2[2] โดยตรง
	bitpacket2[1] = combinedValue2;

	//printf("packet2[1] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[1] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[1] as hexadecimal: 0x%04X\n", bitpacket2[1]);

	unsigned int combinedValue3;
	unsigned int shiftedhd3;
	shiftedhd3 = packet2->id.dport & 0xF;                                                    // เก็บ 4 บิตจาก dport
	unsigned int shiftedsport = packet2->id.sport & 0x3F;                                    // เก็บ 6 บิตจาก sport
	combinedValue3 = (shiftedhd3 << 12) | (shiftedsport << 6) | (packet2->id.flags & 0x3F);  // รวมบิต 16 บิต

	// เก็บค่า combinedValue ทั้งหมดใน packet2[2] โดยตรง
	bitpacket2[2] = combinedValue3;

	//printf("packet[2] as binary: ");
		for (int i = 15; i >= 0; i--) {
			//printf("%u", (bitpacket2[2] >> i) & 1);  // แสดงบิตทีละบิต
			if (i == 0 && (bitpacket2[2] >> i) & 1 == 1) {
				//printf("\nCRC enable\n");
				CRCflags = 1;
			}
		}
	//printf("packet2[2] as hexadecimal: 0x%04X\n", bitpacket2[2]);

	unsigned int combinedValue4;
	hexfirst(send_msg.type, &combinedValue4);

	bitpacket2[3] = combinedValue4;

	//printf("packet2[3] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[3] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[3] as hexadecimal: 0x%04X\n", bitpacket2[3]);

	// shifttype = send_msg.type & 0xFFFF;

	hexsec(send_msg.type, &combinedValue4);

	bitpacket2[4] = combinedValue4;
	//printf("packet2[4] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[4] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[4] as hexadecimal: 0x%04X\n", bitpacket2[4]);

	hexfirst(send_msg.mdid, &combinedValue4);
	bitpacket2[5] = combinedValue4;

	//printf("packet2[5] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[5] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[5] as hexadecimal: 0x%04X\n", bitpacket2[5]);

	hexsec(send_msg.mdid, &combinedValue4);

	bitpacket2[6] = combinedValue4;
	//printf("packet2[6] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	printf("%u", (bitpacket2[6] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[6] as hexadecimal: 0x%04X\n", bitpacket2[6]);

	hexfirst(send_msg.req_id, &combinedValue4);
	bitpacket2[7] = combinedValue4;

	//printf("packet2[7] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[7] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[7] as hexadecimal: 0x%04X\n", bitpacket2[7]);

	hexsec(send_msg.req_id, &combinedValue4);

	bitpacket2[8] = combinedValue4;
	//printf("packet2[8] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[8] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[8] as hexadecimal: 0x%04X\n", bitpacket2[8]);

	hexfirst(send_msg.param, &combinedValue4);
	bitpacket2[9] = combinedValue4;

	//printf("packet2[9] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[9] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[9] as hexadecimal: 0x%04X\n", bitpacket2[9]);

	hexsec(send_msg.param, &combinedValue4);

	bitpacket2[10] = combinedValue4;
	//printf("packet2[10] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[10] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[9] as hexadecimal: 0x%04X\n", bitpacket2[10]);
	hexfirst(send_msg.val, &combinedValue4);
	bitpacket2[11] = combinedValue4;

	//printf("packet2[11] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[11] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[11] as hexadecimal: 0x%04X\n", bitpacket2[11]);
	hexsec(send_msg.val, &combinedValue4);

	bitpacket2[12] = combinedValue4;
	//printf("packet2[12] as binary: ");
	// for (int i = 15; i >= 0; i--) {
	// 	//printf("%u", (bitpacket2[12] >> i) & 1);  // แสดงบิตทีละบิต
	// }
	//printf("\n");
	//printf("packet2[12] as hexadecimal: 0x%04X\n", bitpacket2[12]);

	char hexString[60];
	hexString[0] = '\0';  // เริ่มต้นสตริงให้ว่าง
	for (int i = 0; i < 13; i++) {
		char temp[10];                         // สตริงชั่วคราวเพื่อเก็บค่าฐานสิบหก
		sprintf(temp, "%04X", bitpacket2[i]);  // แปลงเป็นฐานสิบหก (4 หลัก)
		strcat(hexString, temp);               // ต่อสตริง
	}

	//printf("Combined hexadecimal string: %s\n", hexString);


	unsigned long crc = crc32(0L, Z_NULL, 0);  // เริ่มต้นค่า CRC32
		//printf("CRC32: %lx\n", crc);
		if (CRCflags == 1) {
			printf("CRC enable\n");
			// คำนวณค่า CRC32 ของข้อมูล
			crc = crc32(crc, hexString, strlen(hexString));
			// แสดงผลค่า CRC32
			printf("CRC32: %lX\n", crc);  // แสดงผลค่าเป็นเลขฐาน 16 (hex)
			char crcString[20];
			sprintf(crcString, "%lX", crc);  // แปลงเป็นเลขฐาน 16 (hexadecimal)

			strcat(hexString, crcString);
			printf("Combined hexadecimal + CRC32 : %s\n", hexString);
			printf("-------------------------------------------\n");
		}
		else{
			printf("Packet return : %s\n", hexString);
			printf("-------------------------------------------\n");
		}
		
	/* 3. Copy data to packet2 */
	memcpy(packet2->data, &send_msg, sizeof(send_msg));

	// for(int i=0 ;i<sizeof(packet2->data);i++){
	//   printf("%u", packet2->data[i]);
	// }
	// memcmp(packet2->id, &packet2->id,sizeof(csp_id_t));
	// printf("packet2 : %u\n", packet2->data);
	// csp_print("pri : %u\n", packet2->id.pri);
	// csp_print("dst: %u\n", packet2->id.dst);
	// csp_print("src: %u\n", packet2->id.src);
	// csp_print("dport: %u\n", packet2->id.dport);
	// csp_print("sport: %u\n", packet2->id.sport);
	// csp_print("flags: %u\n", packet2->id.flags);

	// printf("Size of csp_id_t: %lu\n", sizeof(csp_id_t));
	// printf("Size of sendbit: %u\n", sizeof(send_msg));

	/* 4. Set packet2 length */
	packet2->length = sizeof(send_msg) + sizeof(csp_id_t);

	// char hex_string[packet2->length * 2 + 1];  // ต้องรองรับ 2 เท่าของขนาด packet2 + null terminator
	// packet_to_hex(packet2->data, packet2->length, hex_string);
	//  char id_hex[64]; // Buffer สำหรับเก็บค่า hex ของ packet->id
	//  sprintf(id_hex, "%02X%02X%02X%02X%02X%02X",
	//     packet2->id.pri,
	//     packet2->id.dst,
	// 	packet2->id.src,
	//     packet2->id.dport,
	//     packet2->id.sport,
	// 	packet2->id.flags);

	// 2. ต่อ header ที่แปลงแล้วกับ hex_string
	// strcat(id_hex, hex_string);
	// printf("Hex String: %s\n", hex_string);

	int uart0_filestream = open("/dev/serial0", O_WRONLY | O_NOCTTY);

	if (uart0_filestream == -1) {
		printf("Error - Unable to open UART.\n");
		return NULL;
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
	int count = write(uart0_filestream, hexString, sizeof(hexString));

	// printf("ID + Hex String: %u\n", sizeof(hexString));
	// printf("ID + Hex String: %s\n", hexString);
	if (count < 0) {
		printf("UART TX error.\n");
	}

	if (tcdrain(uart0_filestream) != 0) {
		printf("Error draining UART.\n");
	}

	close(uart0_filestream);
}

void * msg_dis(void * arg) {
	mqdes_tm = mq_open("/mq_tm", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
	mqdes_tc = mq_open("/mq_tc", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);
	mqdes_obc = mq_open("/mq_obc", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);

	mqd_t mqdes_type = mq_open("/mq_ttctype", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);       // send to aocs_dispatcher
	mqd_t mq_return = mq_open("/mq_return_sender", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attributes);  // return from aocs_dispatcher

	if (mqdes_dis == -1 || mqdes_tm == -1 || mqdes_tc == -1) {
		perror("mq_open");
		pthread_exit(NULL);
	}

	send_msg.type = receive_msg.type;
	send_msg.mdid = receive_msg.mdid;
	send_msg.req_id = receive_msg.req_id;
	send_msg.param = receive_msg.param;
	if (send_msg.type == TM_REQUEST && send_msg.req_id > 10 && send_msg.req_id < 14){
			printf("- REQUEST NO TYPE 0-\n");
			printf("Type : %u\n", send_msg.type);
			printf("Receive ModuleID : %u\n", send_msg.mdid);
			printf("Receive TelemetryID : %u\n", send_msg.req_id);
			printf("No Type\n");
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			return_csp(NULL);
	}
	else if (send_msg.type == TM_REQUEST && send_msg.mdid == 1 && send_msg.req_id > 0 && send_msg.req_id <= 17) {
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
		else if (receive_msg.type == TM_RETURN) {
			printf("- RETURN TM -\n");
			printf("Type : %u\n", receive_msg.type);
			printf("Respond ModuleID : %u\n", receive_msg.mdid);
			printf("Respond TelemetryID : %u\n", receive_msg.req_id);
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			send_msg.val = receive_msg.val;
			return_csp(NULL);
		}
		else {
			printf("- REQUEST NO TYPE 1-\n");
			printf("Type : %u\n", send_msg.type);
			printf("Receive ModuleID : %u\n", send_msg.mdid);
			printf("Receive TelemetryID : %u\n", send_msg.req_id);
			printf("No Type\n");
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			return_csp(NULL);
		}
	} else if (send_msg.type == TM_REQUEST && send_msg.mdid == 3 && send_msg.req_id != 0) {
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
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			return_csp(NULL);
		}
	}

	else if (send_msg.type == TC_REQUEST && send_msg.mdid == 3 && send_msg.req_id != 0) {
		printf("- REQUEST TC AOCS-\n");
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
		if (receive_msg.type == TC_RETURN) {
			printf("- RETURN TC -\n");
			printf("Type : %u\n", receive_msg.type);
			printf("Respond ModuleID : %u\n", receive_msg.mdid);
			printf("Respond TelemetryID : %u\n", receive_msg.req_id);
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			return_csp(NULL);
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

		if (receive_msg.type == TC_RETURN && receive_msg.val == 1) {
			printf("- REJECT TC -\n");
			printf("Type : %u\n", receive_msg.type);
			printf("Respond ModuleID : %u\n", receive_msg.mdid);
			printf("Respond TelecommandID : %u\n", receive_msg.req_id);
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			send_msg.val =  receive_msg.val;
			return_csp(NULL);
		}

		else if (receive_msg.type == TC_RETURN) {
			printf("- RETURN TC -\n");
			printf("Type : %u\n", receive_msg.type);
			printf("Respond ModuleID : %u\n", receive_msg.mdid);
			printf("Respond TelecommandID : %u\n", receive_msg.req_id);
			printf("-------------------------------------------\n");
			send_msg.type = receive_msg.type;
			send_msg.mdid = receive_msg.mdid;
			send_msg.req_id = receive_msg.req_id;
			send_msg.val =  receive_msg.val;
			return_csp(NULL);
		}
	} else {
		printf("- REQUEST NO TYPE 2-\n");
		printf("Type : %u\n", send_msg.type);
		printf("Receive ModuleID : %u\n", send_msg.mdid);
		printf("Receive TCID/TMID : %u\n", send_msg.req_id);
		printf("No Type\n");
		printf("-------------------------------------------\n");
		send_msg.type = receive_msg.type;
		send_msg.mdid = receive_msg.mdid;
		send_msg.req_id = receive_msg.req_id;
		return_csp(NULL);
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

void convert_header_to_decimal(const char * hex_string, csp_packet_t * packet, uint16_t * destination, uint16_t * crc_check) {
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

	if (*destination != RECEIVE_ID) {
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

	//printf("alltemp : %s\n", alltemp);

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
			*crc_check = 1;
			printf("CRC equal!!\n");
			printf("-------------------------------------------\n");
		}
		else{
			*crc_check = 2;
			printf("CRC false!!\n");
			printf("-------------------------------------------\n");
		}
	}
}

void convert_hex_string_to_decimal(const char * hex_string, csp_packet_t * packet) {
	size_t len = strlen(hex_string);
	size_t chunk_count = 0;  // Counter for the number of 32-bit chunks processed
	size_t packet_idx = 0;

	// Iterate over the hex string in 8-character (32-bit) chunks
	for (size_t i = 12; i < len && chunk_count < 3; i += 8) {
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
		csp_conn_t * conn;
		if ((conn = csp_accept(&sock, 10000)) == NULL) {
			/* timeout */
			continue;
		}
		csp_packet_t * packet;

		while ((packet = csp_read(conn, 50)) != NULL) {
			switch (csp_conn_dport(conn)) {
				case SERVER_PORT:
					/* Process packet here */
					// csp_print("Packet received on SERVER_PORT: %s\n", (char *) packet->data);
					memcpy(&receive_msg, packet->data, packet->length);
					printf("packet : %u\n", packet->data);
					printf("pri: %u\n", packet->id.pri);
					printf("flags: %u\n", packet->id.flags);
					printf("src: %u\n", packet->id.src);
					printf("dst: %u\n", packet->id.dst);

					printf("dport: %u\n", packet->id.dport);
					printf("sport: %u\n", packet->id.sport);
					//          csp_print("Type : %u\n", receive_msg.type);
					//          csp_print("Receive ModuleID : %u\n", receive_msg.mdid);
					//          csp_print("Receive TelemetryID : %u\n", receive_msg.req_id);
					//          csp_print("Packet : %u\n",packet);
					//          csp_print("Packet length set to: %u\n", packet->length);
					//  				csp_buffer_free(packet);
					//  				++server_received;
					csp_close(conn);
					msg_dis(NULL);
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
	int ret = EXIT_SUCCESS;

	csp_print("Initialising CSP\n");

	/* Init CSP */
	csp_init();

	// int result = csp_usart_open_and_add_kiss_interface(&uart_conf, CSP_IF_KISS_DEFAULT_NAME, &kiss_iface);
	// if (result != CSP_ERR_NONE) {
	//     printf("Error adding KISS interface: %d\n", result);
	//     return result;
	// } else {
	//     printf("KISS interface added successfully\n");
	// }

	// kiss_iface->addr = server_address;
	// kiss_iface->is_default = 1;
	/* Start router */
	//router_start();

	// csp_print("Connection table\r\n");
	// csp_conn_print_table();

	// csp_print("Interfaces\r\n");
	// csp_iflist_print();

	csp_print("Server started\n");
	while (1) {

		while (1) {
			// server_start();
			int uart0_filestream = open("/dev/serial0", O_RDONLY | O_NOCTTY);

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

					// printf("Combined Received: %s\n", combined_buffer);  // แสดงข้อมูลที่รวม
					// printf("Combined Received no CRC : %s\n", combined_buffer);
				} else if (rx_length < 0) {
					printf("Error: %s\n", strerror(errno));
				}

				usleep(8000);
			}
			csp_packet_t * packet;
			size_t receive_msg_size = sizeof(receive_msg);
			packet->length = sizeof(receive_msg);
			// ขนาดของ packet->data (ไม่สามารถรู้ได้เพราะเป็น pointer แต่ใช้ packet_length แทน)
			// printf("Size of receive_msg: %zu bytes\n", receive_msg_size);
			// printf("Size of packet->data (from packet_length): %d bytes\n", packet->length);
			printf("Packet Received: %s\n", combined_buffer);
			uint16_t destination;
			uint16_t crc_check;
			convert_header_to_decimal(combined_buffer, packet, &destination, &crc_check);
			if (crc_check == 2) {
				printf("CRC False\n");
				return_crc_false(&crc_check);
				break;
			}
			if (destination != RECEIVE_ID) {
				printf("False destination : %u\n", destination);
				break;
			}
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
			if (destination == RECEIVE_ID) {
				msg_dis(NULL);
				break;
			}

			close(uart0_filestream);
			//printf("HI\n");
		}
		// while(1) {
		//     sleep(run_duration_in_sec);

		//     if (test_mode) {
		//         /* Test mode, check that server & client can exchange packets */
		//         if (server_received < 5) {
		//             csp_print("Server received %u packets\n", server_received);
		//             exit(EXIT_FAILURE);
		//         }
		//         csp_print("Server received %u packets\n", server_received);
		//         exit(EXIT_SUCCESS);
		//     }
		// }
	}
}
