#include "ivf_assign.h"
#include "compute_gist.h"

void parallel_assign (char *dataset, int w, int comm_sz, MPI_Comm search_comm,int threads){
	mat vquery, residual;
	ivfpq_t ivfpq;
	int *coaidx, dest, rest,id, search_rank;
	float *coadis;
	static int last_assign, last_search, last_aggregator;
	
	int server_fd, new_socket; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 	

	set_last (comm_sz, &last_assign, &last_search, &last_aggregator);

	//Recebe os centroides
	MPI_Recv(&ivfpq, sizeof(ivfpq_t), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	ivfpq.pq.centroids = (float*)malloc(sizeof(float)*ivfpq.pq.centroidsn*ivfpq.pq.centroidsd);
	MPI_Recv(&ivfpq.pq.centroids[0], ivfpq.pq.centroidsn*ivfpq.pq.centroidsd, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	ivfpq.coa_centroids=(float*)malloc(sizeof(float)*ivfpq.coa_centroidsd*ivfpq.coa_centroidsn);
	MPI_Recv(&ivfpq.coa_centroids[0], ivfpq.coa_centroidsn*ivfpq.coa_centroidsd, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	// Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 
       
    // Forcefully attaching socket to the port 8080 
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(server_fd, 3) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }
	
	while (1){
		if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                       (socklen_t*)&addrlen))<0) 
		{ 
			perror("accept"); 
			exit(EXIT_FAILURE); 
		} 

		//Le os vetores de consulta
		//vquery = pq_test_load_query(dataset, threads);

		vquery = pq_test_receive_query(dataset, new_socket);

		//Calcula o residuo de cada vetor da query para os processos de busca
		residual.d = vquery.d;
		residual.n = vquery.n*w;
		residual.mat = (float*)malloc(sizeof(float)*residual.d*residual.n);

		coaidx = (int*)malloc(sizeof(int)*vquery.n*w);
		coadis = (float*)malloc(sizeof(float)*vquery.n*w);
		knn_full(2, vquery.n, ivfpq.coa_centroidsn, ivfpq.coa_centroidsd, w, ivfpq.coa_centroids, vquery.mat, NULL, coaidx, coadis);

		free(coadis);

		for(int i=0;i<vquery.n; i++){
			for(int j=0;j<w;j++){
				id=i*w+j;
				bsxfunMINUS(&residual.mat[residual.d*id], vquery, ivfpq.coa_centroids, i, &coaidx[id], 1);
			}
		}

		//Envia os identificadores dos centroides correspondentes a cada vetor da query para o agregador
		for(int i=last_search+1; i<=last_aggregator; i++){
			MPI_Send(&vquery.n, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
		free(vquery.mat);


		int div=1;
		int num_q=residual.n;
		int num_qw = num_q/w;
		int finish=0;

		MPI_Barrier(search_comm);
		MPI_Bcast(&num_qw, 1, MPI_INT, 0, search_comm);
		double start = MPI_Wtime();
		MPI_Send(&start, 1, MPI_DOUBLE, last_aggregator, 0, MPI_COMM_WORLD);
		for(int j=0; j<div; j++){

			//Envia o resíduo para o processo de busca
			MPI_Bcast(&num_q, 1, MPI_INT, 0, search_comm);
			MPI_Bcast(&residual.d, 1, MPI_INT, 0, search_comm);
			MPI_Bcast(&residual.mat[0]+j*num_q*residual.d, residual.d*num_q, MPI_FLOAT, 0, search_comm);
			MPI_Bcast(&coaidx[0]+j*num_q, num_q, MPI_INT, 0, search_comm);
			if(j==div-1)finish=1;
			MPI_Bcast(&finish, 1, MPI_INT, 0, search_comm);
		}
		free(coaidx);
		free(residual.mat);

		int k;
		int *ids;

		MPI_Recv(&k, 1, MPI_INT, last_aggregator, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		ids = (int*) malloc(sizeof(int)*k);
		MPI_Recv(ids, k, MPI_INT, last_aggregator, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		send(new_socket, ids, sizeof(int)*k, 0);
		close(new_socket); 
	}
	free(ivfpq.pq.centroids);
	free(ivfpq.coa_centroids);
}

void bsxfunMINUS(float *mout, mat vin, float* vin2, int nq, int* qcoaidx, int ncoaidx){
	for (int i = 0; i < vin.d; i++) {
		for (int j = 0; j < ncoaidx; j++) {
			mout[j*vin.d+i] = vin.mat[(vin.d*nq) + i] - vin2[(qcoaidx[j]*vin.d)+i];
		}
	}
}

mat pq_test_receive_query(char *dataset, int new_socket){
	mat vquery;
	int valread;

	if(strcmp(dataset, "flickr")==0){
		char buffer[MAX_BUF] = {0};
		unsigned char *image = (unsigned char *)malloc(sizeof(char));
		int size = 0;

		do {
			valread = read( new_socket , buffer, MAX_BUF); 
			if(valread == 0 && size > 0){
				break;
			}else if(valread > 0){
				image = (unsigned char *) realloc (image, size + valread);
				memcpy(image + size, buffer, valread);
				size += valread;
			}
		} while(1);

		vquery.mat = compute_gist(image, NULL, size);
		vquery.n=1;
		vquery.d=960;

		free(image);
	}
	return vquery;
}