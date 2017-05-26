/*******************************************************************************
 * Copyright (c) 2016 CMIOT Company.
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
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_THREADX_
#define __MQTT_THREADX_

#include "sockets.h"

typedef struct Network
{
	SOCKET socket;
#if 1
	uint16 rx_left;//异步接收模式缓冲区剩余长度
	uint16 packLen;//记录读取的包大小
#endif
	size_t total_txLen;
	size_t total_rxLen;
	int (*mqttread) (struct Network*, unsigned char*, int, int);
	int (*mqttwrite) (struct Network*, unsigned char*, int, int);
	void (*disconnect) (struct Network*);

}Network;

int threadx_asyncRead(Network*, unsigned char*, int, int);
int threadx_write(Network*, unsigned char*, int, int);
void threadx_disconnect(Network*);
void NewNetwork(Network*);

#endif

