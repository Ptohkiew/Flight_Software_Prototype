#include <stdio.h>
#include <string.h>
#include <zlib.h>  // รวมไลบรารี zlib

int main() {
    const char *data = "Hello, World!";
    unsigned long crc = crc32(0L, Z_NULL, 0);  // เริ่มต้นค่า CRC32

    // คำนวณค่า CRC32 ของข้อมูล
    crc = crc32(crc, (const unsigned char *)data, strlen(data));

    // แสดงผลค่า CRC32
    printf("CRC32: %lx\n", crc);  // แสดงผลค่าเป็นเลขฐาน 16 (hex)

    return 0;
}
