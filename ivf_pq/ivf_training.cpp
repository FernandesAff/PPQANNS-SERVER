#include "ivf_training.h"
#include "compute_gist.h"
#include "gist.h"
#include "standalone_image.h"

void parallel_training (char *dataset, int coarsek, int nsq, long long tam, int comm_sz, int threads){
	mat vtrain;
	ivfpq_t ivfpq;
	char invertedFile[100];
	char centFile[100];
	char coaFile[100];
	static int last_assign, last_search, last_aggregator;

	set_last (comm_sz, &last_assign, &last_search, &last_aggregator);

	sprintf(invertedFile, "bin/file_ivfpq_%d.bin", coarsek);
	sprintf(centFile, "bin/cent_ivfpq_%d.bin", coarsek);
	sprintf(coaFile, "bin/coa_ivfpq_%d.bin", coarsek);

	// Cria os centroides baseado em uma base de treinamento e os armazena em arquivos
	#ifdef TRAIN
		if(strcmp(dataset,"flickr") == 0){
			write_descriptors(dataset);
		}
		vtrain = pq_test_load_train(dataset, tam);

		ivfpq = ivfpq_new(coarsek, nsq, vtrain, threads);

		write_cent(invertedFile, centFile, coaFile, ivfpq);

		free(vtrain.mat);
		free(ivfpq.pq.centroids);
		free(ivfpq.coa_centroids);

		//Le ou cria os centroides e os envia para os processos de assign
	#else
    		int my_rank;

		MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

		//Le os centroides de um arquivo
		#ifdef READ_TRAIN

			read_cent(invertedFile, centFile, coaFile, &ivfpq);

 		//Cria centroides a partir dos vetores de treinamento
		#else
			if(strcmp(dataset,"flickr") == 0){
				write_descriptors(dataset);
			}
			vtrain = pq_test_load_train(dataset, tam);
			ivfpq = ivfpq_new(coarsek, nsq, vtrain, threads);
			free(vtrain.mat);
		#endif
		
		//Envia os centroides para os processos de recebimento da query e de indexação e busca
		for(int i=1; i<=last_search; i++){
			MPI_Send(&ivfpq, sizeof(ivfpq_t), MPI_BYTE, i, 0, MPI_COMM_WORLD);
			MPI_Send(&ivfpq.pq.centroids[0], ivfpq.pq.centroidsn*ivfpq.pq.centroidsd, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
			MPI_Send(&ivfpq.coa_centroids[0], ivfpq.coa_centroidsn*ivfpq.coa_centroidsd, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
		}

		free(ivfpq.pq.centroids);
		free(ivfpq.coa_centroids);
		
		

	#endif
}

ivfpq_t ivfpq_new(int coarsek, int nsq, mat vtrain, int threads){

	ivfpq_t ivfpq;
	ivfpq.coarsek = ivfpq.coa_centroidsn = coarsek;
	ivfpq.coa_centroidsd = vtrain.d;
	ivfpq.coa_centroids = (float*)malloc(sizeof(float)*ivfpq.coa_centroidsn*ivfpq.coa_centroidsd);

	float * dis = (float*)malloc(sizeof(float)*vtrain.n);

	//definicao de variaveis
	int flags = 0;
	flags = flags | KMEANS_INIT_BERKELEY;
	flags |= 1;
	flags |= KMEANS_QUIET;
	flags |= threads;
	int* assign = (int*)malloc(sizeof(int)*vtrain.n);

	kmeans(vtrain.d, vtrain.n, coarsek, 50, vtrain.mat, flags, 2, 1, ivfpq.coa_centroids, NULL, NULL, NULL);

	//calculo do vetores residuais
	knn_full(L2, vtrain.n, ivfpq.coa_centroidsn, ivfpq.coa_centroidsd, 1, ivfpq.coa_centroids, vtrain.mat, NULL, assign, dis);
	subtract(vtrain, ivfpq.coa_centroids, assign, ivfpq.coa_centroidsd);

	//aprendizagem do produto residual
	ivfpq.pq = pq_new(nsq, vtrain, coarsek, threads);

	free(assign);
	free(dis);

	return ivfpq;
}

void write_cent(char *invertedFile, char *centFile, char *coaFile, ivfpq_t ivfpq){
	FILE *arq, *arq2, *arq3;

	arq = fopen(invertedFile, "wb");
	arq2 = fopen(centFile, "wb");
	arq3 = fopen(coaFile, "wb");

	if (arq == NULL){
   	printf("Problemas na CRIACAO do arquivo\n");
  	return;
  }

  fwrite (&ivfpq.pq.nsq, sizeof(int), 1, arq);
  fwrite (&ivfpq.pq.ks, sizeof(int), 1, arq);
  fwrite (&ivfpq.pq.ds, sizeof(int), 1, arq);
  fwrite (&ivfpq.pq.centroidsn, sizeof(int), 1, arq);
  fwrite (&ivfpq.pq.centroidsd, sizeof(int), 1, arq);
  fwrite (&ivfpq.coarsek, sizeof(int), 1, arq);
  fwrite (&ivfpq.coa_centroidsn, sizeof(int), 1, arq);
  fwrite (&ivfpq.coa_centroidsd, sizeof(int), 1, arq);
  fwrite (&ivfpq.pq.centroids[0], sizeof(float), ivfpq.pq.centroidsn*ivfpq.pq.centroidsd, arq2);
  fwrite (&ivfpq.coa_centroids[0], sizeof(float), ivfpq.coa_centroidsn*ivfpq.coa_centroidsd, arq3);

  fclose(arq);
  fclose(arq2);
  fclose(arq3);
}

void read_cent(char *invertedFile, char *centFile, char *coaFile, ivfpq_t *ivfpq){
	FILE *arq, *arq2, *arq3;



	arq = fopen(invertedFile, "rb");
	arq2 = fopen(centFile, "rb");
	arq3 = fopen(coaFile, "rb");


	fread (&ivfpq->pq.nsq, sizeof(int), 1, arq);
	fread (&ivfpq->pq.ks, sizeof(int), 1, arq);
	fread (&ivfpq->pq.ds, sizeof(int), 1, arq);
	fread (&ivfpq->pq.centroidsn, sizeof(int), 1, arq);
	fread (&ivfpq->pq.centroidsd, sizeof(int), 1, arq);
	fread (&ivfpq->coarsek, sizeof(int), 1, arq);
 	fread (&ivfpq->coa_centroidsn, sizeof(int), 1, arq);
	fread (&ivfpq->coa_centroidsd, sizeof(int), 1, arq);
	ivfpq->pq.centroids = (float *) malloc(sizeof(float)*ivfpq->pq.centroidsn*ivfpq->pq.centroidsd);
	fread (&ivfpq->pq.centroids[0], sizeof(float), ivfpq->pq.centroidsn*ivfpq->pq.centroidsd, arq2);
	ivfpq->coa_centroids = (float *) malloc(sizeof(float)*ivfpq->coa_centroidsn*ivfpq->coa_centroidsd);
	fread (&ivfpq->coa_centroids[0], sizeof(float), ivfpq->coa_centroidsn*ivfpq->coa_centroidsd, arq3);

	fclose(arq);
	fclose(arq2);
	fclose(arq3);
}

void subtract(mat v, float* v2, int* idx, int c_d){
	for (int i = 0; i < v.d; i++) {
		for (int j = 0; j < v.n; j++) {
			v.mat[j*v.d + i] = v.mat[j*v.d + i] - v2[idx[j]*c_d + i];
		}
	}
}

void write_descriptors(char* dataset){
	char* path = (char*)malloc(sizeof(char)*100);
	char* desc_path = (char*)malloc(sizeof(char)*100);
	int n;
	int d;
	FILE *fp;

	if(strcmp(dataset, "flickr") == 0){
		n = 50000;
		d = 960;

		sprintf(desc_path, "./datasets/%s/flickr_base_50000.fvecs1", dataset);

		fp = fopen(desc_path, "w");

		#pragma omp parallel for num_threads(1)
		for(int i = 0; i < n; i++){
			
			float *desc;
			sprintf(path, "./datasets/%s/%d.jpeg", dataset, i);
			cout << i << endl;
			desc = compute_gist((unsigned char*)path, NULL);

			#pragma omp critical
			{
				fwrite(&d,sizeof(int),1,fp);
				fwrite(desc,sizeof(float),d,fp);
				free(desc);
			}
			
		}
		fclose(fp);

		copy_file("./datasets/flickr/flickr_base_50000.fvecs1", "./datasets/flickr/flickr_learn_50000.fvecs");
	}
}

void copy_file(char *src, char *dest){
	FILE *fp1, *fp2;
	char ch;
    int pos;

    if ((fp1 = fopen(src,"r")) == NULL){    
        printf("\nFile cannot be opened");
        return;
    }else {
        printf("\nFile opened for copy...\n ");    
    }
	fp2 = fopen(dest, "w");  

	fseek(fp1, 0L, SEEK_END); // file pointer at end of file
	pos = ftell(fp1);
	fseek(fp1, 0L, SEEK_SET); // file pointer set at start

	while (pos--){
		ch = fgetc(fp1);  // copying file character by character
		fputc(ch, fp2);
	}    

	fcloseall();
}