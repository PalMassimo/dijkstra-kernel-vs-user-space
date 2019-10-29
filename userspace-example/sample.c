/*
 * tesi_2.1.c
 *
 *  Created on: 17 ott 2019
 *      Author: massimo
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define INFTY 999999999999

typedef struct peer {
	unsigned long id; 			// node id
	unsigned long distance;
} Peer;

typedef struct node {
	unsigned long id;		// node id
	double lon; 			// lasciamo ma non va passato al kernel module
	double lat; 			// lasciamo ma non va passato al kernel module
	short visited;
	unsigned int num_adjacents;
	struct peer * adjacent;
} Node;

void lettore_grafo(int *);
void kernel_process(int fd_in, int fd_out);
void ricevi_risultati(int *);
int main(int argc, char *argv[]){

	//creation pipe: parent process write, child process read
	int first_pipe[2]; //kernel_process lavora solo con stdin, stdout (vanno preparati prima di chiamare kernel_process)
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
			ricevi_risultati(second_pipe);
		}
		else{
			lettore_grafo(first_pipe);
		}
		break;
	}
	}
	//all processes comes here
	exit(EXIT_SUCCESS);
}

void lettore_grafo(int * first_pipe) { // processo 1

	if(close(first_pipe[0])){
		perror("[lettore_grafo]: close read side of the first pipe");
		exit(EXIT_FAILURE);
	}
	int fp = first_pipe[1];

//	int dup_stdout = dup(STDOUT_FILENO);

//	if(dup2(first_pipe[1], STDOUT_FILENO)==-1) {
//		perror("[1ettore_grafo]: dup2");
//		exit(EXIT_FAILURE);
//	}

	FILE * inPtr=fopen("./grafo.txt", "r"); // occhio alla posizione del file
	if(inPtr==NULL){
		perror("[lettore_grafo]: fopen");
		exit(EXIT_FAILURE);
	}

	//origin and num_nodes
	unsigned long origin_id=0;
	unsigned long num_nodes;

	fscanf(inPtr,"%lu", &num_nodes);

	fprintf(stdout,"[lettore_grafo]num_nodes = %lu\n", num_nodes);


	write(fp, &num_nodes, sizeof(unsigned long *));
	write(fp, &origin_id, sizeof(unsigned long *));

	int counter = 0;

	while(!feof(inPtr)){
		unsigned long node_id, peer_id;
		unsigned int num_adj;
		int num_items;
		double new_lat, new_lon, peer_distance;
		unsigned long int_peer_distance;
		num_items=fscanf(inPtr, "%lu %lf %lf %u", &node_id, &new_lat, &new_lon, &num_adj);
		if(num_items==0 || num_items==EOF) break;
		write(fp, &node_id, sizeof(unsigned long *));
		write(fp, &num_adj, sizeof(unsigned int *));

		for(unsigned int i=0; i<num_adj; i++){
			fscanf(inPtr, "%lf %lu", &peer_distance, &peer_id);
			write(fp, &peer_id, sizeof(unsigned long *));
			int_peer_distance=peer_distance*1000000;
			write(fp, &int_peer_distance, sizeof(unsigned long *));
		}

		counter++;
	} //puts("[1st process]: completed");

	fprintf(stdout,"[lettore_grafo]finito counter=%u\n", counter);

	if(fclose(inPtr)){
		perror("[lettore_grafo]: fclose");
		exit(EXIT_FAILURE);
	}
	if(close(first_pipe[1])){
		perror("[lettore_grafo]: close write side");
		exit(EXIT_FAILURE);
	}
}

void kernel_process(int fd_in, int fd_out) {

	// non usare direttamente le pipe ma usare stdin e stdout
	unsigned long origin_id, num_nodes;
	read(fd_in, &num_nodes, sizeof(unsigned long *));
	read(fd_in, &origin_id, sizeof(unsigned long *));

	fprintf(stdout, "[kernel_process] num_nodes=%u  \n", num_nodes);

	unsigned long distances[num_nodes];
	Node * previouses[num_nodes];
	Node * nodes[num_nodes];

	for(unsigned long i=0; i<num_nodes; i++) {
		nodes[i]=NULL;
		previouses[i]=NULL;
	}

	unsigned long node_id;
	unsigned int num_adj;
	Peer * list_adjacent;

	for(unsigned long i=0; i<num_nodes; i++) {
		read(fd_in, &node_id, sizeof(unsigned long *));
		read(fd_in, &num_adj, sizeof(unsigned int *));

		//printf("node_id=%lu, num_adj=%u\n", node_id, num_adj);
		list_adjacent=(Peer *) calloc(num_adj, sizeof(Peer));

		nodes[node_id]=malloc(sizeof(Node));
		nodes[node_id]->id=node_id;
		nodes[node_id]->num_adjacents=num_adj;
		nodes[node_id]->visited=0;
		nodes[node_id]->adjacent=list_adjacent;

		unsigned long peer_id;
		unsigned long peer_distance;
		for(unsigned int j=0; j<num_adj; j++){
			read(fd_in, &peer_id, sizeof(unsigned long *));
			read(fd_in, &peer_distance, sizeof(unsigned long *));
			list_adjacent[j].id=peer_id;
			list_adjacent[j].distance=peer_distance;
		}
	}



	Node * origin=nodes[origin_id];
	for(unsigned int i=0; i<num_nodes; i++){
		distances[i]=INFTY;
	}
	distances[origin->id]=0;
	previouses[origin->id]=nodes[0];



	unsigned long unvisited=num_nodes;
	unsigned long indice;
	while(unvisited){
		unsigned long min_distance=INFTY;
		for(unsigned long i=0; i<num_nodes; i++){
			if(distances[i]<min_distance && nodes[i]->visited!=1){
				min_distance=distances[i];
				indice=i;
			}
		}

		fprintf(stdout, "[kernel_process] ***1 min_distance=%u\n", min_distance);

		if(min_distance==INFTY) break;
		Peer * visit=nodes[indice]->adjacent;
		for(unsigned int i=0; i<nodes[indice]->num_adjacents; i++){
			if(distances[visit->id]>distances[nodes[indice]->id]+visit->distance){
				distances[visit->id]=distances[nodes[indice]->id]+visit->distance;
				previouses[visit->id]=nodes[indice];
			} visit++;
		}
		nodes[indice]->visited=1;
		unvisited--;
	}


	fprintf(stdout, "[kernel_process] finito \n");

	// send results to the second process:
	Peer * hold;
	write(fd_out, &num_nodes, sizeof(unsigned long *));
	for(unsigned long i=0; i<num_nodes; i++){
		write(fd_out, &(nodes[i]->id), sizeof(unsigned long *));
		write(fd_out, &(nodes[i]->num_adjacents), sizeof(unsigned int *));
		hold=nodes[i]->adjacent;
		for(unsigned int j=0; j<(nodes[i]->num_adjacents); j++){
			write(fd_out, &(hold->distance), sizeof(unsigned long *));
			write(fd_out, &(hold->id), sizeof(unsigned long *));
			hold++;
		}
		write(fd_out, &(distances[i]), sizeof(unsigned long *));
		write(fd_out, &(previouses[i]->id), sizeof(unsigned long *));
	}

	if(close(fd_out)){
		perror("[kernel]:  close write side of the first pipe");
		exit(EXIT_FAILURE);
	}
	if(close(fd_out)){
		perror("[kernel]: close read side of the second pipe");
		exit(EXIT_FAILURE);
	}
}

void ricevi_risultati(int * second_pipe) {
	// il terzo processo "non sa" nulla dei dati inviati dal primo processo
	// quindi il kernel process deve passare "tutto" al terzo processo
	// scrivere anche il numero di nodi; id del nodo di partenza
	// usare lo stesso formato dell'input, aggiungendo su ogni riga dist e prev_node
	//puts("[ricevi_risultati]: start");
	if(close(second_pipe[1])){
		perror("[2nd process]: closing write side of the second pipe");
		exit(EXIT_FAILURE);
	}

	if(dup2(second_pipe[0], STDIN_FILENO)){
		perror("[ricevi_risultati]: dup2");
		exit(EXIT_FAILURE);
	}

	FILE * outPtr=fopen("./output.txt", "w"); // occhio alla posizione del file
	if(outPtr==NULL){
		perror("[ricevi_parametri]: fopen");
		exit(EXIT_FAILURE);
	}

	unsigned long num_nodes, node_id, previous_id, distance, peer_id, peer_distance;
	unsigned int num_adj;
	read(STDIN_FILENO, &num_nodes, sizeof(unsigned long *));

	printf("num_nodes=%lu\n", num_nodes);
	//printf("%lu\n", num_nodes);
	for(unsigned long i=0; i<num_nodes; i++){
		read(STDIN_FILENO, &node_id, sizeof(unsigned long *));
		read(STDIN_FILENO, &num_adj, sizeof(unsigned int *));
		fprintf(outPtr, "%lu %u ", node_id, num_adj);
		for(unsigned int i=0; i<num_adj; i++){
			read(STDIN_FILENO, &peer_distance, sizeof(unsigned long *));
			read(STDIN_FILENO, &peer_id, sizeof(unsigned long *));
			fprintf(outPtr, "%.2lf %lu ", (double)(peer_distance/1000000.0), peer_id);
		}
		read(STDIN_FILENO, &distance, sizeof(unsigned long *));
		read(STDIN_FILENO, &previous_id, sizeof(unsigned long *));
		fprintf(outPtr, "%.2lf %lu\n", (double)(distance/1000000.0), previous_id);
	}

	if(fclose(outPtr)){
		perror("[ricevi_risultati]: fclose");
		exit(EXIT_FAILURE);
	}
	if(close(second_pipe[0])){
		perror("[ricevi_risultati]: closing read side of the second pipe");
		exit(EXIT_FAILURE);
	}
}
