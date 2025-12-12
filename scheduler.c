#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#define MAX_TASKS  3
#define MAX_K      8

#define SIM_TIME_MS  20000.0

#define BUSY_LOOP_FACTOR 1000


enum SchedulerType { SCHED_RMS, SCHED_EDF, SCHED_MK };

typedef enum {
    SCHED_RMS,
    SCHED_EDF,
    SCHED_MK
} scheduler_t;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void sleep_ms(double ms)
{
    if (ms <= 0.0) return;
    usleep((useconds_t)(ms * 1000.0));
}


static void busy_work(double exec_ms)
{
    double end = now_ms() + exec_ms;
    volatile uint32_t x = 0;
    while (now_ms() < end) {
   
        for (int i = 0; i < BUSY_LOOP_FACTOR; ++i) {
            x += (uint32_t)i;
        }
    }
}


static bool mk_can_skip(rt_task_t *t)
{
    if (t->k <= 0 || t->m <= 0) return false;
    return (t->window_successes >= t->m);
}


static int rms(int ready_indices[], int n)
{
    if (n == 0) return -1;
    int best = ready_indices[0];
    double best_period = tasks[best].period_ms;

    for (int i = 1; i < n; ++i) {
        int idx = ready_indices[i];
        if (tasks[idx].period_ms < best_period) {
            best = idx;
            best_period = tasks[idx].period_ms;
        }
    }
    return best;
}

static int edf(int ready_indices[], int n)
{
    if (n == 0) return -1;
    int best = ready_indices[0];
    double best_deadline = tasks[best].abs_deadline_ms;

    for (int i = 1; i < n; ++i) {
        int idx = ready_indices[i];
        if (tasks[idx].abs_deadline_ms < best_deadline) {
            best = idx;
            best_deadline = tasks[idx].abs_deadline_ms;
        }
    }
    return best;
}

static int pick_mk_firm(int ready_indices[], int n)
{
    if (n == 0) return -1;

    int filtered[MAX_TASKS];
    int nf = 0;

    for (int i = 0; i < n; ++i) {
        int idx = ready_indices[i];
        rt_task_t *t = &tasks[idx];

        if (!mk_can_skip(t)) {
            filtered[nf++] = idx;
        } else {
            //drop job
            mk_advance_window(t, false);
            t->active_job = false;
            t->jobs_finished++;
            t->deadlines_missed++;  //miss
        }
    }

    if (nf == 0) return -1;

 //EDF on remaining
    int best = filtered[0];
    double best_deadline = tasks[best].abs_deadline_ms;

    for (int i = 1; i < nf; ++i) {
        int idx = filtered[i];
        if (tasks[idx].abs_deadline_ms < best_deadline) {
            best = idx;
            best_deadline = tasks[idx].abs_deadline_ms;
        }
    }
    return best;
}

static void run_scheduler(scheduler_t sched)
{
    double sim_start = now_ms();
    double sim_end   = sim_start + SIM_TIME_MS;

    while (now_ms() < sim_end) {
        double now = now_ms();
        int ready_indices[MAX_TASKS];
        int n_ready = build_ready_list(ready_indices, now);

        int idx = -1;
        switch (sched) {
            case SCHED_RMS: idx = pick_rms(ready_indices, n_ready); break;
            case SCHED_EDF: idx = pick_edf(ready_indices, n_ready); break;
            case SCHED_MK:  idx = pick_mk_firm(ready_indices, n_ready); break;
        }

        if (idx < 0) {
     
            sleep_ms(0.5);
            continue;
        }

        rt_task_t *t = &tasks[idx];

        double release_time = t->abs_deadline_ms - t->deadline_ms;
        double start = now_ms();


        double finish = now_ms();

        t->jobs_finished++;
        t->total_exec_ms += (finish - start);
        t->total_response_ms += (finish - release_time);

        if (finish > t->abs_deadline_ms)
            t->deadlines_missed++;

        if (sched == SCHED_MK)
            mk_advance_window(t, true);

        t->active_job = false;
    }
}

static void mk_advance_window(rt_task_t *t, bool executed)
{
    if (t->k <= 0) return;

    uint8_t old = t->history[t->window_pos];
    if (old == 1 && t->window_successes > 0)
        t->window_successes--;

    t->history[t->window_pos] = executed ? 1 : 0;
    if (executed)
        t->window_successes++;

    t->window_pos = (t->window_pos + 1) % t->k;
}


