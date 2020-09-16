__kernel void histogram(__global uchar *image, __global unsigned int *R, __global unsigned int *G, __global unsigned int *B, int rgb_num) {
    int idx = get_global_id(0);
    int s = (idx % rgb_num);
    unsigned int rgb = image[idx]&0xFF;
    if(s==0){
        atomic_inc(&R[rgb]);
    } else if(s==1) {
        atomic_inc(&G[rgb]);
    } else if(s==2) {
        atomic_inc(&B[rgb]);
    }
}