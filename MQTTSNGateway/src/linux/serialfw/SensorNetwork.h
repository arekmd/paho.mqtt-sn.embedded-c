
/**************************************************************************************
 * Copyright (c) 2016, Tomoaki Yamaguchi
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Tomoaki Yamaguchi - initial API and implementation 
 **************************************************************************************/
#ifndef SENSORNETWORKX_H_
#define SENSORNETWORKX_H_

#include "MQTTSNGWDefines.h"
#include "MQTTSNGWProcess.h"
#include <string>
#include <vector>
#include <termios.h>

using namespace std;

namespace MQTTSNGW
{
//#define DEBUG_NWSTACK

#ifdef  DEBUG_NWSTACK
  #define D_NWSTACK(...) printf(__VA_ARGS__)
#else
  #define D_NWSTACK(...)
#endif

/*===========================================
  Class  SerialPort
 ============================================*/
class SerialPort{
public:
	SerialPort();
	~SerialPort();
	int open(char* devName, unsigned int baudrate,  bool parity, unsigned int stopbit, unsigned int flg);
	bool send(unsigned char b);
	bool recv(unsigned char* b);
	void flush();

private:
	int _fd;  // file descriptor
};

/*===========================================
 Class  SensorNetAddreess
 ============================================*/
class SensorNetAddress
{
	friend class Sfw;
public:
	SensorNetAddress();
	~SensorNetAddress();
	void setAddress(uint8_t* address, uint8_t size);
	void setAddress(const std::vector<uint8_t> &);
	int  setAddress(string* data);
	void setBroadcastAddress(void);
	bool isMatch(SensorNetAddress* addr);
	SensorNetAddress& operator =(SensorNetAddress& addr);
	char* sprint(char*);
    bool broadcast() const;
    uint8_t *ptr();
    uint32_t length() const;
private:

    bool _broadcast;
    std::vector<uint8_t> _address;
};

/*========================================
 Class Sfw
 =======================================*/
class Sfw
{
public:
	Sfw();
	~Sfw();

	int open(char* device, int boudrate);
	void close(void);
	int unicast(const uint8_t* buf, uint16_t length, SensorNetAddress* sendToAddr);
	int broadcast(const uint8_t* buf, uint16_t length);
	int recv(uint8_t* buf, uint16_t len, SensorNetAddress* addr);

private:
	int send(const uint8_t* payload, uint8_t pLen, SensorNetAddress* addr);

	Semaphore _sem;
	SerialPort* _serialPort;
};

/*===========================================
 Class  SensorNetwork
 ============================================*/
class SensorNetwork: public Sfw
{
public:
	SensorNetwork();
	~SensorNetwork();

	int unicast(const uint8_t* payload, uint16_t payloadLength, SensorNetAddress* sendto);
	int broadcast(const uint8_t* payload, uint16_t payloadLength);
	int read(uint8_t* buf, uint16_t bufLen);
	int initialize(void);
	const char* getDescription(void);
	SensorNetAddress* getSenderAddress(void);

private:
	SensorNetAddress _clientAddr;   // Sender's address. not gateway's one.
	string _description;
};

}

#endif /* SENSORNETWORKX_H_ */
