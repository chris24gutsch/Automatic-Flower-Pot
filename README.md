# Automatic-Flower-Pot [![Build Status](https://travis-ci.com/chris24gutsch/ESP8266-Auto-Flower-Pot.svg?token=s59VV8KQT6cPxW1cy3Vz&branch=master)](https://travis-ci.com/chris24gutsch/ESP8266-Auto-Flower-Pot)
This is a copy of the original private Automatic Flower Pot repository due to some sensitive information in previous commits. The Automatic Flower Pot is a way to set a schedule for watering your flowers via a web server.

## Parts
- ESP8266 NodeMCU
- Arduino IDE
- 3.5-9v Submersible Water Pump
- PC817 Optocoupler
- 1x330ohm
- 1xPN2222A

## Accurate Time Keeping
I currently use the Network Time Protocol (NTP) to keep time and track time intervals when they are set by the user. To send and recieve packets, User Data Protocol (UDP) is used. The `sendNTPpacket()` sends a 48 byte () packet to the NTP server's IP address (which was chosen in `setup()`) over port 123. The packet is then recieved and parsed by `getTime()`. If no packet is found, the program tries 30 more times for a packet. If no packet is found, then it waits another 10 seconds to check for a packet. If a packet is found, then bytes 40 through 43 are combined into `uint32_t NTPTime` which is the UTC. From there it is converted into UNIX time by subtracting 70 years of seconds from the returned UTC.
