/*
 * tesi_2.1.c
 *
 *  Created on: 17 ott 2019
 *      Author: massimo
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// TODO: non si può usare un define per NODES ma usare una variabile
#define NODES 6
#define INFTY 99999999

typedef struct peer {
	int id; // node id
	double distance; // TODO: double -> long
} Peer;

typedef struct node {
	int id; // node id
	double lon; // lasciamo ma non va passato al kernel module
	double lat; // lasciamo ma non va passato al kernel module
	unsigned int num_adjacents;
	struct peer * adjacent;
} Node;

// NON usare array globali ma sposta tutto nelle funzioni
double distances[NODES];  // TODO: double -> long
Node * previouses[NODES];
Node * nodes[NODES];

void print_peers(Node *);
void print_distances(void);
void print_previouses(void);

void lettore_grafo() { // processo 1
    // non usare direttamente le pipe ma usare stdin e stdout
}


void kernel_process() {
	// non usare direttamente le pipe ma usare stdin e stdout

}

void ricevi_risultati() {
	// non usare direttamente le pipe ma usare stdin e stdout
}

int main(int argc, char *argv[]){

	//creation pipe: parent process write, child process read
	int first_pipe[2]; // va bene qui; però: kernel_process lavora solo con stdin, stdout (vanno preparati prima di chiamare kernel_process)
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
		perror("[1st process]: fork");
		exit(EXIT_FAILURE);
		break;

	case 0: //kernel process

		// TODO: spostare il "kernel process" in kernel_process()

		puts("[kernel]:  started");

		if(close(first_pipe[1])) {
			perror("[kernel]:  closing write side of the first pipe");
			exit(EXIT_FAILURE);
		}

		if(close(second_pipe[0])) {
			perror("[kernel]: closing read side of the second pipe");
			exit(EXIT_FAILURE);
		}

		for(int i=0; i<NODES; i++) {
			nodes[i]=NULL;
			previouses[i]=NULL;
		}

		int node_id, origin_id, copy_id;
		double node_lat, node_lon;
		unsigned int num_adj;
		Peer * list_adjacent;

		// TODO: aggiungi read del numero di nodi

		// quindi: l'array nodes[] viene allocato DOPO aver ricevuto il parametro numero_di_nodi

		read(first_pipe[0], &copy_id, sizeof(int *));

		origin_id=copy_id;
		for(unsigned int i=0; i<NODES; i++){
			read(first_pipe[0], &node_id, sizeof(int *));
			read(first_pipe[0], &node_lat, sizeof(double *));
			read(first_pipe[0], &node_lon, sizeof(double *));
			read(first_pipe[0], &num_adj, sizeof(unsigned int *));

			list_adjacent=(Peer *) calloc(num_adj, sizeof(Peer));

			nodes[node_id]=malloc(sizeof(Node));
			nodes[node_id]->id=node_id;
			nodes[node_id]->lat=node_lat;
			nodes[node_id]->lon=node_lon;
			nodes[node_id]->num_adjacents=num_adj;
			nodes[node_id]->adjacent=list_adjacent;

			int peer_id;
			double peer_distance;
			for(unsigned int j=0; j<num_adj; j++){
				read(first_pipe[0], &peer_id, sizeof(int *));
				read(first_pipe[0], &peer_distance, sizeof(double *));
				list_adjacent[j].id=peer_id;
				list_adjacent[j].distance=peer_distance;
			}
		}

		Node * origin=nodes[origin_id];

		for(unsigned int i=0; i<NODES; i++){
			distances[i]=INFTY;
		}
		distances[origin->id]=0; printf("origin->id is %d\n", origin->id);
		previouses[origin->id]=nodes[0];

		unsigned int unvisited=NODES;
		unsigned int indice;
		while(unvisited){
			double min_distance=INFTY;
			for(unsigned int i=0; i<NODES; i++){
				if(distances[i]<min_distance && nodes[i]){
					min_distance=distances[i];
					indice=i;
				}
			}
			if(min_distance==INFTY) break;
			Peer * visit=nodes[indice]->adjacent;
			for(unsigned int i=0; i<nodes[indice]->num_adjacents; i++){
				if(distances[visit->id]>distances[nodes[indice]->id]+visit->distance){
					distances[visit->id]=distances[nodes[indice]->id]+visit->distance;
					previouses[visit->id]=nodes[indice];
				} visit++;
			}
			nodes[indice]=NULL;
			unvisited--;
		}

		// send results to the second process:

		// il terzo processo "non sa" nulla dei dati inviati dal primo processo
		// quindi il kernel process deve passare "tutto" al terzo processo

		// TODO: scrivere anche il numero di nodi; id del nodo di partenza

		// TODO: usare lo stesso formato dell'input, aggiungendo su ogni riga dist e prev_node

		for(unsigned int i=0; i<NODES; i++){
			write(second_pipe[1], &(distances[i]), sizeof(double *));
		}

		for(unsigned int i=0; i<NODES; i++){
			write(second_pipe[1], &(previouses[i]->id), sizeof(int *));
		}

		if(close(first_pipe[0])){
			perror("[kernel]:  close read side of the first pipe");
			exit(EXIT_FAILURE);
		}
		if(close(second_pipe[1])){
			perror("[kernel]: close write side of the second pipe");
			exit(EXIT_FAILURE);
		}
		puts("[kernel]:  exit");
		exit(EXIT_SUCCESS);
		break;

	default: //first process
		// TODO: spostare nella funzione lettore_grafo

		puts("[1st process]: after fork");
		if(close(first_pipe[0])){
			perror("[1st process]: close read side of the first pipe");
			exit(EXIT_FAILURE);
		}

		FILE * inPtr=fopen("../input.txt", "r"); // occhio alla posizione del file
		if(inPtr==NULL){
			perror("[1st process]: fopen");
			exit(EXIT_FAILURE);
		}

		//this is the origin
		int origin_index=0;
		write(first_pipe[1], &origin_index, sizeof(int *));

		while(!feof(inPtr)){
			unsigned int num_items, num_nodi;
			int node_id;
			double new_lat, new_lon;
			int peer_id;
			double peer_distance;
			num_items=fscanf(inPtr, "%d %lf %lf %d", &node_id, &new_lat, &new_lon, &num_nodi);
			if(num_items==0 || num_items==EOF) break;
			write(first_pipe[1], &node_id, sizeof(int *));
			write(first_pipe[1], &new_lat, sizeof(double *));
			write(first_pipe[1], &new_lon, sizeof(double *));
			write(first_pipe[1], &num_nodi, sizeof(unsigned int *));

			for(unsigned int i=0; i<num_nodi; i++){
				fscanf(inPtr, "%lf %d", &peer_distance, &peer_id);
				write(first_pipe[1], &peer_id, sizeof(int *));
				write(first_pipe[1], &peer_distance, sizeof(double *));
			}
		} //puts("[1st process]: completed");
		if(fclose(inPtr)){
			perror("[1st process]: fclose");
			exit(EXIT_FAILURE);
		}
		if(close(first_pipe[1])){
			perror("[1st process]: close write side");
			exit(EXIT_FAILURE);
		}
		switch(fork()){
		case -1:
			perror("[1th process]: second fork");
			exit(EXIT_FAILURE);
			break;

		case 0: //terzo processo

			// TODO: spostare nella funzione ricevi_risultati

			if(close(second_pipe[1])){
				perror("[2nd process]: closing write side of the second pipe");
				exit(EXIT_FAILURE);
			}

			FILE * outPtr=fopen("../output.txt", "w"); // occhio alla posizione del file
			if(outPtr==NULL){
				perror("[2nd]: fopen");
				exit(EXIT_FAILURE);
			}

			int id;
			double distance;
			for(unsigned int i=0; i<NODES; i++){
				read(second_pipe[0], &distance, sizeof(double *));
				fprintf(outPtr, "To node %d is %05.2lf\n", i, distance);
			} fprintf(outPtr, "\n");
			for(unsigned int i=0; i<NODES; i++){
				read(second_pipe[0], &id, sizeof(int *));
				fprintf(outPtr, "Passing from %d to node %d\n", id, i);
			} fprintf(outPtr, "\n");

			if(fclose(outPtr)){
				perror("[2nd process]: fclose");
				exit(EXIT_FAILURE);
			}
			if(close(second_pipe[0])){
				perror("[2nd process]: closing read side of the second pipe");
				exit(EXIT_FAILURE);
			}
			puts("[2nd process]: exit");
			exit(EXIT_SUCCESS);
		break;

		default: //first process
			break;
		}

		puts("[1st process]: exit");
		exit(EXIT_SUCCESS);
		break;
	}
}
/*
void add_peer(Node * node, double adj_distance, int adj_id){
	if(node->adjacent==NULL){
		node->adjacent=malloc(sizeof(Peer));
		node->adjacent->distance=adj_distance;
		node->adjacent->id=adj_id;
		node->adjacent->next=NULL;
	}

	else{
		Peer * hold=node->adjacent;
		while(hold->next!=NULL){
			hold=hold->next;
		}
		hold->next=malloc(sizeof(Peer));
		hold->next->id=adj_id;
		hold->next->distance=adj_distance;
		hold->next->next=NULL;
	}

}*/
/*
void print_peers(Node * node){
	Peer * hold=node->adjacent;
	printf("%d", node->id);
	while(hold){
		printf(" -> (%05.2f, %d)", hold->distance, hold->id);
		hold=hold->next;
	} puts("");
}*/
/*
void print_distances(void){
	for(unsigned int i=0; i<NODES; i++){
		printf("%.2lf\n", distances[i]);
	}
}*/
/*
void print_previouses(void){
	for(unsigned int i=0; i<NODES; i++){
		printf("%d -> %d\n", previouses[i]->id, i);
	}
}*/
