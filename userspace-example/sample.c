/*
 * tesi_2.1.c
 *
 *  Created on: 17 ott 2019
 *      Author: massimo
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <linux/types.h>
#include <inttypes.h>


typedef struct peer {
	uint32_t id;
	uint32_t distance;
} Peer;

typedef struct node {
	uint32_t visited; // si potrebbe usare uint16_t
	uint32_t num_adjacents; // si potrebbe usare uint16_t
	struct peer * adjacent;
	uint32_t distance; // calculated by dijkstra algorithm
	uint32_t prev_node_id; // calculated by dijkstra algorithm
} Node;

void lettore_grafo(char * file_name, int * pipe);
void kernel_process(int fd_in, int fd_out);
void ricevi_risultati(char * file_name, int * pipe);
int main(int argc, char *argv[]){

	char * input_file_name = "./grafo.txt";
	char * output_file_name = "./output.txt";

	//kernel_process work only with stdin, stdout (prepare them before passing to kernel_process)
	int first_pipe[2];
	int second_pipe[2];

	if(pipe(first_pipe)) {
		perror("first pipe");
		exit(EXIT_FAILURE);
	}

	if(pipe(second_pipe)) {
		perror("second pipe");
		exit(EXIT_FAILURE);
	}

	switch(fork()) {

	case -1:
		perror("[lettore_grafo]: fork");
		exit(EXIT_FAILURE);
		break;

	case 0: //kernel process
		if(close(first_pipe[1])) {
			perror("[kernel_process]: close");
			exit(EXIT_FAILURE);
		}

		if(close(second_pipe[0])) {
			perror("[kernel]: close");
			exit(EXIT_FAILURE);
		}

//		if(dup2(first_pipe[0], STDIN_FILENO)==-1) {
//			perror("[kernel]: dup2");
//			exit(EXIT_FAILURE);
//		}
//
//		if(dup2(second_pipe[1], STDOUT_FILENO)==-1) {
//			perror("[kernel]: dup2");
//			exit(EXIT_FAILURE);
//		}

		kernel_process(first_pipe[0], second_pipe[1]);

		break;

	default: //first process
	{
		pid_t fd;
		fd=fork();

		if(fd==-1){
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if(fd==0){
			ricevi_risultati(output_file_name, second_pipe);
		}
		else{
			lettore_grafo(input_file_name, first_pipe);
		}
		break;
	}
	}
	//all processes comes here
	exit(EXIT_SUCCESS);
}

void lettore_grafo(char * file_name, int * fd_out) { // processo 1

	if(close(fd_out[0])){
		perror("[lettore_grafo]: close read side of the first pipe");
		exit(EXIT_FAILURE);
	}

	int fp = fd_out[1];

//	if(dup2(first_pipe[1], STDOUT_FILENO)==-1) {
//		perror("[1ettore_grafo]: dup2");
//		exit(EXIT_FAILURE);
//	}

	FILE * inPtr=fopen(file_name, "r"); // occhio alla posizione del file
	if(inPtr==NULL){
		perror("[lettore_grafo]: fopen");
		exit(EXIT_FAILURE);
	}

	uint32_t origin_id, num_nodes;
	int catch_error=0; //default: no error
	fscanf(inPtr,"%u %u", &num_nodes, &origin_id);
	//fprintf(stdout, "%d\n", catch_error);

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
	write(fp, &num_nodes, sizeof(uint32_t));
	write(fp, &origin_id, sizeof(uint32_t));

	//continue  to read the file text...
	while(!feof(inPtr)){
		uint32_t node_id, peer_id;
		uint32_t num_adj;
		uint32_t num_items;
		double new_lat, new_lon, peer_distance;
		uint32_t int_peer_distance;
		num_items=fscanf(inPtr, "%u %lf %lf %u", &node_id, &new_lat, &new_lon, &num_adj);
		if(num_items==0 || num_items==EOF) break;
		write(fp, &node_id, sizeof(uint32_t));
		write(fp, &num_adj, sizeof(uint32_t));

		for(uint32_t i=0; i<num_adj; i++){
			fscanf(inPtr, "%lf %u", &peer_distance, &peer_id);
			write(fp, &peer_id, sizeof(uint32_t));
			int_peer_distance=peer_distance*1000;
			write(fp, &int_peer_distance, sizeof(uint32_t));
		}
	}

	puts("[lettore_grafo] exit");

	if(close(fd_out[1])){
		perror("[lettore_grafo]: close write side");
		exit(EXIT_FAILURE);
	}

	if(fclose(inPtr)){
		perror("[lettore_grafo]: fclose");
		exit(EXIT_FAILURE);
	}
}


unsigned long long get_cycles() {
  asm ("rdtsc");
}


void kernel_process(int fd_in, int fd_out) {

	unsigned long long t1, t2;
	struct timespec start, stop;
	int catch_error=0, dowhile_counter=0;

//	//catch eventual error
//	read(fd_in, &catch_error, sizeof(int));
//	//fprintf(stdout, "catch error %d\n", catch_error);
//	if(catch_error==-1){
//		close(fd_in);
//		close(fd_out);
//		exit(EXIT_FAILURE);
//	}

	// non usare direttamente le pipe ma usare stdin e stdout
	uint32_t origin_id, num_nodes;
	read(fd_in, &num_nodes, sizeof(uint32_t));
	read(fd_in, &origin_id, sizeof(uint32_t));

	if(num_nodes<=origin_id){
		catch_error=-1;
		write(fd_out, &catch_error, sizeof(int));
		close(fd_in);
		close(fd_out);
		exit(EXIT_FAILURE);
	}


	Node * nodes = malloc(sizeof(Node) * num_nodes);
	Peer * list_adjacent;
	uint32_t node_id, num_adj;

	for(uint32_t i=0; i<num_nodes; i++) {
		read(fd_in, &node_id, sizeof(uint32_t));
		read(fd_in, &num_adj, sizeof(uint32_t));

		//check that node_id<num_nodes and that node_id==i
		if(node_id!=i || node_id>=num_nodes){
			close(fd_in);
			write(fd_out, &catch_error, sizeof(int));
			close(fd_out);
			exit(EXIT_FAILURE);
		}

		list_adjacent=(Peer *) calloc(num_adj, sizeof(Peer));

		//nodes[node_id]=malloc(sizeof(Node));
		nodes[node_id].num_adjacents=num_adj;
		nodes[node_id].visited=0;
		nodes[node_id].adjacent=list_adjacent;
		nodes[node_id].distance=UINT_MAX;
		nodes[node_id].prev_node_id=-1;

		uint32_t peer_id;
		uint32_t peer_distance;
		for(uint32_t j=0; j<num_adj; j++){
			read(fd_in, &peer_id, sizeof(uint32_t));
			read(fd_in, &peer_distance, sizeof(uint32_t));

			if(peer_id>=num_nodes){
				close(fd_in);
				close(fd_out);
				exit(EXIT_FAILURE);
			}
			list_adjacent[j].id=peer_id;
			list_adjacent[j].distance=peer_distance;
		}
	}

	close(fd_in);

	for (int number_of_repetitions = 0; number_of_repetitions < 20; number_of_repetitions++) {

		for (uint32_t i=0; i < num_nodes; i++) {
			nodes[i].visited=0;
			nodes[i].distance=UINT_MAX;
			nodes[i].prev_node_id=-1;
		}

		dowhile_counter = 0;

		clock_gettime(/*CLOCK_PROCESS_CPUTIME_ID*/ CLOCK_MONOTONIC, &start);

		t1 = get_cycles();

		nodes[origin_id].distance=0;
		nodes[origin_id].prev_node_id = origin_id;


		uint32_t unvisited=num_nodes;
		uint32_t indice;
		while(unvisited){
			uint32_t min_distance=UINT_MAX;
			for(uint32_t i=0; i<num_nodes; i++){
				if(nodes[i].distance < min_distance && nodes[i].visited==0){
					min_distance=nodes[i].distance;
					indice=i;
				}
			}

			if(min_distance==UINT_MAX) break;

			Peer * visit=nodes[indice].adjacent;
			for(uint32_t i=0; i < nodes[indice].num_adjacents; i++){
				if(nodes[visit->id].distance > nodes[indice].distance + visit->distance){
					nodes[visit->id].distance = nodes[indice].distance + visit->distance;
					nodes[visit->id].prev_node_id = indice;
				}
				visit++;
			}
			nodes[indice].visited=1;
			unvisited--;
			dowhile_counter++;
		}

		t2 = get_cycles();

		clock_gettime(/*CLOCK_PROCESS_CPUTIME_ID*/ CLOCK_MONOTONIC, &stop);
		double result = (stop.tv_sec - start.tv_sec) * 1e9 + (stop.tv_nsec - start.tv_nsec) /*/ 1e3*/;
		printf("[kernel]  total cycles: %llu , tempo di CPU consumato: "
				"%lf nanoseconds, unvisited: %d, dowhile_counter: %d\n", t2 - t1, result, unvisited, dowhile_counter);

	}


	// send results to the second process:
	Peer * hold;
	write(fd_out, &catch_error, sizeof(int));
	write(fd_out, &num_nodes, sizeof(uint32_t));
	for(uint32_t i=0; i<num_nodes; i++){
		write(fd_out, &(nodes[i].num_adjacents), sizeof(uint32_t));
		hold=nodes[i].adjacent;
		for(uint32_t j=0; j<(nodes[i].num_adjacents); j++){
			write(fd_out, &(hold->distance), sizeof(uint32_t));
			write(fd_out, &(hold->id), sizeof(uint32_t));
			hold++;
		}
		write(fd_out, &(nodes[i].distance), sizeof(uint32_t));
		write(fd_out, &(nodes[i].prev_node_id), sizeof(uint32_t));
	}

	//free associated memory
	free(nodes);

	if(close(fd_out)){
		perror("[kernel]: close read side of the second pipe");
		exit(EXIT_FAILURE);
	}

	fprintf(stdout, "[kernel_process] dijkstra finished \n");
}

void ricevi_risultati(char * file_name, int * pipe_param) {

	if(close(pipe_param[1])){
		perror("[2nd process]: closing write side of the second pipe");
		exit(EXIT_FAILURE);
	}

	if(dup2(pipe_param[0], STDIN_FILENO)){
		perror("[ricevi_risultati]: dup2");
		exit(EXIT_FAILURE);
	}

	FILE * outPtr=fopen(file_name, "w"); // occhio alla posizione del file
	if(outPtr==NULL){
		perror("[ricevi_parametri]: fopen");
		exit(EXIT_FAILURE);
	}

	int catch_error=0;
	read(STDIN_FILENO, &catch_error, sizeof(int));
	fprintf(stdout, "catch error %d\n", catch_error);
	if(catch_error==-1){
		puts("[ricevi_risultati]: kernel process checked an error");
		close(STDIN_FILENO);
		exit(EXIT_FAILURE);
	}

	uint32_t num_nodes, previous_id, distance, peer_id, peer_distance;
	uint32_t num_adj;
	read(STDIN_FILENO, &num_nodes, sizeof(uint32_t));

	// TODO: se num_nodes < 0 significa che kernel process ha restituito un errore (e quindi ...)
	// Appunto: ma num_nodes è un unsigned int

	for(uint32_t i=0; i<num_nodes; i++){
		read(STDIN_FILENO, &num_adj, sizeof(uint32_t));
		fprintf(outPtr, "%u %u ", i, num_adj);
		for(unsigned int i=0; i<num_adj; i++){
			read(STDIN_FILENO, &peer_distance, sizeof(uint32_t));
			read(STDIN_FILENO, &peer_id, sizeof(uint32_t));
			fprintf(outPtr, "%.2lf %u ", (double)(peer_distance/1000.0), peer_id);
		}
		read(STDIN_FILENO, &distance, sizeof(uint32_t));
		read(STDIN_FILENO, &previous_id, sizeof(uint32_t));
		fprintf(outPtr, "%.2lf %u\n", (double)(distance/1000.0), previous_id);
	}

	fprintf(stdout,"[ricevi_risultati] exit\n");

	if(fclose(outPtr)){
		perror("[ricevi_risultati]: fclose");
		exit(EXIT_FAILURE);
	}
	if(close(pipe_param[0])){
		perror("[ricevi_risultati]: closing read side of the second pipe");
		exit(EXIT_FAILURE);
	}
}
