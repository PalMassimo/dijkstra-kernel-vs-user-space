/*
 * tesi_2.1.c
 *
 *  Created on: 17 ott 2019
 *      Author: massimo
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#define NODES 6
#define INFTY 99999999

typedef struct peer {
	int id;
	double distance;
} Peer;

typedef struct node {
	int id;
	double longitudine;
	double latitudine;
	unsigned int num_adjacents;
	struct peer * adjacent;
} Node;

double distances[NODES];
Node * previouses[NODES];
Node * nodes[NODES];

void print_peers(Node *);
void print_distances(void);
void print_previouses(void);

int main(int argc, char *argv[]){

	//creation pipe: parent process write, child process read
	int first_pipe[2];
	int second_pipe[2];
	if(pipe(first_pipe)){
		perror("first pipe");
		exit(EXIT_FAILURE);
	}

	if(pipe(second_pipe)){
		perror("second pipe");
		exit(EXIT_FAILURE);
	}

	switch(fork()){

	case -1:
		perror("[1st process]: fork");
		exit(EXIT_FAILURE);
		break;

	case 0: //kernel process

		puts("[kernel]:  started");
		if(close(first_pipe[1])){
			perror("[kernel]:  closing write side of the first pipe");
			exit(EXIT_FAILURE);
		}
		if(close(second_pipe[0])){
			perror("[kernel]: closing read side of the second pipe");
			exit(EXIT_FAILURE);
		}

		for(int i=0; i<NODES; i++) {
			nodes[i]=NULL;
			previouses[i]=NULL;
		}

		int node_id, origin_id, copy_id;
		double node_latitudine, node_longitudine;
		unsigned int num_adj;
		Peer * list_adjacent;
		read(first_pipe[0], &copy_id, sizeof(int *));
		origin_id=copy_id;
		for(unsigned int i=0; i<NODES; i++){
			read(first_pipe[0], &node_id, sizeof(int *));
			read(first_pipe[0], &node_latitudine, sizeof(double *));
			read(first_pipe[0], &node_longitudine, sizeof(double *));
			read(first_pipe[0], &num_adj, sizeof(unsigned int *));

			list_adjacent=(Peer *) calloc(num_adj, sizeof(Peer));

			nodes[node_id]=malloc(sizeof(Node));
			nodes[node_id]->id=node_id;
			nodes[node_id]->latitudine=node_latitudine;
			nodes[node_id]->longitudine=node_longitudine;
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

		//send results to the second process:
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
		puts("[1st process]: after fork");
		if(close(first_pipe[0])){
			perror("[1st process]: close read side of the first pipe");
			exit(EXIT_FAILURE);
		}

		FILE * inPtr=fopen("/home/massimo/Documenti/Tesi_II/input.txt", "r");
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
			double new_latitudine, new_longitudine;
			int peer_id;
			double peer_distance;
			num_items=fscanf(inPtr, "%d %lf %lf %d", &node_id, &new_latitudine, &new_longitudine, &num_nodi);
			if(num_items==0 || num_items==EOF) break;
			write(first_pipe[1], &node_id, sizeof(int *));
			write(first_pipe[1], &new_latitudine, sizeof(double *));
			write(first_pipe[1], &new_longitudine, sizeof(double *));
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

		case 0: //second process

			if(close(second_pipe[1])){
				perror("[2nd process]: closing write side of the second pipe");
				exit(EXIT_FAILURE);
			}

			FILE * outPtr=fopen("/home/massimo/Documenti/Tesi_II/output.txt", "w");
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
