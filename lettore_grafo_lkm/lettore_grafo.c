/**
 * @file   testdijkstra.c
 * @author Marco Tessarotto
 * @date  19 September 2019
 * @version 0.1
 * @brief  A Linux user space program that communicates with the dijkstra.c LKM. It passes a
 * string to the LKM and reads the response from the LKM. For this example to work the device
 * must be called /dev/dijkstrasp.
*/
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

#include <sys/ioctl.h>

#define WR_ORIGIN_NODE_ID _IOW('a','a',int32_t*)
#define RD_ORIGIN_NODE_ID _IOR('a','b',int32_t*)

#define WR_NUM_NODES _IOW('a','c',int32_t*)
#define RD_NUM_NODES _IOR('a','d',int32_t*)

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM

int main(){
   int ret, fd;
//   char stringToSend[BUFFER_LENGTH];

   unsigned int num_nodes;
   unsigned int origin_id;

   printf("Starting device test code example...\n");

   fd = open("/dev/dijkstrasp", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
//   printf("Type in a short string to send to the kernel module:\n");
//   scanf("%[^\n]%*c", stringToSend);              // Read in a string (with spaces)

   num_nodes = 1234;
   origin_id = 4321;


   ioctl(fd, WR_NUM_NODES, (int32_t *) &num_nodes);

   ioctl(fd, WR_ORIGIN_NODE_ID, (int32_t *) &origin_id);

   // adesso cominciamo a scrivere un nodo alla volta


//   ret = write(fd, &num_nodes, sizeof(unsigned int)); // Send the string to the LKM
//   if (ret < 0){
//      perror("Failed to write num_nodes to the device.");
//      return errno;
//   }
//
//   ret = write(fd, &origin_id, sizeof(unsigned int)); // Send the string to the LKM
//   if (ret < 0){
//      perror("Failed to write origin_id to the device.");
//      return errno;
//   }


//   printf("Press ENTER to read back from the device...");
//   getchar();
//
//   printf("Reading from the device...\n");
//   ret = read(fd, receive, BUFFER_LENGTH);        // Read the response from the LKM
//   if (ret < 0){
//      perror("Failed to read the message from the device.");
//      return errno;
//   }
//   printf("The received message is: [%s]\n", receive);
   printf("End of the program\n");
   return 0;
}
