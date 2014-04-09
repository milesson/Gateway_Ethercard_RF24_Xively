// Simple demo for feeding some random data to Xively from a single remote sensor using RF24.

#include <EtherCard.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

// Dear compiler, please be quiet about those uninitalized progmem warnings.
#undef PSTR
#define PSTR(s) (__extension__({static const char __c[] __attribute__ (( section (".progmem") )) = (s); &__c[0];}))

// change these settings to match your own setup
#define FEED    "1167527071"
#define APIKEY  "foNYQ8Ne39ATsxeQNBlpbOWFmV0YZoNQkcISdMJaGXy11Xvc"

// ethernet interface mac address, must be unique on the LAN
byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

char website[] PROGMEM = "api.xively.com";

byte Ethernet::buffer[650];
bool has_data_to_send = false;
Stash stash;

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

void setup () {
  Serial.begin(57600);
  printf_begin();
  
  //
  // Setup and configure rf radio
  //
  radio.begin();
  radio.setRetries(15,15);
  radio.setPayloadSize(4);
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1,pipes[0]);
  radio.startListening();
  radio.printDetails();
  
  //Disable radio module
  switch_to_ethernet();
  
  //
  // Setup and configure Ethernet
  //
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");
    
  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
  if (!ether.dnsLookup(website))
    Serial.println("DNS failed");
    
  ether.printIp("SRV: ", ether.hisip);
}

void loop () {
  
  //Listen for radio data
  switch_to_radio();
  check_radio_messages();
  
  //Send values over Ethernet
  switch_to_ethernet();
  ether.packetLoop(ether.packetReceive());
  sendpackets();
}

void check_radio_messages() {
  
  if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      unsigned long got_time;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &got_time, sizeof(unsigned long) );

        // Spew it
        printf("Got payload %lu...",got_time);
        
        has_data_to_send = true;
        
	// Delay just a little bit to let the other unit
	// make the transition to receiver
	delay(20);
      }

      // First, stop listening so we can talk
      radio.stopListening();

      // Send the final one back.
      radio.write( &got_time, sizeof(unsigned long) );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
} 

void sendpackets() {
  
    if(!has_data_to_send)
      return;

    // generate two fake values as payload - by using a separate stash,
    // we can determine the size of the generated message ahead of time
    byte sd = stash.create();
    stash.print("moisture-terrace,");
    stash.println((word) 66);
    stash.print("temp-terrace,");
    stash.println((word) 11);
    stash.print("humidity-terrace,");
    stash.println((word) 77);
    stash.save();
    
    Serial.println("Sending values..");
    
    // generate the header with payload - note that the stash size is used,
    // and that a "stash descriptor" is passed in as argument using "$H"
    Stash::prepare(PSTR("PUT http://$F/v2/feeds/$F.csv HTTP/1.0" "\r\n"
                        "Host: $F" "\r\n"
                        "X-PachubeApiKey: $F" "\r\n"
                        "Content-Length: $D" "\r\n"
                        "\r\n"
                        "$H"),
            website, PSTR(FEED), website, PSTR(APIKEY), stash.size(), sd);

    // send the packet - this also releases all stash buffers once done
    ether.tcpSend();
    
    has_data_to_send = false;
}

void switch_to_radio() {
  digitalWrite(8, HIGH); //Network dactivated
  digitalWrite(10, LOW); //Radio active
}

void switch_to_ethernet() {
  digitalWrite(10, HIGH); //Radio deactivated
  digitalWrite(8, LOW); //Network active
}

