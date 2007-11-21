#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

/* sends a frame every 5 seconds with values the same as the examples
    in the documentation. Every 10 frames a bad frame is sent.
*/
/* there is a mistake in the example for character 3 */
unsigned char frame[28] = { 0xAA, 0xAA, 0xDB, 0x17, 0x37, 0x92, 0x1C, 0x39,
    0xA8, 0x50, 0x05, 0xC4, 0xFD, 0x83, 0xF2, 0x19, 0xF8, 0xE4,
    0xEF, 0xB3, 0xCF, 0xA5, 0x1F, 0xBE, 0xCF, 0xA5, 0x1F, 0xDE };


main(int argc, char **argv) {
int fd;
int i=0;
fd = open(argv[1], O_WRONLY);
while (1) {
    if (i%10 == 0) frame[0] = 0xFF;
    write(fd, frame, 28);
    frame[0] = 0xAA;
    sleep(5);
    i++;
}
}
