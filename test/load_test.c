#include <stdio.h>
#include <time.h>
#include <math.h>

// Function to check if a number is prime
int is_prime(int num) {
    if (num <= 1) return 0;
    for (int i = 2; i * i <= num; i++) {
        if (num % i == 0) return 0;
    }
    return 1;
}

int main() {
    clock_t start_time = clock();
    double time_spent = 0.0;
    long long count = 0;
    int num = 2;

    // Loop until approximately 2 seconds have passed
    while (time_spent < 2.0) {
        if (is_prime(num)) {
            count++;
        }
        num++;
        time_spent = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    }

    printf("Found %lld prime numbers in %.2f seconds.\n", count, time_spent);

    return 0;
}
