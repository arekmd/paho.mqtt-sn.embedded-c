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

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include "SensorNetwork.h"
#include "MQTTSNGWProcess.h"

#include <algorithm>

using namespace std;
using namespace MQTTSNGW;

/*===========================================
 Class  SensorNetAddress
 ============================================*/
SensorNetAddress::SensorNetAddress()
{
    _broadcast = false;
    _address.push_back(0);
}

SensorNetAddress::~SensorNetAddress()
{

}

void SensorNetAddress::setAddress(const std::vector<uint8_t> &value) {
    _address = value;
}

bool SensorNetAddress::broadcast() const {
    return _broadcast;
}

void SensorNetAddress::setAddress(uint8_t* address, uint8_t size)
{
    _address.clear();
    std::copy_n(address, size, std::back_inserter(_address));
}

int SensorNetAddress::setAddress(string* address)
{
    _address.clear();
    std::copy(address->begin(), address->end(), std::back_inserter(_address));
	return 0;
}

uint32_t SensorNetAddress::length() const {
    return _address.size();
}

void SensorNetAddress::setBroadcastAddress(void)
{
    _broadcast = true;
}

bool SensorNetAddress::isMatch(SensorNetAddress* addr)
{
    return addr->_address == _address; 
}

SensorNetAddress& SensorNetAddress::operator =(SensorNetAddress& addr)
{
    _address = addr._address;
	return *this;
}

char* SensorNetAddress::sprint(char* buf)
{
	char* pbuf = buf;
	for ( int i = 0; i < 8; i++ )
	{
		sprintf(pbuf, "%02X", _address[i]);
		pbuf += 2;
	}
	return buf;
}

uint8_t *SensorNetAddress::ptr() {
    return &*_address.begin();
}

/*===========================================
 Class  SensorNetwork
 ============================================*/
SensorNetwork::SensorNetwork()
{

}

SensorNetwork::~SensorNetwork()
{

}

int SensorNetwork::unicast(const uint8_t* payload, uint16_t payloadLength, SensorNetAddress* sendToAddr)
{
	return Sfw::unicast(payload, payloadLength, sendToAddr);
}

int SensorNetwork::broadcast(const uint8_t* payload, uint16_t payloadLength)
{
	return Sfw::broadcast(payload, payloadLength);
}

int SensorNetwork::read(uint8_t* buf, uint16_t bufLen)
{
	return Sfw::recv(buf, bufLen, &_clientAddr);
}

int SensorNetwork::initialize(void)
{
	char param[MQTTSNGW_PARAM_MAX];
	uint32_t baudrate = 9600;

	_description += param;

	if (theProcess->getParam("Baudrate", param) == 0)
	{
		baudrate = (uint32_t)atoi(param);
	}
	_description += ", Baudrate ";
	sprintf(param ,"%d", baudrate);
	_description += param;

	theProcess->getParam("SerialDevice", param);
	_description += ", SerialDevice ";
	_description += param;

	return Sfw::open(param, baudrate);
}

const char* SensorNetwork::getDescription(void)
{
	return _description.c_str();
}

SensorNetAddress* SensorNetwork::getSenderAddress(void)
{
	return &_clientAddr;
}

/*===========================================
              Class  Sfw
 ============================================*/
Sfw::Sfw(){
    _serialPort = new SerialPort();
}

Sfw::~Sfw(){
	if ( _serialPort )
	{
		delete _serialPort;
	}
}

int Sfw::open(char* device, int baudrate)
{
	int rate = B9600;

	switch (baudrate)
	{
	case 9600:
		rate = B9600;
		break;
	case 19200:
		rate = B19200;
		break;
	case 38400:
		rate = B38400;
		break;
	case 57600:
		rate = B57600;
		break;
	case 115200:
		rate = B115200;
		break;
	default:
		return -1;
	}

	return _serialPort->open(device, rate, false, 1, O_RDWR | O_NOCTTY);
}

int Sfw::broadcast(const uint8_t* payload, uint16_t payloadLen){
	SensorNetAddress addr;
	addr.setBroadcastAddress();
	return send(payload, (uint8_t) payloadLen, &addr);
}

int Sfw:: unicast(const uint8_t* payload, uint16_t payloadLen, SensorNetAddress* addr){
	return send(payload, (uint8_t) payloadLen, addr);
}

int Sfw::recv(uint8_t* buf, uint16_t bufLen, SensorNetAddress* clientAddr)
{
    uint8_t value = 0;

    // Check length of the header. It should be more then 4
    if (_serialPort->recv(&value) != 1 || value < 4)
        return 0;

    // Node id length is equal to header length - 3 bytes
    int nodeIdLength = value - 3;

    // Check MsgType 
    if (_serialPort->recv(&value) != 1 || value != 0xFE)
        return 0;

    // Ctrl fiels
    if (_serialPort->recv(&value) != 1 )
        return 0;

    bool broadcast = (value && 1) != 0;

    std::vector<uint8_t> nodeId;

    for (uint i = 0; i < nodeIdLength; ++i) {
        
        if (_serialPort->recv(&value) != 1)
            return 0;

        nodeId.push_back(value);
    }

    clientAddr->setAddress(nodeId);
    uint8_t *iterator = buf;

    uint32_t mqtt_frame_len = 0;

    if (_serialPort->recv(iterator++) != 1)
        return 0;

    if (*buf == 1) {
        // Three octet length

        if (_serialPort->recv(iterator++) != 1)
            return 0;
        if (_serialPort->recv(iterator++) != 1)
            return 0;
            
        mqtt_frame_len = buf[2] << 8;
        mqtt_frame_len |= buf[3];

    } else {
        // Single octet length
        mqtt_frame_len = buf[0];
    }

    mqtt_frame_len -= (iterator - buf);

    for (uint32_t i = 0; i < mqtt_frame_len; ++i) {
        if (_serialPort->recv(iterator++) != 1)
            return 0;
    }

    int frame_size = iterator - buf;

    return frame_size;
}

int Sfw::send(const uint8_t* payload, uint8_t pLen, SensorNetAddress* addr){

	D_NWSTACK("\r\n===> Send:    ");

    _serialPort->send( addr->length() + 3);
    _serialPort->send(0xFE);

    if (addr->broadcast()) {
        _serialPort->send(1);
    } else {
        _serialPort->send(0);
    }

    uint8_t *ptr = addr->ptr();
    for (int i  = 0; i < addr->length(); ++i) {
        _serialPort->send(*ptr++);
    }

    for (int i = 0; i < pLen; ++i) {
       _serialPort->send(*payload++); 
    }

    return pLen;
}

/*=========================================
 Class SerialPort
 =========================================*/
SerialPort::SerialPort()
{
	//_tio.c_iflag = IGNBRK | IGNPAR;
	//_tio.c_cflag = CS8 | CLOCAL | CREAD;
	//_tio.c_cc[VINTR] = 0;
	//_tio.c_cc[VTIME] = 10;   // 1 sec.
	//_tio.c_cc[VMIN] = 1;
	_fd = 0;
}

SerialPort::~SerialPort()
{
	if (_fd)
	{
		::close(_fd);
	}
}

int SerialPort::open(char* devName, unsigned int baudrate, bool parity,
		unsigned int stopbit, unsigned int flg)
{
	_fd = ::open(devName, flg);
	if (_fd < 0)
	{
		return _fd;
	}


    struct termios t;

    bzero(&t, sizeof(t));

    if (tcgetattr(_fd, &t) < 0){
        return -1;
    }
    

    t.c_cflag = (t.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    t.c_iflag &= ~IGNBRK;         // disable break processing
    t.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
    t.c_oflag = 0;                // no remapping, no delays
    t.c_cc[VMIN]  = 0;            // read doesn't block
    t.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    t.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    t.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
    t.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    t.c_cflag |= parity;
    t.c_cflag &= ~CSTOPB;
    t.c_cflag &= ~CRTSCTS;

    cfsetispeed(&t, baudrate);
    cfsetospeed(&t, baudrate);

    cfmakeraw(&t);

	return tcsetattr(_fd, TCSANOW, &t);
}

bool SerialPort::send(unsigned char b)
{
	if (write(_fd, &b, 1) < 0)
	{
		return false;
	}
	else
	{
		D_NWSTACK( " %02x", b);
		return true;
	}
}

bool SerialPort::recv(unsigned char* buf)
{
    struct timeval timeout;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(_fd, &rfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;    // 500ms
    if ( select(_fd + 1, &rfds, 0, 0, &timeout) > 0 )
    {
        if (read(_fd, buf, 1) > 0)
        {
            D_NWSTACK( " %02x",buf[0] );
            return true;
        }
    }
    return false;
}

void SerialPort::flush(void)
{
    struct termios t;
    bzero(&t, sizeof(t));
    tcgetattr(_fd, &t); 

	tcsetattr(_fd, TCSAFLUSH, &t);
}

