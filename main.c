#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#include <stdio.h>
#include <wiringPi.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/*#define GPIO_LOCATION (0x3F200000)*/

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

typedef int bool;
#define true 1
#define false 0

static int CONTROLLER, HARDWARE;
static char *CONTROLLER_FILE;

static volatile unsigned int *gpio;
int gpioP(){
    int fd = open("/dev/gpiomem",O_RDWR);
    if(fd == -1){
        fprintf(stderr, "Error opening /dev/gpiomem\n");
        return -1;
    }

    gpio = (unsigned int *) mmap(NULL, 4096, PROT_READ + PROT_WRITE, MAP_SHARED, fd, 0);
    fprintf(stderr, "GPIO mapped %p\n", gpio);
    return 0;
}


//https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf
//
//The ARM has 32-bit registers and the main timer has a 64-bits wide changing
//value. There is the potential for reading the wrong value if the timer value
//changes between reading the LS and MS 32 bits. Therefore it has dedicated
//logic to give trouble free access to the 64-bit value.
//
//When reading the 64-bit timer value the user must always read the LS 32 bits
//first. At the same time the LS-32 bits are read, internal a copy is made of
//the MS-32 bits into a timer-read-hold register. When reading the timer MS-32
//bits actually the timer-read-hold register is read.

static volatile uint32_t *preciseTimerptr;
uint64_t readPrecise(){
    uint64_t time;
    time = *preciseTimerptr;
    time |= (uint64_t) *(preciseTimerptr + 1) << 32;
    return time;
}

#define QA7REGISTERS 0x40000000
#define DIVIDER_OFFSET 0x8
#define PRECISE_TIMER_OFFSET 0x1c

int allocHWTimer(){
    void *QA7Registers;
    int fd = open("/dev/mem", O_RDWR);
    if(fd == -1){
        fprintf(stderr, "open() failed.\n");
        return -1;
    }

    QA7Registers = mmap(NULL, 4096, PROT_READ + PROT_WRITE, MAP_SHARED, fd, QA7REGISTERS);
    if(MAP_FAILED == QA7Registers){
        fprintf(stderr, "mmap() failed.\n");
        return -1;
    }

    //https://www.raspberrypi.org/documentation/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf
    //page 4
    volatile uint32_t *divider = (uint32_t *) ((char *) QA7Registers + DIVIDER_OFFSET);
    fprintf(stderr, "divider changed %x -> ", *divider);
    *divider = 0x06AAAAAA;
    fprintf(stderr, "%x\n", *divider);

    preciseTimerptr = (uint32_t *) ((char *) QA7Registers + PRECISE_TIMER_OFFSET);
    fprintf(stderr, "GPIO mapped %p\n", preciseTimerptr);
    return 0;
}

#define pinoff(hardware) gpio[10] |= 1 << hardware;
#define pinon(hardware) gpio[7] |= 1 << hardware;
#define NOOP asm volatile("nop");

void waitOneUS(){
    uint64_t time = readPrecise();
    while(readPrecise() == time);
}

uint64_t waitOneUSTime(){
    uint64_t time = readPrecise();
    while(readPrecise() == time);
    return time + 1;
}

void sendBit(int n){
    if(n){
        pinoff(HARDWARE);
        waitOneUS();
        pinon(HARDWARE);
        waitOneUS();
        waitOneUS();
        waitOneUS();
    }else{
        pinoff(HARDWARE);
        waitOneUS();
        waitOneUS();
        waitOneUS();
        pinon(HARDWARE);
        waitOneUS();
    }
}

void sendByte(unsigned char b){
    sendBit(b & (1 << 7));
    sendBit(b & (1 << 6));
    sendBit(b & (1 << 5));
    sendBit(b & (1 << 4));
    sendBit(b & (1 << 3));
    sendBit(b & (1 << 2));
    sendBit(b & (1 << 1));
    sendBit(b & (1 << 0));
}

/*
   struct {
       unsigned char connected;
       unsigned char controller_state[8];
       unsigned char hash;
       unsigned char extra;
   }

 * Hash is already checked before this program *
 */

#define CONTROLLER_INPUT_SIZE 11
static uint8_t data[CONTROLLER_INPUT_SIZE];

//Hasgamed can be one of 3 states:
//  0: Send first controller info: 0x9 0x0 0x0
//  1: Send controller origin packet, with deadzone info (TODO improvements)
//  2: Send controller data
uint64_t gamerTime(int hasgamed){
	pinMode(CONTROLLER, OUTPUT);
    //prevent accidental 1 bit being written
    pinon(HARDWARE);
    //Wait to be on a boundary so that our timing can be a bit more accurate
    uint64_t start = waitOneUSTime();

    switch(hasgamed){
        case 2:
            sendByte(data[1]);
            sendByte(data[2]);
            sendByte(data[3]);
            sendByte(data[4]);
            sendByte(data[5]);
            sendByte(data[6]);
            sendByte(data[7]);
            sendByte(data[8]);
            sendBit(1);
            break;
        case 1:
            sendByte(data[1]);
            sendByte(data[2]);
            sendByte(data[3]);
            sendByte(data[4]);
            sendByte(data[5]);
            sendByte(data[6]);
            sendByte(data[7]);
            sendByte(data[8]);
            sendByte(0x02);
            sendByte(0x02);
            sendBit(1);
            break;
        case 0:
            sendByte(0x09);
            sendByte(0x00);
            sendByte(0x00);
            sendBit(1);
            break;
    }

    start = readPrecise() - start;

    pinMode(CONTROLLER, INPUT);

    //read in controller state from file:
    //TODO mmap this
    FILE *ff = fopen(CONTROLLER_FILE, "rb");
    fread(data, CONTROLLER_INPUT_SIZE, 1, ff);
    fclose(ff);

    return start;
}


int main(int argc, char *argv[]) {
    if(argc != 4){
        fprintf(stderr, "bad args: ./xxx controllerFile wiringPiPin BCMPin");
        return 1;
    }

    CONTROLLER_FILE = argv[1];
    CONTROLLER = atoi(argv[2]);
    HARDWARE = atoi(argv[3]);

    fprintf(stderr, "controller %i\n", CONTROLLER);
    allocHWTimer();
    gpioP();

    uint64_t start;
    start = readPrecise();

	wiringPiSetup();
	pinMode(CONTROLLER, INPUT);
    fprintf(stderr, "setup took %lldus\n", readPrecise() - start);

    //this fork is for also reading from a probe pin (hw pin 26)
#ifdef FORK
    int cp = fork();
    if(cp > 0){
#endif

    //every tick of the loop, the controller will read in a bit and increment
    //either time1 or time0. On every falling edge, time1 and time0 are
    //compared to decide if the last sent bit was a 1, 0. Completion is marked
    //by 100 consecutive 1 bits, approx 6us of 1s. value is then read, switched
    //on, begins writing controller signals
    int time0=0, time1=0;
    int lastVal = 1 << HARDWARE;
    //the max byte length should be 3 (24 bits), but alloc some extra just in case
    bool data[32];
    int datap = 0;

    int stateLevel = 0;

    for(;;){
        int f = gpio[13] & (1 << HARDWARE);
        if(f){
            time1++;
            if(time1 == 100){
                char stuff = 0;
                for(int i = 1; i < datap; i++){
                    stuff |= data[i] << (8 - i);
                }

                uint64_t totalTime;
                start = readPrecise();
                if(unlikely(stuff == 0)){
                    totalTime = gamerTime(0);
                    fprintf(stderr, "\nnew connection request with state level %i\n", stateLevel++);
                }else if(unlikely(stuff == 0x41)){
                    totalTime = gamerTime(1);
                }else{
                    totalTime = gamerTime(2);
                }
                start = readPrecise() - start;
                if(totalTime > 262){
                fprintf(stderr, KNRM "req %x, %llius(taken) %llius(signal) " KRED " MISINPUT    \r", stuff, start, totalTime);
                }else if(start > 310){
                fprintf(stderr, KNRM "req %x, %llius(taken) %llius(signal) " KYEL " DISKIO?     \r", stuff, start, totalTime);
                }else{
                fprintf(stderr, KNRM "req %x, %llius(taken) %llius(signal) " KGRN "          OK \r", stuff, start, totalTime);
                }
                fprintf(stderr, KNRM);

                //reset data once we have written our state
                datap = 0;

                //we can yield for a short amount of time. 
                // ~5.8ms maximum - linux overhead
                // also apply downscaled timer here)
                const struct timespec ms5 = {
                    .tv_sec = 0,
                    .tv_nsec = 3e6/19.2,
                };
                nanosleep(&ms5, NULL);
            }
        }else{
            time0++;
        }

        if(f == lastVal) continue;
        lastVal = f;
        //wait for falling edge to decide bit
        if(!f){
            if(datap == 32){
                fprintf(stderr, "\n preventing segfault, datap too high :(\n");
                datap = 0;
            }
            data[datap++] = time0 < time1;
            time0 = 0;
            time1 = 0;
        }
    }

#ifdef FORK
    }else{
        /*while((gpio[13] & (1 << 26)) == 1);*/
#define maxN 1000000
        int vals[maxN];
        int i;
        for(i = 0; i < maxN; i++){
            vals[i] = gpio[13] & (1 << 26);
        }
        for(i = 0; i < maxN; i++){
            putc(vals[i] ? '1' : '0', stdout);
            if((i%100) == 99) putc('\n', stdout);
        }
        fprintf(stderr, "child ded\n;");
    }
#endif
    return 0;
}
