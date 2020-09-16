/**********************************************************************
 * DESCRIPTION:
 *   Serial Concurrent Wave Equation - C Version
 *   This program implements the concurrent wave equation
 *********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define MAXPOINTS 1000000
#define MAXSTEPS 1000000
#define MINPOINTS 20
#define PI 3.14159265

void check_param(void);
void init_line(void);
void update (void);
void printfinal (void);

int nsteps,                 	/* number of time steps */
    tpoints, 	     		/* total points along string */
    rcode;                  	/* generic return code */
float   *values;
float   *oldval;
float   *newval;


/**********************************************************************
 *	Checks input values from parameters
 *********************************************************************/
void check_param(void)
{
   char tchar[20];

   /* check number of points, number of iterations */
   while ((tpoints < MINPOINTS) || (tpoints > MAXPOINTS)) {
      printf("Enter number of points along vibrating string [%d-%d]: "
           ,MINPOINTS, MAXPOINTS);
      scanf("%s", tchar);
      tpoints = atoi(tchar);
      if ((tpoints < MINPOINTS) || (tpoints > MAXPOINTS))
         printf("Invalid. Please enter value between %d and %d\n", 
                 MINPOINTS, MAXPOINTS);
   }
   while ((nsteps < 1) || (nsteps > MAXSTEPS)) {
      printf("Enter number of time steps [1-%d]: ", MAXSTEPS);
      scanf("%s", tchar);
      nsteps = atoi(tchar);
      if ((nsteps < 1) || (nsteps > MAXSTEPS))
         printf("Invalid. Please enter value between 1 and %d\n", MAXSTEPS);
   }

   printf("Using points = %d, steps = %d\n", tpoints, nsteps);

}

/**********************************************************************
 *     Initialize points on line
 *********************************************************************/
__global__ void init_line(int __tpoints, float* __oldval, float* __newval) // host call, device exec
{
   int i = blockIdx.x * blockDim.x + threadIdx.x;

   if (i < __tpoints) {
      float x = (float)i / (__tpoints-1);
      __oldval[i] = __newval[i] = __sinf(2.0 * PI * x);
    }
}

/**********************************************************************
 *      Calculate new values using wave equation
 *********************************************************************/
/*__device__ float do_math(float __newval, float __oldval)
{
   float dtime, c, dx, tau, sqtau;

   dtime = 0.3;
   c = 1.0;
   dx = 1.0;
   tau = (c * dtime / dx);
   sqtau = tau * tau;
   return (2.0 * __newval) - __oldval + (sqtau *  (-2.0)*__newval);
}*/

/**********************************************************************
 *     Update all values along line a specified number of times
 *********************************************************************/
__global__ void update(int __tpoints, int __nsteps, float* __oldval, float* __newval)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    /* Update values for each time step */
    if(i< __tpoints) {
        float locPrevVal = __oldval[i], locCurrVal = __newval[i] , locNextVal;
		for (int j = 0; j < __nsteps; j++) {
			if ((i == 0) || (i == __tpoints - 1))
				locNextVal = 0.0;
			else
				locNextVal = 1.82 * locCurrVal - locPrevVal;
			locPrevVal = locCurrVal;
			locCurrVal = locNextVal;
		}
		__newval[i] = locCurrVal;
    }
}

/**********************************************************************
 *     Print final results
 *********************************************************************/
void printfinal()
{
   int i;

   for (i = 1; i <= tpoints; i++) {
      printf("%6.4f ", values[i]);
      if (i%10 == 0)
         printf("\n");
   }
}

/**********************************************************************
 *	Main program
 *********************************************************************/
int main(int argc, char *argv[])
{
	sscanf(argv[1],"%d",&tpoints);
	sscanf(argv[2],"%d",&nsteps);

    // memory allocation
    cudaMalloc(&oldval, (tpoints+256) * sizeof(float));
    cudaMalloc(&newval, (tpoints+256) * sizeof(float));
    values = (float*) malloc(sizeof(float) * (tpoints+256));

    // kernel parameter init
    dim3 blockSize(256);
	dim3 numBlocks((tpoints+256)/256);
   //int blockSize = 256;
    //int numBlocks = (tpoints + blockSize + 1) / blockSize; 

	check_param();

	printf("Initializing points on the line...\n");
    // To prevent data transfer, redesign the func only use newval & oldval
	init_line<<<numBlocks, blockSize>>>(tpoints, oldval, newval);
	printf("Updating all points for all time steps...\n");
	update<<<numBlocks, blockSize>>>(tpoints, nsteps, oldval, newval);

    //cudaDeviceSynchronize();

    cudaMemcpy(values, newval, sizeof(float) * tpoints, cudaMemcpyDeviceToHost);

	printf("Printing final results...\n");
	printfinal();
	printf("\nDone.\n\n");

    cudaFree(newval);
	cudaFree(oldval);
	free(values);

	
	return 0;
}