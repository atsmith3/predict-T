#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "interprocess.h"

mapped* p = NULL;

#define NUM_SIGNALS 15

uint32_t clk_times[NUM_SIGNALS] = {0,         1,         2,3,4,5,6,7,8,9, 15,16,        17,18,25};
uint32_t nxt_times[NUM_SIGNALS] = {1,         2,         3,4,5,6,7,8,9,10,16,17,        18,19,26};
uint32_t a[NUM_SIGNALS] =         {0,0xabadcafe,0xdeadbeef,0,0,0,0,0,0,0,  0, 0,0xFFFF00FF, 0, 0};
uint32_t b[NUM_SIGNALS] =         {0,0xdeadbeef,0xabadcafe,0,0,0,0,0,0,0,  0, 0,0x0000AA00, 0, 0};
uint32_t start[NUM_SIGNALS] =     {0,         1,         0,0,0,0,0,0,0,0,  0, 0,         1, 0, 0};
uint32_t rst[NUM_SIGNALS] =       {1,         0,         0,0,0,0,0,0,1,0,  0, 0,         0, 0, 0};
uint32_t sim_over[NUM_SIGNALS] =  {0,         0,         0,0,0,0,0,0,0,0,  0, 0,         0, 0, 1};

static void int_handler(int signum) {
  destroy_shm(p);
  exit(-1);
}

int main(int argc, char** argv) {
  p = create_shm(INIT);

  // Install Signal Handler:
  struct sigaction sa;
  sa.sa_handler = int_handler;
  sigaction(SIGINT, &sa, NULL);

  int sent = 0;
  int done = 0;
  for(int i = 0; i < NUM_SIGNALS; i++) {
    sent = 0;
    while(sent == 0) {
      sem_wait(&p->pv.sem);
      if(p->pv.new_data == NO_NEW_DATA) {
        // We can send new data to the Verilog Sim:
        p->pv.new_data = NEW_DATA;
        sent = 1;
        p->pv.data.a = a[i];
        p->pv.data.b = b[i];
        p->pv.data.start = start[i];
        p->pv.data.rst = rst[i];
        p->pv.data.next_clk_cnt = nxt_times[i];
        p->pv.data.sim_over = sim_over[i];
      }
      sem_post(&p->pv.sem);
      sem_wait(&p->vp.sem);
      if(p->vp.new_data == NEW_DATA) {
        printf("Recieved: %u %u %u %u\n", p->vp.data.clk_cnt, p->vp.data.ready, p->vp.data.res, p->vp.data.overflow);
        p->vp.new_data = NO_NEW_DATA;
      }
      sem_post(&p->vp.sem);
    }
    printf("Sent signal packet %d\n",i);
  }
  // wait for the simulation to drain of events
  while(done == 0) {
    sem_wait(&p->vp.sem);
    if(p->vp.new_data == NEW_DATA) {
      printf("Recieved: %u %u %u %u\n", p->vp.data.clk_cnt, p->vp.data.ready, p->vp.data.res, p->vp.data.overflow);
      if(p->vp.data.sim_done) {
        done = 1;
      }
      p->vp.new_data = NO_NEW_DATA;
    }
    sem_post(&p->vp.sem);
  }
  destroy_shm(p);
  return 0;
}
