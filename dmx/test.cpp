#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>
#include <iostream>

#define PI 3.14159

using std::cout;
using std::endl;
int main(int, char *[]) {
 unsigned int universe = 1; // universe to use for sending data
 // turn on OLA logging
 ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);
 ola::DmxBuffer buffer; // A DmxBuffer to hold the data.
 buffer.Blackout(); // Set all channels to 0
 // Create a new client.
 ola::client::StreamingClient ola_client(
 (ola::client::StreamingClient::Options()));
 // Setup the client, this connects to the server
 if (!ola_client.Setup()) {
 std::cerr << "Setup failed" << endl;
 exit(1);
 }
 // Send 100 frames to the server. Increment slot (channel) 0 each time a
 // frame is sent.


buffer.SetChannel(7,255);
buffer.SetChannel(6,0);
buffer.SetChannel(9,0);
buffer.SetChannel(10,0);

while(true){
for (unsigned int i = 0; i < 2 * PI * 20 ; i+=2) {
   buffer.SetChannel(4, i);
   buffer.SetChannel(5,i);
   buffer.SetChannel(0,(int)((sin(i/20.)/2.+.5)*255));
   buffer.SetChannel(2,(int)((sin(i/20.)/2.+.5)*255));
   if (!ola_client.SendDmx(universe, buffer)) {
     cout << "Send DMX failed" << endl;
     exit(1);
   }
//   usleep(25000); // sleep for 25ms between frames.
   usleep(75000); // sleep for 25ms between frames.
 }
}
return 0;
}
