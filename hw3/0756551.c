#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#ifndef W
#define W 20                                    // Width
#endif
int main(int argc, char **argv) {

  MPI_Init(&argc, &argv);

  // Get the number of processes
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Get the rank of the process
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Declare Type
  MPI_Datatype rowtype;
  MPI_Type_contiguous(W, MPI_INT, &rowtype);
  MPI_Type_commit(&rowtype);

  MPI_Request req;
  MPI_Status sta;


  int L = atoi(argv[1]);                        // Length
  int iteration = atoi(argv[2]);                // Iteration
  srand(atoi(argv[3]));                         // Seed
  float d = (float) random() / RAND_MAX * 0.2;  // Diffusivity
  int *temp = malloc(L*W*sizeof(int));          // Current temperature
  int *next = malloc(L*W*sizeof(int));          // Next time step

  for (int i = 0; i < L; i++) {
    for (int j = 0; j < W; j++) {
      temp[i*W+j] = random()>>3;
    }
  }

  int steps = L/world_size;
  int start = rank*steps;
  int end;
  if (rank!=(world_size-1)) {
    end = rank*steps + steps;
  } else {
    end = L;
  }
  
  //printf("rank: %d\t steps: %d\n", rank, steps);
  //printf("world size: %d\tstart: %d\tend: %d\n", world_size, start, end);
  
  int count = 0, balance = 0;
  while (iteration--) {     // Compute with up, left, right, down points
    balance = 1;
    count++;

    for (int i = start; i < end; i++) {
      for (int j = 0; j < W; j++) {
        float t = temp[i*W+j] / d;
        t += temp[i*W+j] * -4;
        t += temp[(i - 1 <  0 ? 0 : i - 1) * W + j];
        t += temp[(i + 1 >= L ? i : i + 1)*W+j];
        t += temp[i*W+(j - 1 <  0 ? 0 : j - 1)];
        t += temp[i*W+(j + 1 >= W ? j : j + 1)];
        t *= d;
        next[i*W+j] = t ;
        if (next[i*W+j] != temp[i*W+j]) {
          balance = 0;
        }
      }
    }


    int global_balance;
    MPI_Allreduce(&balance, &global_balance, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    if (global_balance) {
      break;
    }

    if (rank!=0) {
        MPI_Isend(&temp[start*W],1,rowtype,rank-1,1,MPI_COMM_WORLD,&req);
        MPI_Irecv(&temp[(start-1)*W],1,rowtype,rank-1,0,MPI_COMM_WORLD,&req);
		MPI_Wait(&req,&sta);
    }
    if (rank!=(world_size-1)) {
        MPI_Isend(&temp[(end-1)*W],1,rowtype,rank+1,0,MPI_COMM_WORLD,&req);
        MPI_Irecv(&temp[end*W],1,rowtype,rank+1,1,MPI_COMM_WORLD,&req);
		MPI_Wait(&req,&sta);
    }

    int *tmp = temp;
    temp = next;
    next = tmp;
  }
 
  int min = temp[start*W];
  for (int i = start; i < end; i++) {
    for (int j = 0; j < W; j++) {
      if (temp[i*W+j] < min) {
        min = temp[i*W+j];
      }
    }
  }
  
  //printf("rank: %d\tmin: %d\n", rank, min);
  int global_min=0;
  MPI_Reduce(&min, &global_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

  if (rank==0) {
   printf("Size: %d*%d, Iteration: %d, Min Temp: %d\n", L, W, count, global_min);
 }
  
  MPI_Type_free(&rowtype);
  // Finalize the MPI environment. No more MPI calls can be made after this
  MPI_Finalize();
  return 0;
}