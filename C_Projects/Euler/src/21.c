#include "Euler.h"

int64_t Euler_21()
{
    int N = 10000;

    int sums[N + 1];
    long n, s;
    for (n = 0; n <= N; n++)
        sums[n] = 0;
    for (n = 1; n <= N; n++)
        for (s = 2; s * n <= N; s++)
            sums[n * s] += n;

    int total = 0;
    for (n = 1; n <= N; n++) {
        if (sums[n] > N || sums[n] == n)
            continue;
        if (sums[sums[n]] == n) {
            total += sums[n] + sums[sums[n]];
            sums[n] = N + 1;
        }
    }
    return (int64_t) total;
}

