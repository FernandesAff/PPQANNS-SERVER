#ifndef H_IVFASSIGN
#define H_IVFASSIGN

	#include <stdio.h>
	#include <stdlib.h>
	#include <mpi.h>
	extern "C"{
		#include "../yael/nn.h"
	}
	#include "../pq-utils/pq_test_load_vectors.h"
	#include "../pq-utils/pq_new.h"
	#include "myIVF.h"
	#include <netdb.h> 
	#include <netinet/in.h> 
	#include <sys/socket.h> 
	#include <sys/types.h> 
	#include <unistd.h>
	#include <cstdint>
	#define MAX_BUF 1024
	#define PORT 8001
	#define SA struct sockaddr 

	void parallel_assign (char *dataset, int w, int comm_sz, MPI_Comm search_comm, int threads);
	void bsxfunMINUS(float *mout, mat vin, float* vin2, int nq, int* qcoaidx, int ncoaidx);
	mat pq_test_receive_query(char *dataset, int sockfd);

#endif
