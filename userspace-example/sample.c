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


typedef struct peer {
	unsigned int id;
	unsigned int distance;
} Peer;

typedef struct node {
	unsigned int id;
	double lon; 			// lasciamo ma non va passato al kernel module
	double lat; 			// lasciamo ma non va passato al kernel module
	short visited;
	unsigned int num_adjacents;
	struct peer * adjacent;
} Node;

void lettore_grafo(char * file_name, int * pipe);
void kernel_process(int fd_in, int fd_out);
void ricevi_risultati(char * file_name, int * pipe);
int main(int argc, char *argv[]){

	char * input_file_name = "./grafo.txt";
	char * output_file_name = "./output.txt";

	//kernel_process work onyl with stdin, stdout (prepare them before passing to kernel_process)
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


	unsigned origin_id=0;
	unsigned num_nodes;

	fscanf(inPtr,"%u", &num_nodes);
	//fprintf(stdout,"[lettore_grafo]num_nodes = %u\n", num_nodes);

	write(fp, &num_nodes, sizeof(unsigned));
	write(fp, &origin_id, sizeof(unsigned));

	int counter = 0;

	while(!feof(inPtr)){
		unsigned node_id, peer_id;
		unsigned int num_adj;
		int num_items;
		double new_lat, new_lon, peer_distance;
		unsigned int_peer_distance;
		num_items=fscanf(inPtr, "%u %lf %lf %u", &node_id, &new_lat, &new_lon, &num_adj);
		if(num_items==0 || num_items==EOF) break;
		write(fp, &node_id, sizeof(unsigned));
		write(fp, &num_adj, sizeof(unsigned));

		for(unsigned int i=0; i<num_adj; i++){
			fscanf(inPtr, "%lf %u", &peer_distance, &peer_id);
			write(fp, &peer_id, sizeof(unsigned));
			int_peer_distance=peer_distance*1000;
			write(fp, &int_peer_distance, sizeof(unsigned));
		}
		counter++;
	}

	//fprintf(stdout,"[lettore_grafo]finito counter=%u\n", counter);
	fprintf(stdout, "[lettore_grafo] exit\n");

	if(fclose(inPtr)){
		perror("[lettore_grafo]: fclose");
		exit(EXIT_FAILURE);
	}
	if(close(fd_out[1])){
		perror("[lettore_grafo]: close write side");
		exit(EXIT_FAILURE);
	}
}

void kernel_process(int fd_in, int fd_out) {

	struct timespec start, stop;
	int dowhile_counter=0;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
	// non usare direttamente le pipe ma usare stdin e stdout
	unsigned origin_id, num_nodes;
	read(fd_in, &num_nodes, sizeof(unsigned));
	read(fd_in, &origin_id, sizeof(unsigned));

	//fprintf(stdout, "[kernel_process] num_nodes=%u  \n", num_nodes);

	unsigned int * distances = malloc(sizeof(unsigned int)*num_nodes);
	Node ** previouses = malloc(sizeof(Node *) * num_nodes);
	Node ** nodes = malloc(sizeof(Node *) * num_nodes);

	unsigned node_id;
	unsigned int num_adj;
	Peer * list_adjacent;

	for(unsigned i=0; i<num_nodes; i++) {
		read(fd_in, &node_id, sizeof(unsigned));
		read(fd_in, &num_adj, sizeof(unsigned int));

		list_adjacent=(Peer *) calloc(num_adj, sizeof(Peer));

		nodes[node_id]=malloc(sizeof(Node));
		nodes[node_id]->id=node_id;
		nodes[node_id]->num_adjacents=num_adj;
		nodes[node_id]->visited=0;
		nodes[node_id]->adjacent=list_adjacent;

		unsigned peer_id;
		unsigned peer_distance;
		for(unsigned int j=0; j<num_adj; j++){
			read(fd_in, &peer_id, sizeof(unsigned));
			read(fd_in, &peer_distance, sizeof(unsigned));
			list_adjacent[j].id=peer_id;
			list_adjacent[j].distance=peer_distance;
		}
	}

	close(fd_in);

	Node * origin=nodes[origin_id];
	for(unsigned int i=0; i<num_nodes; i++){
		distances[i]=UINT_MAX;
	}
	distances[origin->id]=0;
	previouses[origin->id]=nodes[0];


	unsigned unvisited=num_nodes;
	unsigned indice;
	while(unvisited){
		unsigned min_distance=UINT_MAX;
		for(unsigned i=0; i<num_nodes; i++){
			if(distances[i]<min_distance && nodes[i]->visited!=1){
				min_distance=distances[i];
				indice=i;
			}
		}

		if(min_distance==UINT_MAX) break;
		Peer * visit=nodes[indice]->adjacent;
		for(unsigned int i=0; i<nodes[indice]->num_adjacents; i++){
			if(distances[visit->id]>distances[nodes[indice]->id]+visit->distance){
				distances[visit->id]=distances[nodes[indice]->id]+visit->distance;
				previouses[visit->id]=nodes[indice];
			} visit++;
		}
		nodes[indice]->visited=1;
		unvisited--;
		dowhile_counter++;
	}

	fprintf(stdout, "[kernel_process] exit \n");

	// send results to the second process:
	Peer * hold;
	write(fd_out, &num_nodes, sizeof(unsigned));
	for(unsigned i=0; i<num_nodes; i++){
		write(fd_out, &(nodes[i]->id), sizeof(unsigned));
		write(fd_out, &(nodes[i]->num_adjacents), sizeof(unsigned int));
		hold=nodes[i]->adjacent;
		for(unsigned int j=0; j<(nodes[i]->num_adjacents); j++){
			write(fd_out, &(hold->distance), sizeof(unsigned));
			write(fd_out, &(hold->id), sizeof(unsigned));
			hold++;
		}
		write(fd_out, &(distances[i]), sizeof(unsigned));
		write(fd_out, &(previouses[i]->id), sizeof(unsigned));
	}

	//free associated memory
	free(distances);
	free(previouses);
	free(nodes);

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
	double result = (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3;
	printf("[main] tempo di CPU consumato (microsecondi): "
			"%lf, numero cicli do-while: %d\n", result, dowhile_counter);

	if(close(fd_out)){
		perror("[kernel]: close read side of the second pipe");
		exit(EXIT_FAILURE);
	}
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

	unsigned num_nodes, node_id, previous_id, distance, peer_id, peer_distance;
	unsigned int num_adj;
	read(STDIN_FILENO, &num_nodes, sizeof(unsigned));

	for(unsigned i=0; i<num_nodes; i++){
		read(STDIN_FILENO, &node_id, sizeof(unsigned));
		read(STDIN_FILENO, &num_adj, sizeof(unsigned int));
		fprintf(outPtr, "%u %u ", node_id, num_adj);
		for(unsigned int i=0; i<num_adj; i++){
			read(STDIN_FILENO, &peer_distance, sizeof(unsigned));
			read(STDIN_FILENO, &peer_id, sizeof(unsigned));
			fprintf(outPtr, "%.2lf %u ", (double)(peer_distance/1000.0), peer_id);
		}
		read(STDIN_FILENO, &distance, sizeof(unsigned));
		read(STDIN_FILENO, &previous_id, sizeof(unsigned));
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
