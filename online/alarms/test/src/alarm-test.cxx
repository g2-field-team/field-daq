#include <cstdio>
#include "midas.h"

int main(int argc, char *argv[])
{
  INT rc;
  
  // Connect to the running experiment
  cm_connect_experiment("localhost", "g2-field", "Test", NULL);
  rc = al_trigger_class("Warning", "Test Warning Alarm", false);
  rc = al_trigger_class("Error", "Test Error Alarm", false);
  rc = al_trigger_class("Failure", "Test Failure Alarm", false);
  
  cm_disconnect_experiment();
  return 0;
}
