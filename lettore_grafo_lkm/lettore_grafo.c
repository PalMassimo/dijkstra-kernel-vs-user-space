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
#include <linux/types.h>
#include <inttypes.h>

#define WR_ORIGIN_NODE_ID _IOW('a','a',int32_t*)
#define RD_ORIGIN_NODE_ID _IOR('a','b',int32_t*)

#define WR_NUM_NODES _IOW('a','c',int32_t*)
#define RD_NUM_NODES _IOR('a','d',int32_t*)

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM

#define START_DIJKSTRA_THREAD 0xDEADBEEF

int main(){
   int ret, fd;
//   char stringToSend[BUFFER_LENGTH];

   uint32_t num_nodes;
   uint32_t origin_id;

   printf("Starting device test code example...\n");

   fd = open("/dev/dijkstrasp_dev", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }

//   close(fd);
//   return 0;

   char * input_file_name = "./grafo.txt";

   FILE * inPtr=fopen(input_file_name, "r");
   if(inPtr==NULL){
   		perror("[lettore_grafo]: fopen");
   		exit(EXIT_FAILURE);
   	}


	fscanf(inPtr,"%u %u", &num_nodes, &origin_id);
	//fprintf(stdout, "%d\n", catch_error);
	printf("num_nodes=%u origin_id=%u\n", num_nodes, origin_id);

	//catch the error: origin's id>=total nodes
	if(origin_id>=num_nodes){
	//		catch_error=-1;
	//		write(fp, &catch_error, sizeof(int));
		puts("[lettore_grafo]: il numero dei nodi e' superiore all'id dell'origine ");
	//		close(fp);
		exit(EXIT_FAILURE);
	}
	//	else write(fp, &catch_error, sizeof(int));

	//passing the total nodes and the origin's id
//	write(fd, &num_nodes, sizeof(uint32_t));
//	write(fp, &origin_id, sizeof(uint32_t));


//	num_nodes = 1234;
//	origin_id = 4321;

	ioctl(fd, WR_NUM_NODES, (int32_t *) &num_nodes);
	ioctl(fd, WR_ORIGIN_NODE_ID, (int32_t *) &origin_id);


	//continue  to read the file text...
	while (!feof(inPtr)) {
		uint32_t node_id, peer_id;
		uint32_t num_adjacents;
		uint32_t num_items;
		double new_lat, new_lon, peer_distance;
		uint32_t int_peer_distance;
		num_items=fscanf(inPtr, "%u %lf %lf %u", &node_id, &new_lat, &new_lon, &num_adjacents);

		printf("current node_id=%u, num_adjacents=%u\n", node_id, num_adjacents);

		if(num_items==0 || num_items==EOF) break;

		int buf_len = (2 + num_adjacents * 2) * sizeof(uint32_t);
		char * buf = malloc(buf_len);
		int i = 2;

		printf("buf_len=%u\n",buf_len);

		uint32_t * u32_buf = buf;

		u32_buf[0] = node_id;
		u32_buf[1] = num_adjacents;

//		write(fd, &node_id, sizeof(uint32_t));
//		write(fd, &num_adj, sizeof(uint32_t));

		uint32_t pos = 2;

		for(uint32_t i=0; i<num_adjacents; i++){
			fscanf(inPtr, "%lf %u", &peer_distance, &peer_id);

			u32_buf[pos++] = peer_id;
//			write(fd, &peer_id, sizeof(uint32_t));

			int_peer_distance=peer_distance*1000;
			u32_buf[pos++] = int_peer_distance;
//			write(fd, &int_peer_distance, sizeof(uint32_t));
		}

		for (int i = 0; i < buf_len; i++)
			printf("%i ", buf[i]);

		printf("\n");

		if (write(fd, buf, buf_len) == -1) {
			printf("error while writing node %u to device\n", node_id);
		}

		uint32_t msg = START_DIJKSTRA_THREAD;

		write(fd, &msg, sizeof(msg));

		free(buf);
	}

	puts("[lettore_grafo] exit");

	if(fclose(inPtr)){
		perror("[lettore_grafo]: fclose");
		exit(EXIT_FAILURE);
	}




//   printf("Type in a short string to send to the kernel module:\n");
//   scanf("%[^\n]%*c", stringToSend);              // Read in a
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
