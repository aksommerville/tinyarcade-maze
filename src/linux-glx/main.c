#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void tinyc_quit();
int tinyc_quit_requested();

/* Signals.
 */
 
static volatile int sigc=0;

static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals, aborting.\n");
        exit(1);
      } break;
  }
}

/* Main.
 */

int main(int argc,char **argv) {
  signal(SIGINT,rcvsig);
  fprintf(stderr,"Starting...\n");
  setup();
  fprintf(stderr,"Begin main loop, SIGINT to exit...\n");
  while (!sigc&&!tinyc_quit_requested()) {
    loop();
  }
  tinyc_quit();
  fprintf(stderr," Normal exit\n");
  return 0;
}
