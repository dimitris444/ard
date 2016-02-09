#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>
#include <utility/w5100.h>

//ETHERNET INITIALISATION******************

// Enter a MAC address for your controller below.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 166);

//ETHERNET INITIALISATION******************

EthernetClient client;

double bintodec (String input)
{
  //int strlength = input.length();
  double  value = 0;
  for (int i = 0; i < 8; i++)
  {
    if (input.charAt(i) == '1')
    {
      value = value + pow(2, 7 - i);
    }
  }
  return value / 10;
}

// ring buffer size has to be large enough to fit
// data between two successive sync signals
#define RING_BUFFER_SIZE  100

#define SYNC_LENGTH  9000
#define SEP_LENGTH   500
#define BIT1_LENGTH  4000
#define BIT0_LENGTH  2000
#define INTERRUPT  1
#define DATAPIN  3  // D3 is interrupt 1

unsigned long timings[RING_BUFFER_SIZE];
unsigned int syncIndex1 = 0;  // index of the first sync signal
unsigned int syncIndex2 = 0;  // index of the second sync signal
bool received = false;

// detect if a sync signal is present
bool isSync(unsigned int idx) {
  unsigned long t0 = timings[(idx + RING_BUFFER_SIZE - 1) % RING_BUFFER_SIZE];
  unsigned long t1 = timings[idx];

  // on the temperature sensor, the sync signal
  // is roughtly 9.0ms. Accounting for error
  // it should be within 8.0ms and 10.0ms
  if (t0 > (SEP_LENGTH - 100) && t0 < (SEP_LENGTH + 100) &&
      t1 > (SYNC_LENGTH - 1000) && t1 < (SYNC_LENGTH + 1000) &&
      digitalRead(DATAPIN) == HIGH) {
    return true;
  }
  return false;
}

/* Interrupt 1 handler */
void handler() {
  static unsigned long duration = 0;
  static unsigned long lastTime = 0;
  static unsigned int ringIndex = 0;
  static unsigned int syncCount = 0;

  // ignore if we haven't processed the previous received signal
  if (received == true) {
    return;
  }
  // calculating timing since last change
  long time = micros();
  duration = time - lastTime;
  lastTime = time;

  // store data in ring buffer
  ringIndex = (ringIndex + 1) % RING_BUFFER_SIZE;
  timings[ringIndex] = duration;

  // detect sync signal
  if (isSync(ringIndex)) {
    syncCount ++;
    // first time sync is seen, record buffer index
    if (syncCount == 1) {
      syncIndex1 = (ringIndex + 1) % RING_BUFFER_SIZE;
    }
    else if (syncCount == 2) {
      // second time sync is seen, start bit conversion
      syncCount = 0;
      syncIndex2 = (ringIndex + 1) % RING_BUFFER_SIZE;
      unsigned int changeCount = (syncIndex2 < syncIndex1) ? (syncIndex2 + RING_BUFFER_SIZE - syncIndex1) : (syncIndex2 - syncIndex1);
      // changeCount must be 66 -- 32 bits x 2 + 2 for sync
      if (changeCount != 66) {
        received = false;
        syncIndex1 = 0;
        syncIndex2 = 0;
      }
      else {
        received = true;
      }
    }

  }

}

void setup() {
  Serial.begin(9600);
  //enable ethernet
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  Serial.println("Started....");
  pinMode(DATAPIN, INPUT);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);
/*

  // get default IMR
  byte oldIMR = W5100.readIMR();
 
  Serial.print("Old IMR = ");
  Serial.println(oldIMR,HEX);
 
  // enable interrupts for all sockets
  W5100.writeIMR(0x0F);

  // read again to insure it worked
  byte newIMR = W5100.readIMR();
 
  Serial.print("New IMR = ");
  Serial.println(newIMR,HEX);
for (int i = 0; i < 4; i++)
    {
      // read the socket and interrupt register
      byte newSnSR = W5100.readSnSR(i);
      Serial.print("socket and int register ");
      Serial.println(newSnSR, HEX);
      // reset the socket and interrupt register
      W5100.writeSnSR(i, 0xFF);
    }

  */
  attachInterrupt(INTERRUPT, handler, CHANGE);
}
void loop() {
  char Str[32];
  if (received == true) {
    int k = 0;
    char bitreceived;
    // disable interrupt to avoid new data corrupting the buffer
    detachInterrupt(INTERRUPT);
    // loop over buffer data
    for (unsigned int i = syncIndex1; i != syncIndex2; i = (i + 2) % RING_BUFFER_SIZE) {
      unsigned long t0 = timings[i], t1 = timings[(i + 1) % RING_BUFFER_SIZE];
      if (t0 > (SEP_LENGTH - 100) && t0 < (SEP_LENGTH + 100)) {
        if (t1 > (BIT1_LENGTH - 1000) && t1 < (BIT1_LENGTH + 1000)) {
          //  Serial.print("1");
          bitreceived = '1';
        } else if (t1 > (BIT0_LENGTH - 1000) && t1 < (BIT0_LENGTH + 1000)) {
          //   Serial.print("0");
          bitreceived = '0';
        } else {
          //   Serial.print("SYNC");  // sync signal
        }
      } else {
        // Serial.print("?");  // undefined timing
        bitreceived = '?';
      }
      Str[k] = bitreceived;
      k++;
    }
    String stringOne(Str);
    String temp_out = stringOne.substring(16, 24);
    Serial.print("Temp_out=");
    Serial.println(bintodec(temp_out));
    
    digitalWrite(10, HIGH);
    received = false;
    syncIndex1 = 0;
    syncIndex2 = 0;
    delay(500);
    // re-enable interrupt
    attachInterrupt(INTERRUPT, handler, CHANGE);
  }
}



