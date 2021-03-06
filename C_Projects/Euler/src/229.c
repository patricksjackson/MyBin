#include "Euler.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Currently does not work
int64_t Euler_229()
{
    //printf("USES 1GB MEMORY\nARE YOU SURE YOU WANT TO?\nEDIT FILE TO CONTINUE\n");
    const uint64_t memsize = (1 << 20);
    uint64_t N = 10000000; //2000000000;
    unsigned char* list = (unsigned char*) calloc((memsize + 1) >> 1,
                          sizeof(unsigned char));
    if (!list) {
        printf("Memory Fail\n");
        return 0;
    }
    uint64_t x, y;
    uint64_t tmp;
    int shift;
    int count = 0;
    uint64_t tmpx;
    int vals[4] = {1, 2, 3, 7};
    uint64_t low, high = memsize;
    for (low = 0; low <= N; low += memsize, high += memsize) {
	if (high > N)
	    high = N;
	for (x = 1; x * x <= high; x++) {
	    tmpx = x * x;
	    int index;
	    for (index = 0; index < 4; index++) {
		y = (int)sqrt(low-tmpx*vals[index]) + 1;
		tmp = y * y + (vals[index] * tmpx);
		while (tmp <= high) {
		    shift = 0;
		    if (tmp & 1)
			shift = 4;
		    list[(tmp&(memsize-1)) >> 1] |= (1 << index) << shift;

		    y++;
		    tmp = y * y + (vals[index] * tmpx);
		}
		
	    }
	    /*
	    y = (int)sqrt((low-tmpx) / 7)+1;
	    if (tmpx + 7*y*y <= low)
		printf("wrong\n");
	    y = 1;
	    

	    while (1) {
		uint64_t tmpy = y * y;
		y++;

		tmp = tmpx + tmpy;
		if (tmp > high)
		    break;
		if (tmp > low) {
		    shift = 0;
		    if (tmp & 1)
			shift = 4;
		    list[(tmp&(memsize-1)) >> 1] |= 1 << shift;
		}

		//tmp = tmpx + (tmpy << 1);
		tmp += tmpy;
		if (tmp > high)
		    continue;
		if (tmp > low) {
		    shift = 0;
		    if (tmp & 1)
			shift = 4;
		    list[(tmp&(memsize-1)) >> 1] |= 2 << shift;
		}

		//tmp = tmpx + (tmpy * 3);
		tmp += tmpy;
		if (tmp > high)
		    continue;
		if (tmp > low) {
		    shift = 0;
		    if (tmp & 1)
			shift = 4;
		    list[(tmp&(memsize-1)) >> 1] |= 4 << shift;
		}

		//tmp = tmpx + (tmpy * 7);
		tmp += tmpy * 4;
		if (tmp > high)
		    continue;
		if (tmp > low) {
		    shift = 0;
		    if (tmp & 1)
			shift = 4;
		    list[(tmp&(memsize-1)) >> 1] |= 8 << shift;
		}
	    }
	    */
	}
	for (x = 0; x <= memsize; x++) {
	    int tmp = x >> 1;
	    int shift = 0;
	    if (x & 1)
		shift = 4;
	    if ((list[tmp] & (15 << shift)) == 15 << shift)
		count++;
	    if (shift == 4)
		list[tmp] = 0;
	}
    }
    free(list);
    return (int64_t) count;
}
