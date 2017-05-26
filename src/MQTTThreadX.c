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

#include "string.h"
#include "cfw.h"
#include "tcpip.h"
#include "sockets.h"
#include "tcpip_api_msg.h"

#include "MQTTThreadX.h"

#define NUM_SOCKETS MEMP_NUM_NETCONN

int threadx_asyncRead(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	int rc;

	rc = CFW_TcpipSocketRecv(n->socket, buffer, len, 0);
	if(rc > 0) {
		n->total_rxLen += rc;
	}

	return rc;
}

int threadx_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	fd_set fdset;
	struct timeval timeVal;
	int rc = 0;
	int readySock;

	FD_ZERO(&fdset);
	FD_SET(n->socket, &fdset);

	timeVal.tv_sec = timeout_ms / 1000;
	timeVal.tv_usec = (timeout_ms & 1000) * 1000;

	do {
		readySock = CFW_TcpipSocketSelect(NUM_SOCKETS, NULL, &fdset, NULL, &timeVal);
	} while(readySock != 1);

	rc = CFW_TcpipSocketSend(n->socket, buffer, len, 0);
	if (rc >= 0) {
		n->total_txLen += rc;
	}

	return rc;
}

void threadx_disconnect(Network* n)
{
	if (n->socket != 0) {
		CFW_TcpipSocketClose(n->socket);
	}
	n->socket = 0;
}

void NewNetwork(Network* n) {
	n->socket = 0;
	n->rx_left = 0;
	n->total_txLen = 0;
	n->total_rxLen = 0;
	n->mqttread = threadx_asyncRead;
	n->mqttwrite = threadx_write;
	n->disconnect = threadx_disconnect;
}

