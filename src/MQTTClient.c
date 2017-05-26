/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
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
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "string.h"
#include "at_trace.h"
#include "cfw.h"
#include "at_define.h"

#include "MQTTClient.h"
#include "sockets.h"


extern void mqtt_reset_pingTimer(UINT32 timeout);
extern void mqtt_stop_pingTimer(void);
extern void mqtt_reset_cmdTimer(int timeout);
extern void mqtt_stop_cmdTimer(void);

void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage)
{
	md->topicName = aTopicName;
	md->message = aMessgage;
}

int getNextPacketId(Client *c)
{
	return c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}

int mqtt_sendPacket(Client* c, int length)
{
	int rc = MQTT_FAILURE;
	int sent = 0;

	while (sent < length ) {
		rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, 0);
		if (rc < 0)
			break;
		sent += rc;
	}

	if (sent > 0) {
		mqtt_reset_pingTimer(c->keepAliveInterval * 16384 / 2);
	}

	if (sent == length) {
		rc = MQTT_SUCCESS;
	} else {
		rc = MQTT_FAILURE;
	}

	return rc;
}


void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
	int i;
	c->ipstack = network;

	for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
		c->messageHandlers[i].avail = FALSE;

	c->command_timeout_ms = command_timeout_ms;

	c->readbuf_len  = 0;
	c->buf = buf;
	c->buf_size = buf_size;
	c->readbuf = readbuf;
	c->readbuf_size = readbuf_size;

	c->isconnected = MQTT_STAT_CONNECTING;

	c->ping_outstanding = 0;
}

int decodePacket(Client* c, int* value, int timeout)
{
	unsigned char i;
	int multiplier = 1;
	int len = 0;

	*value = 0;
	do {
		int rc = MQTTPACKET_READ_ERROR;

		if (++len > MQTT_REMAINING_LEN) {
			rc = MQTTPACKET_READ_ERROR;
			goto exit;
		}

		rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
		if (rc != 1) {
			goto exit;
		}

		*value += (i & 127) * multiplier;

		multiplier *= 128;
	} while ((i & 128) != 0);

exit:
	return len;
}

int readPacket(Client* c)
{
	int rc = MQTT_FAILURE;
	MQTTHeader header = {0};
	int len = 0;
	int rem_len = 0;

	if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, 0) != 1)
		goto exit;

	len = 1;
	decodePacket(c, &rem_len, 0);
	len += MQTTPacket_encode(c->readbuf + 1, rem_len);

	if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, 0) != rem_len)) {
		goto exit;
	}

	header.byte = c->readbuf[0];
	rc = header.bits.type;

exit:
	return rc;
}

int readPacket_Async(Client* c)
{
	int rc = MQTT_FAILURE;
	MQTTHeader header = {0};
	int len = 0;
	int rem_len = 0;
	int rcvLen = 0;

	if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, 0) != 1)
		return MQTT_FAILURE;

	AT_TC(g_sw_TCPIP, "[MQTT]Header Byte 0x%x",c->readbuf[0]);

	len = 1;
	decodePacket(c, &rem_len, 0);
	len += MQTTPacket_encode(c->readbuf + 1, rem_len);

	AT_TC(g_sw_TCPIP, "[MQTT]Need %d Bytes,  %d Bytes remain",rem_len,c->ipstack->rx_left);

	c->ipstack->packLen = len;
	c->payLoadSize = rem_len;
	if (rem_len > 0 ) {
		rcvLen = c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, 0) ;
		if (rcvLen <= 0) {
			return MQTT_FAILURE;
		}
		if (rem_len == rcvLen) {
			c->needByte = 0;
			c->ipstack->packLen = rcvLen;
		} else {
			c->needByte  = rem_len -  rcvLen;
			c->ipstack->packLen = rcvLen;
		}
			
	}

	while (c->ipstack->rx_left > 0) {
		rcvLen = c->ipstack->mqttread(c->ipstack,  c->readbuf + len + rcvLen, c->ipstack->rx_left, 0) ;
		AT_TC(g_sw_TCPIP, "[MQTT]readPacket: receive data exceed!!!");	
	}
	
	header.byte = c->readbuf[0];
	rc = header.bits.type;

	return rc;
}

int readPacket_Async1(Client* c)
{
	int rc = MQTT_FAILURE;
	MQTTHeader header = {0};
	int len = 0;
	int rem_len = 0;
	int rcvLen = 0;

	unsigned char i;
	int multiplier = 1;

	c->readbuf_len = c->ipstack->mqttread(c->ipstack, c->readbuf, c->readbuf_size, 0);
	if (c->readbuf_len <= 0)
		return MQTT_FAILURE;

	AT_TC(g_sw_TCPIP, "[MQTT]Header Byte 0x%x, total %d Bytes", c->readbuf[0], c->readbuf_len);
	len = 1;
	do {
		if (len > MQTT_REMAINING_LEN || len > c->readbuf_len - 1) {
			rc = MQTTPACKET_READ_ERROR;
			return rc;
		}

		i = c->readbuf[len];
		rem_len += (i & 127) * multiplier;
		multiplier *= 128;

		len++;
	} while ((i & 128) != 0);

	AT_TC(g_sw_TCPIP, "[MQTT]Need %d Bytes,  %d Bytes remain", rem_len, c->ipstack->rx_left);

	c->payLoadSize = rem_len;
	if (rem_len > 0) {
		if(rem_len <= c->readbuf_len - len) {
			c->needByte = 0;
			c->ipstack->packLen = rem_len;
		} else {
			c->needByte  = rem_len -  (c->readbuf_len - len);
			c->ipstack->packLen = c->readbuf_len - len;
		}
			
	} else {
		c->needByte = 0;
	}

	rc = MQTT_SUCCESS;
	while (c->ipstack->rx_left > 0) {
		rcvLen = c->ipstack->mqttread(c->ipstack,  c->readbuf, c->ipstack->rx_left, 0) ;
		AT_TC(g_sw_TCPIP, "[MQTT]readPacket: receive data exceed!!!");
		rc = MQTT_BUFFER_OVERFLOW;
	}
	if(rc < 0)
		return rc;

	header.byte = c->readbuf[0];
	rc = header.bits.type;
	return rc;
}

// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
	char* curf = topicFilter;
	char* curn = topicName->lenstring.data;
	char* curn_end = curn + topicName->lenstring.len;

	while (*curf && curn < curn_end) {
		if (*curn == '/' && *curf != '/') {
			break;
		}

		if (*curf != '+' && *curf != '#' && *curf != *curn) {
			break;
		}

		if (*curf == '+') {
			// skip until we meet the next separator, or end of string
			char* nextpos = curn + 1;
			while (nextpos < curn_end && *nextpos != '/') {
				nextpos = ++curn + 1;
			}
		} else if (*curf == '#') {
			// skip until end of string
			curn = curn_end - 1;
		}

		curf++;
		curn++;
	}

	return (curn == curn_end) && (*curf == '\0');
}


int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
	int i;
	int rc = MQTT_FAILURE;

	for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i) {
		if (c->messageHandlers[i].topicFilter != 0
				&& (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter)
					|| isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName))) {
			if (c->messageHandlers[i].fp != NULL) {
				MessageData md;
				NewMessageData(&md, topicName, message);
				c->messageHandlers[i].fp(&md);
				rc = MQTT_SUCCESS;
			}
		}
	}

	if (rc == MQTT_FAILURE && c->defaultMessageHandler != NULL) {
		MessageData md;
		NewMessageData(&md, topicName, message);
		c->defaultMessageHandler(&md);
		rc = MQTT_SUCCESS;
	}

	return rc;
}

/*****************************************************************************
*  \brief 异步发送Mqtt连接请求
*
*  \param [in] c       客户端handle
*  \param [in] options 连接参数
*  \return 返回FAILURE代表异常
*  		返回SUCCESS代表正常发送connect包
*
*  \details Details
******************************************************************************/
int MQTTConnect_Async(Client* c, MQTTPacket_connectData* options)
{
	int rc = MQTT_FAILURE;
	MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
	int len = 0;

	if (options == 0) {
		options = &default_options;
	}

	c->keepAliveInterval = options->keepAliveInterval;
	if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0) {
		return MQTT_FAILURE;
	}

	mqtt_reset_cmdTimer(c->command_timeout_ms);
	if ((rc = mqtt_sendPacket(c, len)) != MQTT_SUCCESS) {
		return MQTT_FAILURE;
	}

	return MQTT_SUCCESS;
}

int MQTTSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandlers)
{
	int rc = MQTT_FAILURE;
	int len = 0;
	MQTTString topic = MQTTString_initializer;
	topic.cstring = (char *)topicFilter;

	if (c->isconnected != MQTT_STAT_CONNECTED) {
		goto exit;
	}

	len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
	if (len <= 0) {
		goto exit;
	}

	mqtt_reset_cmdTimer(c->command_timeout_ms);
	if ((rc = mqtt_sendPacket(c, len)) != MQTT_SUCCESS) {
		goto exit;
	}

	rc = MQTT_SUCCESS;

exit:
	return rc;
}


int MQTTUnsubscribe(Client* c, const char* topicFilter)
{
	int rc = MQTT_FAILURE;
	MQTTString topic = MQTTString_initializer;
	int len = 0;
	topic.cstring = (char *)topicFilter;

	if (c->isconnected != MQTT_STAT_CONNECTED)
		goto exit;

	if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
		goto exit;

	mqtt_reset_cmdTimer(c->command_timeout_ms);
	if ((rc = mqtt_sendPacket(c, len)) != MQTT_SUCCESS) {
		goto exit;
	}

	rc = MQTT_SUCCESS;

exit:
	return rc;
}

/*****************************************************************************
*  \brief 向服务器发送MQTT PUB包
*
*  \param [in] c MQTT对象结构体
*  \param [in] topicName PUB包的topic字段,函数返回后可释放此缓冲区
*  \param [in] PUB包payload字段数据封装,函数返回后可释放此指缓冲区
*  \return Return_Description
*
*  \details Details
******************************************************************************/
int MQTTPublish(Client* c, const char* topicName, MQTTMessage* message)
{
	int rc = MQTT_FAILURE;
	MQTTString topic = MQTTString_initializer;
	int len = 0;
	topic.cstring = (char *)topicName;
	
	if (c->isconnected != MQTT_STAT_CONNECTED)
		goto exit;

	if (message->qos == QOS1 || message->qos == QOS2)
		message->id = getNextPacketId(c);

	len = MQTTSerialize_publish(c->buf, c->buf_size, message->dup, message->qos, message->retained, message->id,
	                            topic, (unsigned char*)message->payload, message->payloadlen);
	if (len <= 0)
		goto exit;

	mqtt_reset_cmdTimer(c->command_timeout_ms);
	if ((rc = mqtt_sendPacket(c, len)) != MQTT_SUCCESS) {
		goto exit;
	}
	
	rc = MQTT_SUCCESS;

exit:
	return rc;
}


int MQTTDisconnect(Client* c)
{
	int rc = MQTT_FAILURE;
	int len = MQTTSerialize_disconnect(c->buf, c->buf_size);

	if (len > 0) {
		rc = mqtt_sendPacket(c, len);
	}

	c->isconnected = 0;
	return rc;
}

