#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include "firmware.h"
/**
 * This definition defines the number of stop bits - 1, data bits - 8 and parity - none 
 * for termios configuration 
 * 
 * Update the definitions to change the attribute speed 
 */
#define BAUD_RATE B9600
#define S_PARTIY PARENB
#define STOP_BITS CSTOPB
#define DATA_BITS CS8 

//epoll max events for receiving data 
#define MAX_EVENTS 5
#define READ_BUF_SIZE 100



struct termios config; 
struct termios temp; 

const char device[] = "/dev/pts/4";
const char ignition[] = "RISC-V ACT Framework Enablement firmware\n"; 

char readbuffer[READ_BUF_SIZE];

int main()
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd == -1) {
        printf( "Failed to open port\r\n" );
        return -1; 
    }
    int health = termioshealth(fd, &config); 
    if(health < 0)
    {
        printf("Termios failed health check \r\n"); 
        return -1; 
    }
    int speedstatus = checkspeed(BAUD_RATE, &config); 
    if(speedstatus < 1)
    { 
        printf("Termios Speed is not set \r\n"); 
        return -1; 
    }
    int healthattr = updateattributes(&config, fd); 
    if(healthattr < 0)
    {
        printf("Failed to update attributes \r\n"); 
        return -1; 
    }
    int valid = validateupdate(&config, &temp, fd); 
    if(valid < 0)
    {
        printf("Failed to update termios attibute \r\n"); 
        return -1; 
    }
    int status = sendmessage(fd, ignition, sizeof(ignition) ); 
    if(status < 1)
    {
        printf("Failed to send message \r\n"); 
        return -1; 
    }else{
        printf("Ignition Message sent \r\n"); 
    }

    

    int status_recv = nonblockingrecv(fd,readbuffer,READ_BUF_SIZE-1); 
    if(status_recv < 1)
    {
        printf("Error occured while receiving messages \r\n"); 
        return -1; 
    }
    printf("end of program closing fd \n"); 

    close(fd); 
}
/**
 * @brief
 *       Fn to check terminos object health. The function checks if the fd represents a tty device.
 *       If not the system will not work. 
 * @arg 
 *      fd - file descriptor for communication 
 *      config - pointer to the current config implementation to be used for communication
 * 
 * @return 
 *         -1 when the health checks fail 
 *          1 - success 
 * 
  @author 
 *        Eric Mwangi Macharia 
 * 
 */
int termioshealth(int fd, struct termios* config)
{
    //assume fd is open
    if(!isatty(fd))
    {
        printf("Opened Device is not TTY\r\n");
        return -1; 
    }
    if(tcgetattr(fd, config) != 0) 
    { 
        printf("Termios Specification has a problem\r\n"); 
        return -1; 
    }
    return 1; 
}

/**
 * @brief 
 *         Fn part of the validation system used to check if the termios attributes have been set correctly
 * 
 * @arg 
 *      Speed - baudrate speed i.e 9600, 115200 
 *      config - pointer to the systems termios object 
 * 
 * @note 
 *      Termios provides a nice wrapper function cfsetispeed to validate input and output speed
 *      Prefer this functions over checking the setbits of c_cflags 
 * 
    @author 
 *        Eric Mwangi Macharia 
 * 
 */
int checkspeed(int speed,struct termios* config)
{
    if(cfsetispeed(config, speed) < 0 || cfsetospeed(config, speed) < 0) {
        printf("BAUD rate is undefined \r\n"); 
        return -1; 
    }
    return 1; 
}
/**
 * @brief 
 *       Fn to configure parity in the data frame 
 * @arg
 *        config - current config object 
 *         parity - type of parity for sending data.
 *         Tested options include None, even and odd  
 *  * @author 
 *        Eric Mwangi Macharia 
 */
int enableparity(struct termios* config, int parity)
{
    config->c_cflag &= ~ parity; 
    return 1; 
}
/**
 * @brief 
 *        Fn to set the number of stop bits 
 * @arg 
 *      config - current termios object
 *      numberofstopbits - number of stop bits in the data frame 
 * 
 * @author 
 *        Eric Mwangi Macharia 
 *      
 * 
 */
int setstopbit(struct termios* config, int numberofstopbits)
{
    config->c_cflag &= ~numberofstopbits;
    return 1; 
}
/**
 * @brief 
 *       Fn used to set number of data bits 
 * 
 * @arg 
 *      config - pointer to the current termios object 
 *      numberofbits - number of bits to use in communication 
 * @author 
 *        Eric Mwangi Macharia 
 */
int setdatabits(struct termios* config, int numberofbits)
{
    config->c_cflag |= numberofbits; 
    return 1; 
}
/**
 * @brief 
 *      Fn configures the state of data bits, parity and stop bits 
 * @arg 
 *       config - pointer to the current config implementation 
 *       int fd - file descriptor 
 * @return 
 *          -1 on failure 
 *           1 - on sucess 
 * @author 
 *          Eric Mwangi Macharia 
 * 
 */
int updateattributes(struct termios* config, int fd)
{   
    
    enableparity(config, S_PARTIY); 
    setstopbit(config, STOP_BITS); 
    setdatabits(config, DATA_BITS); 

    int setbit = tcsetattr(fd, TCSANOW, config);
    if(setbit != 0)
    {
        //failed to update the set attribute definition
        printf("Failed to set bit attribute \r\n"); 
        return -1; 
    }
    return 1; 

}

/**
 * @brief 
 *      Fn used to validate the termios attributes including parity, stop bits and number of data bits 
 *    
 *  @arg 
 *      config - pointer to the current termios attributes 
 *      temp   - temp termios object. The current system configs are copied to the temp object to compare 
 *               them with the current config attributes
 * @note 
 *       config in this iteration is not used but will be required in future iterations to compare additional attributes 
 * 
 * @return 
 *        -1 - If any of the attributes is invalid the function will return -1 
 *         1 - The function returns 1 on successful validation of all parameters 
 * 
 * @author 
 *        Eric Mwangi Macharia 
 */
int validateupdate(const struct termios* config, struct termios* temp, int fd)
{
    int result = tcgetattr(fd, temp);
    if(result < 0)
    {
        printf("failed to verify attributes"); 
        return -1; 
    } 
    speed_t speed = cfgetospeed(temp); 
    if(speed == BAUD_RATE)
    {
        printf("Parity is set \r\n"); 
    }else{
        printf("Failed to set BAUD RATE config \r\n"); 
        return -1; 
    }

    if((temp->c_cflag & STOP_BITS) == 0)
    {
        printf("STOP BITS have been set \r\n"); 
    }else {
        printf("Failed to set PARITY \r\n"); 
        return -1; 
    }

    if((temp->c_cflag & CSIZE) == CS8)
    {
        printf("DATA BITS set \r\n");  
    }else {
        printf("Failed to DATA BITS \r\n"); 
        return -1; 
    }

    return 1; 
}

/**
 * @brief 
 *        Function to send message through the file descriptor 
 * 
 * @arg 
 *      fd - file descriptor to write to 
 *      msg - buffer containing information to send to the UART port/TTY port. 
 *      len - number of bytes to transfer
 *      
 * @return type 
 *          return 1 - when message is sent 
 *                -1 - when the system fails to send the message 
 * @author 
 *         Eric Mwangi Macharia 
 */

int sendmessage(int fd, const char* msg, int len)
{
    int status = write(fd, msg, len); 
    if(status < 0)
    {
        printf("Failed to writed\r\n"); 
        return -1; 
    }
    return 1; 
}

/**
 * @brief 
 *        A blocking function to read from the UART port 
 *
 * @note  
 *        In the implementation the function is not called 
 * 
 * @arg 
 *      fd - file descriptor 
 *      buf - data read is stored in this buffer 
 *      len - amount of data that should be read 
 * 
 *  @author Eric Mwangi Macharia 
 * 
 */
int blockingrecv(int fd, char* buf, int len)
{
    int n = read(fd, buf, len); 
    if(n < 0)
    {
        printf("Failed to read file messages\r\n"); 
        return -1; 
    }else if(n ==0)
    {
        printf("No Data to receive \r\n"); 
        return 1; 
    }else{ 
        printf("Received the data below from UART \r\n"); 
        printf("\n---------------------------------\r\n"); 
        fwrite(buf,1, len,stdout); 
        printf("\n---------------------------------\r\n"); 
        return 1; 
    }
}
/**
 * @brief 
 *         Fn non blocking receive - to receive messages from UART terminal 
 * 
 * @param 
 *         fd - file descriptor to read from 
 *         buf - buffer to hold message received
 *         len - number of bytes that the buffer can handle
 * 
 * @note 
 *        The implementation prevents overflows bugs by utilizing the length variable 
 *        Segmentation fault will occur if len > than length of buffer 
 * 
 *        The function is non blocking it utilizes linux epoll functionality to monitor for event in the file descriptor 
 *        Once an event occurs for the file descriptor, it prints the bytes to the stdout file descriptor 
 * 
 * @author 
 *        Eric Mwangi Macharia 
 * @date  22/04/2026 
 */
int nonblockingrecv(int fd, char* buf, int len)
{
    int epfd = epoll_create1(0);
    struct epoll_event ev, events[MAX_EVENTS];

    // Watch fd received data 
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    

    // while (1) {
    
       int n = epoll_wait(epfd, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == fd) {
                printf("Data received from epoll !\n");

                int bytes = read(fd, buf, len-1);

                if (bytes > 0) {
                    buf[bytes] = '\0';
                    printf("Received the data below from UART \r\n"); 
                    printf("\n---------------------------------\r\n"); 
                    fwrite(buf,1, bytes,stdout); 
                    printf("\n---------------------------------\r\n"); 
                    return 1; 
                }
            }
        }
    // }

    // return -1;

}