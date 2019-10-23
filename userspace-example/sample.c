#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>



int pfd[2];




int dijkstra() {

	int buffer[1024];
	int n;
	//
	int array_len;


	read(STDIN_FILENO, &array_len, sizeof(int)); // leggo la lunghezza dell'array


	n = read(STDIN_FILENO, buffer, (array_len-1) * sizeof(int));


	/// altro fork con altra pipe (ricevi_risultati)

}


int lettore_grafo() {

	// legge dal file del grafo...
	// node_id; lat; lon; n (numero di peers); peer_0_node_id; peer_0_dist; .....

	int sample_line[] = { -1, 1, 0, 0, 2, 2, 10, 3, 20 }; // nodo 1, lat=0, lon=0, 2 peers: nodo 2 distanza 10, node 3 distanza 20
	// -1 va rimpiazzato con la lunghezza effettiva dell'array
	sample_line[0] = sizeof(sample_line);

	write(STDOUT_FILENO, sample_line, sizeof(sample_line) * sizeof(int));
	// ....

}


/*
 * esempio dove si utilizzano fork, execve, wait
 *
 */

int main(int argc, char *argv[]) {

	if (pipe(pfd) == -1) {
		perror("problema con pipe");

		exit(EXIT_FAILURE);
	}


	switch (fork()) {
		case -1:
			perror("problema con fork");

			exit(EXIT_FAILURE);

		case 0: // processo FIGLIO: leggerà dalla PIPE

			close(pfd[1]); // chiudiamo l'estremità di scrittura della pipe, non ci serve

		    if (dup2(pfd[0], STDIN_FILENO) == -1) {
		    	perror("problema con dup2");

		    	exit(EXIT_FAILURE);
		    }

			dijkstra();

		default:

			close(pfd[0]); // chiudiamo pipe in lettura

		    if (dup2(pfd[1], STDOUT_FILENO) == -1) {
		    	perror("problema con dup2");

		    	exit(EXIT_FAILURE);
		    }

			lettore_grafo();
	}

	return 0;


}
