#include "CC_Euler.h"

int64 Euler_1() {
	int count;
	int sum = 0;
	for (count = 1; count < 1000; count++) {
		if (!(count % 3) || !(count % 5))
			sum += count;
	}
	return (int64) sum;
}
