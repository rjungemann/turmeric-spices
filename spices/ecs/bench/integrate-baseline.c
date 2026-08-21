/* integrate-baseline.c -- the hand-rolled comparison for the ECS
 * integrate benchmark.
 *
 * Same workload as bench/integrate-ecs.tur: N entities with float Pos
 * and Vel, integrated Pos += Vel * DT for FRAMES frames, then summed.
 * Two flat struct-of-arrays buffers, one flat loop, no dispatch and no
 * per-slot occupancy test -- the floor the ECS surface is measured
 * against. The archived ecs-spice-plan's target is "within 2x of
 * hand-rolled"; this is the denominator.
 *
 * Prints elapsed milliseconds, then the scaled checksum. The checksum
 * must match every Turmeric variant EXACTLY -- it is what proves the
 * comparison is measuring the same arithmetic and not, say, a loop the
 * optimiser deleted.
 *
 * Built by bench/run.sh with the same -O level tur hands to cc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N      100000
#define FRAMES 100
#define DT     0.25

typedef struct { double x, y; } Vec2;

static long long now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int main(void) {
    Vec2 *pos = malloc(sizeof(Vec2) * N);
    Vec2 *vel = malloc(sizeof(Vec2) * N);
    if (!pos || !vel) return 1;

    for (int i = 0; i < N; i++) {
        double f = (double)i;
        pos[i].x = f / 8.0;
        pos[i].y = f / 4.0;
        vel[i].x = 1.5;
        vel[i].y = -0.5;
    }

    long long t0 = now_us();
    for (int frame = 0; frame < FRAMES; frame++) {
        for (int i = 0; i < N; i++) {
            pos[i].x = pos[i].x + vel[i].x * DT;
            pos[i].y = pos[i].y + vel[i].y * DT;
        }
    }
    long long t1 = now_us();

    double sum = 0.0;
    for (int i = 0; i < N; i++) sum += pos[i].x + pos[i].y;

    printf("%lld\n", t1 - t0);
    printf("%lld\n", (long long)(sum * 100.0));

    free(pos);
    free(vel);
    return 0;
}
