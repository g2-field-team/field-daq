#include <cstdio>
#include <unistd.h>
#include "midas.h"

int main(int argc, char *argv[])
{
  printf("allocations\n");
  INT rc, count, size;
  HNDLE buf, req_id;
  char data[0x100000];
  size = sizeof(data);

  // Connect to the running experiment
  cm_connect_experiment("g2-field-test", "g2-field", "Test", NULL);
  printf("connected\n");

  rc = bm_open_buffer("SYSTEM", 0x4000000, &buf);
  printf("bm_open_buffer rc = %i\n", rc);

  count = 0;
  while (count < 10) {

    usleep(5e5);

    rc = bm_request_event(buf, 0x1, 0x0, GET_NONBLOCKING, &req_id, NULL);
    printf("bm_request_event rc, id = %i, %i\n", rc, req_id);

    if (rc == BM_SUCCESS) {

      size = sizeof(data);
      rc = bm_receive_event(buf, data, &size, BM_NO_WAIT);
      printf("bm_receive_event rc, size = %i, %i\n", rc, size);
         
      if (rc == BM_SUCCESS) {
	++count;
	printf("got event\n");
      }
    }
  }
  
  cm_disconnect_experiment();

  return 0;
}
