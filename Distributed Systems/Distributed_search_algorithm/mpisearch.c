#include "mpisearch.h"


search_result* linear_search(int* query_list, int qcount, int* data, int size) {
        search_result  *result = (search_result*) malloc(sizeof(search_result));
        int k=0;
        result->list = (int*)malloc(sizeof(int) * size);
        for(int i=0;i<qcount;i++)
                for(int j=0;j<size;j++)
                        if(query_list[i] == data[j]){
                                result->list[k++] = data[j];
                                break;
                        }
        result->count=k;
        return  result;
}

void print_found(int *data, int size, int source) {
        printf("\nFound query data items from slave %d, count = %d:\n",source, size);
        for (int i = 0;i < size;i++) {
                printf("%d\t",data[i]);
        }
        printf("\n\n");
        return;
}

int main(int argc, char *argv[]) {
        int np;                 // number of processes
        int myrank;             // rank of process
        MPI_Init(&argc, &argv);
        MPI_Comm_size(comm, &np);

        int slave_count = np-1;
        MPI_Comm_rank(comm, &myrank);
        //printf("\nNo. of procs = %d, proc ID = %d initialized...\n", np, myrank);
        MPI_Status statuses[slave_count];
        MPI_Request requests[slave_count];

                /*MASTER LOGIC*/
        if (0 == myrank) {
                MPI_Request request;

                // Create an array of random integers for test purpose
                int *search_array = (int *) malloc(SIZE * sizeof(int));
                for (int i = 0;i < SIZE;i++) {
                        search_array[i] = OFFSET + rand() % RANGE;
                }

                // Now create an array of search queries
                int *query_vector = (int *) malloc(QUERY_SIZE * sizeof(int));
                for (int i = 0;i < QUERY_SIZE;i++) {
                        query_vector[i] = OFFSET + rand() % RANGE;
                }

                //Perform linear search on all search_array elements
                int result_lin_vector[SIZE],k=0;
                double start_lin_time = MPI_Wtime();
                for(int i=0;i<SIZE;i++)
                        for(int j=0;j<QUERY_SIZE;j++)
                                if(search_array[i]==query_vector[j]){
                                        result_lin_vector[k++] = query_vector[j];
                                        break;
                                }
                printf("\nMaster performed linear search and identified %d matches\n",k);


                double end_lin_time = MPI_Wtime();

                double start_time = MPI_Wtime();

                // Send the query vector to all slaves and wait for an acknowledgement
                int ack;
                for(int i = 1;i<np;i++){
                        fflush(stdout);
                        MPI_Isend(query_vector, QUERY_SIZE, MPI_INT, i, QUERY_MSG_TAG, comm, &request);
                        MPI_Irecv(&ack, 1, MPI_INT, i, ACK_MSG_TAG, comm, &requests[i-1]);

                }
                MPI_Waitall(slave_count,requests,statuses);

                //Dividing the search array into chunks and send each chunk to one slave
                int  part = SIZE/(slave_count);
                for(int i=1;i<np;i++)
                {
                        int * start = search_array + (i-1)*part;
                        if(i==slave_count)
                                part = SIZE - (i-1)*part;
                        MPI_Isend(start, part, MPI_INT, i, QUERY_MSG_TAG, comm, &requests[i-1]);
                }

                //Initiating receive calls for the result_vector  from each slave
                int result_count[slave_count];
                int** result_vector = (int**) malloc((slave_count) * sizeof(int*));
                MPI_Status status;
                int current_source;
                for(int i=1;i<np;i++){
                        MPI_Probe(MPI_ANY_SOURCE,RESULT_MSG_TAG, comm, &status);
                        current_source = status.MPI_SOURCE;
                        MPI_Get_count(&status, MPI_INT, &result_count[current_source-1]);
                        result_vector[current_source-1] = (int *) malloc(result_count[current_source-1] * sizeof(int));
                        MPI_Irecv(result_vector[current_source-1],result_count[current_source-1], MPI_INT, current_source ,RESULT_MSG_TAG, comm,&requests[i-1]);
                        printf("Received result Vector from process %d and has %d elements\n\n",current_source,result_count[current_source-1]);fflush(stdout);
                        /*Uncomment next line to get all matching elements from slave*/
                        //print_found(result_vector[current_source-1], result_count[current_source-1], current_source);

                }
                double end_time = MPI_Wtime();
                printf("All matched elements are hidden intentionally to make the output look clean. Uncomment above line in code to print the output\n\n");fflush(stdout);

                // Freeing memory
                for(int index=0;index<slave_count;index++){

                        free(result_vector[index]);
                }
                free(result_vector);
                free(search_array);
                free(query_vector);
                printf("\nslave_count - %d, distributed_time - %f, linear_time %f\n",slave_count,end_time-start_time,end_lin_time-start_lin_time);

        } else {

                //Receives query vector
                MPI_Status status;
                MPI_Probe(0,QUERY_MSG_TAG, comm, &status);
                int query_count;
                MPI_Get_count(&status, MPI_INT, &query_count);
                int * query_vector = (int *) malloc(query_count * sizeof(int));
                MPI_Recv(query_vector, query_count, MPI_INT, 0,QUERY_MSG_TAG, comm, &status);

                //Send an acknowledgement to Master
                MPI_Send(&myrank, 1, MPI_INT, 0, ACK_MSG_TAG, comm);
                fflush(stdout);


                //Waits for the search vector
                MPI_Probe(0,QUERY_MSG_TAG, comm, &status);
                int search_count;
                MPI_Get_count(&status, MPI_INT, &search_count);
                int * search_vector = (int *) malloc(search_count * sizeof(int));
                MPI_Recv(search_vector, search_count, MPI_INT, 0,QUERY_MSG_TAG, comm, &status);


                //Linear search on a partition of the search vector

               int result_vector[search_count],k=0;
                for(int i=0;i<search_count;i++){
                        for(int j=0;j<query_count;j++){
                                if(search_vector[i]==query_vector[j]){
                                        result_vector[k++]=query_vector[j];
                                        break;
                                }
                        }
                }

                printf("[Process %d]:Sending %d elements to master\n",myrank,k);fflush(stdout);
                MPI_Send(result_vector,k,MPI_INT,0,RESULT_MSG_TAG,comm);

        }
        MPI_Finalize();

        return 0;
}