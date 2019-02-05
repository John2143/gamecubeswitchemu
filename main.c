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

#define TIMER_BLOCK (0x3F003000)
#define TIMER_OFFSET (4)

#define GPIO_LOCATION (0x3F200000)

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

typedef int bool;
#define true 1
#define false 0

static int CONTROLLER, HARDWARE;

volatile uint64_t *getHWTimer(){
    int fd;
    void *timerBase;

    if (-1 == (fd = open("/dev/mem", O_RDONLY))) {
        fprintf(stderr, "open() failed.\n");
        return NULL;
    }

    if (MAP_FAILED == (timerBase = mmap(NULL, 4096,
                        PROT_READ, MAP_SHARED, fd, TIMER_BLOCK))) {
        fprintf(stderr, "mmap() failed.\n");
        return NULL;
    }

    return (uint64_t *) ((char *) timerBase + TIMER_OFFSET);
}


static volatile unsigned int *gpio = NULL;
unsigned int *gpioP(){
    unsigned int *gpio;
    int fdgpio=open("/dev/gpiomem",O_RDWR);
    if (fdgpio<0) { fprintf(stderr, "Error opening /dev/gpiomem"); return NULL; }

    gpio = (unsigned int *)mmap(0,4096,
            PROT_READ+PROT_WRITE, MAP_SHARED,
            fdgpio,0);
    fprintf(stderr, "mmap'd gpiomem at pointer %p\n",gpio);

    return gpio;
}

static volatile uint32_t *preciseTimerptr;

uint64_t readPrecise(){
    uint64_t time = 0;
    time = *preciseTimerptr;
    time |= (uint64_t) *(preciseTimerptr + 1) << 32;
    return time;
}

void gimmespeed(){
    int fd;
    void *timerBase;

    if (-1 == (fd = open("/dev/mem", O_RDWR))) {
        fprintf(stderr, "open() failed.\n");
        return;
    }

#define DIVIDER 0x40000000
#define DIVIDER_OFFSET 0x8
#define PRECISE_TIMER_OFFSET 0x1c

    if (MAP_FAILED == (timerBase = mmap(NULL, 4096,
                        PROT_READ + PROT_WRITE, MAP_SHARED, fd, DIVIDER))) {
        fprintf(stderr, "mmap() failed.\n");
        return;
    }

    uint32_t *divider = (uint32_t *) ((char *) timerBase + DIVIDER_OFFSET);
    preciseTimerptr = (uint32_t *) ((char *) timerBase + PRECISE_TIMER_OFFSET);
    fprintf(stderr, "%x\n", *divider);
    *divider = 0x06AAAAAA;
}

volatile uint64_t *timer;

#define pinoff(hardware) gpio[10] |= 1 << hardware;
#define pinon(hardware) gpio[7] |= 1 << hardware;
#define NOOP1 asm volatile("nop");
#define wholelottanothing() while(*timer <= time);

void waitOneUS(){
    uint64_t time = readPrecise();
    while(readPrecise() == time);

    /*uint64_t time = *timer;*/
    /*while(*timer == time);*/

    /*const int f = 95 / 1.10;*/
    /*while(--f){*/
        /*NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1;*/
        /*NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1; NOOP1;*/
    /*}*/
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

static char *controller;
static uint64_t times[10];
static uint64_t oldtimes[10];
static uint8_t data[11];
void gamerTime(int hasgamed){
    for(int i = 0; i < 10; i++){
        oldtimes[i] = times[i];
    }


	pinMode(CONTROLLER, OUTPUT);
    digitalWrite(CONTROLLER, 1);
    waitOneUS();
    times[0] = *timer;

    /*uint64_t startTime = *timer;*/

    if(hasgamed > 0){
        sendByte(data[1]);
        sendByte(data[2]);
        sendByte(data[3]);
        sendByte(data[4]);
        sendByte(data[5]);
        sendByte(data[6]);
        sendByte(data[7]);
        sendByte(data[8]);

        if(unlikely(hasgamed == 1)){
            sendByte(0x02);
            sendByte(0x02);
            hasgamed++;
        }else if(unlikely(hasgamed == 2)){
            hasgamed = 2;
        }

        sendBit(1);
    }else{
        hasgamed = 1;

        sendByte(0x09);
        sendByte(0x00);
        sendByte(0x00);
        sendBit(1);
    }

    /*
    uint64_t endTime = *timer - startTime;
    fprintf(stderr, " %lluus ", endTime);
    for(int i = 1; i < 9; i++){
        fprintf(stderr, "%02x", data[i]);
    }
    fprintf(stderr, "\r");
    */
    times[1] = *timer;
    pinMode(CONTROLLER, INPUT);
    FILE *ff = fopen(controller, "rb");
    /*fseek(ff, 0, SEEK_SET);*/
    times[2] = *timer;
    fread(data, 11, 1, ff);
    times[3] = *timer;
    fclose(ff);
    times[4] = *timer;
}


int main(int argc, char *argv[]) {
    if(argc != 4){
        fprintf(stderr, "bad args");
        return 1;
    }
    CONTROLLER = atoi(argv[2]);
    HARDWARE = atoi(argv[3]);

    fprintf(stderr, "controller %i\n", CONTROLLER);
    controller = argv[1];

    uint64_t start;
    timer = getHWTimer();
    start = *timer;

    gimmespeed();
	wiringPiSetup();
	pinMode(CONTROLLER, INPUT);
    //pinMode(0, INPUT);
    gpio = gpioP();
    fprintf(stderr, "setup took %lldus\n", *timer - start);

#ifdef FORK
    int cp = fork();
    if(cp > 0){
#endif
        uint64_t time0=0, time1=0;
        int lastVal = 1 << 4;
        int data[64];
        int datap = 0;
        int gamerlevel = 0;
        for(;;){
            int f = gpio[13] & (1 << HARDWARE);
            if(f){
                time1++;
                if(time1 == 100){
                    char stuff = 0;
                    for(int i = 1; i < datap; i++){
                        stuff |= data[i] << (8 - i);
                    }


                    if(unlikely(stuff == 0)){
                        gamerTime(0);
                        fprintf(stderr, "gamer level %i\n", gamerlevel++);

                        fprintf(stderr, "gamertime %lldus ", oldtimes[1] - oldtimes[0]);
                        fprintf(stderr, "fread %lldus ", oldtimes[2] - oldtimes[1]);
                        fprintf(stderr, "more fsuff %lldus ", oldtimes[3] - oldtimes[2]);
                        fprintf(stderr, "more fstuff %lldus \n", oldtimes[4] - oldtimes[3]);
                    }else if(unlikely(stuff == 0x41)){
                        gamerTime(1);
                    }else{
                        gamerTime(2);
                        int timeTaken = oldtimes[1] - oldtimes[0];
                        if(timeTaken > 275 || timeTaken < 255 || true){
                            fprintf(stderr, "  %lldus ", oldtimes[1] - oldtimes[0]);
                        }
                    }
                    fprintf(stderr, "  stuff was %x \r", stuff);

                    /*
                    fprintf(stderr, "%i data\n", datap);
                    for(int i = 1; i < datap; i++){
                        fprintf(stderr, data[i] ? "1" : "0");
                    }
                    fprintf(stderr, "\nalso hey value is %x\n", stuff);
                    fprintf(stderr, "\n");
                    */
                    datap = 0;

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
            if(!f){
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
