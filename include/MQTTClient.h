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

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "stdio.h"

#include "MQTTPacket.h"
#include "MQTTThreadX.h"
//#include "" //Platform specific implementation header file

#define MAX_PACKET_ID 65535
#define MAX_MESSAGE_HANDLERS 10//必须与NV中TopicsSubs个数相同，以免NV操作异常
#define MQTT_OMIT_TOPIC_MATCH (1)//忽略终端订阅信息，显示所有服务器下发PUBLISH消息


enum QoS { QOS0, QOS1, QOS2 };

// all failure return codes must be negative
enum returnCode { MQTT_BUFFER_OVERFLOW = -2, MQTT_FAILURE = -1, MQTT_SUCCESS = 0 };



#define MQTT_HOST_LEN (80)//目标地址最大长度
#define MQTT_CLTID_LEN (48)
#define MQTT_USR_LEN (48)
#define MQTT_PWD_LEN (48)

#define MQTT_HOST_NVID (1025)
#define MQTT_PORT_NVID (1026)
#define MQTT_CLTID_NVID (1027)
#define MQTT_KEEPALIVE_NVID (1028)
#define MQTT_USR_NVID (1029)
#define MQTT_PWD_NVID (1030)
#define MQTT_CLEAN_NVID (1031)
#define MQTT_TOPICSUBS_NVID (1032)

//0xD1~0xFF GROUP are reserved for USER's definition

#define MQTT_BUFSIZE	(1440)
#define MQTT_RDBUFSIZE	(1440)
#define MQTT_TOPIC_LEN (64)//必须与NV中TopicsSubs长度相同，以免NV操作异常
#define MQTT_MAXMSG_LEN (1440)
#define MQTT_REMAINING_LEN 4

#define MQTT_DEFAULT_SVR_TIMEOUT (20)//发送数据之后等待服务器响应的超时时间
#define MQTT_DEFAULT_RESEND_INTERVAL (20)//连接失败并且socket未断开时的重发间隔(s)

#define MQTT_MAX_SUB_TOPICS (MAX_MESSAGE_HANDLERS)

#define MQTT_MIN_KEEPALIVE (6)//MQTT最小ping间隔
#define MQTT_MIN_CONNECT_TIME (32)//mqtt连接失败重连基数
#define MQTT_MAX_CONNECT_TIME (2048)//mqtt连接失败重连最大间隔

enum{
	MQTT_MSG_CONNECT =	(0xE700),//59136
	MQTT_MSG_PINGREQ =	(0xE703),
	MQTT_MSG_CYCLE =	(0xE707),//
	MQTT_MSG_CMDTIMEOUT =	(0xE70A),//服务器反馈超时
	MQTT_MSG_RECNT =	(0xE70D),//socket重连次数
	//MQTT_MSG_RESEND,//mqtt socket连接之后的CONN包发送次数，直到从发次数用完或者socket断开
};

typedef struct MQTTMessage
{
	enum QoS qos;
	char retained;
	char dup;
	unsigned short id;
	void *payload;
	size_t payloadlen;
} MQTTMessage;
typedef struct MessageData
{
	MQTTMessage* message;
	MQTTString* topicName;
} MessageData;
typedef void (*messageHandler)(MessageData*);

typedef struct Client
{
	unsigned int command_timeout_ms;

	bool reconEn;//是否断开重连
	bool pingEchoEn;//是否允许显示PingRsp

	size_t buf_size, readbuf_size;//缓冲区大小
	size_t readbuf_len;//,buf_len;//缓冲区实际接收字符数
	unsigned char *buf;//发送缓冲区
	unsigned char *readbuf; //接收缓冲区

	size_t payLoadSize;

	uint32 ip;   ///Server IP
	uint16 port; ///Server port

	//接收大数据段时会收到分包数据,needByte表明当前还需要接收的字节数
	unsigned int needByte;
	unsigned int keepAliveInterval;

	char ping_outstanding;
	int isconnected;
	uint32 reconNum;//记录重连次数

	unsigned int next_packetid;

	struct MessageHandlers
	{
		//const char* topicFilter;
		char topicFilter[MQTT_TOPIC_LEN];
		void (*fp) (MessageData*);
		bool avail;
	} messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic

	char tmpTopics[MQTT_TOPIC_LEN];
	uint16 curTopic; 
	
	void (*defaultMessageHandler) (MessageData*);

	Network* ipstack;
} Client;

typedef struct
{
	uint8 usrFlag;
	uint8 pwdFlag;
	uint8 willFlag;
	uint8 willRetain;
	uint8 willQos;
	MQTTString willTopic;
	MQTTString willMsg;
} MQTT_CNMSG_T; //AT+MQTTOPEN时输入的连接信息

enum MQTT_CON_T
{
	MQTT_STAT_DISCONNECT = 0x00,	//未连接
	MQTT_STAT_CONNECTED,					//已连接空闲
	MQTT_STAT_CONNECTING,			//正在连接---socket正在连接
	MQTT_STAT_CONNSENDING,			//socket已连接，发送CONN包
	MQTT_STAT_WAITCONNACK,
	//MQTT_STAT_PUB,
	MQTT_STAT_TIMEOUT,				//ping ACK超时
};
//type define erea

typedef struct MQTT_CFG_T
{
	char host[MQTT_HOST_LEN];
	uint16 port;
	char clientId[MQTT_CLTID_LEN];
	uint16 keepAlive;
	char user[MQTT_USR_LEN];
	char passwd[MQTT_PWD_LEN];
	uint8 cleanSession;
} MQTT_CFG_T;
extern MQTT_CFG_T g_mqtt_cfg;

typedef  struct MQTT_PUB_INFO_T
{
	char topicName[MQTT_TOPIC_LEN];
	MQTTMessage msg;
}MQTT_PUB_INFO_T;

typedef struct MQTT_STAT_T
{
	struct ip_addr ip;
	bool isDnsParsed;
	MQTT_CNMSG_T conMsg;
	Client *mqttClient;
	bool isAtcDisc;
} MQTT_STAT_T;

extern MQTT_STAT_T g_mqtt_stat;

typedef struct {
	char topics[MQTT_MAX_SUB_TOPICS][MQTT_TOPIC_LEN];
	bool avail[MQTT_MAX_SUB_TOPICS];
	char tmp[MQTT_TOPIC_LEN];
	uint16 cur;
}MQTT_SUBARRAY_T;

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, const char*, MQTTMessage*);
int MQTTSubscribe (Client*, const char*, enum QoS, messageHandler);
int MQTTConnect_Async(Client* c, MQTTPacket_connectData* options);
int MQTTUnsubscribe (Client*, const char*);
int MQTTDisconnect (Client*);
int MQTTYield (Client*, int);
int deliverMessage(Client *, MQTTString *, MQTTMessage *);

extern void setDefaultMessageHandler(Client*, messageHandler);

extern void MQTTClient(Client*, Network*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

//int  mqtt_add_topicsArr(void);
extern int  mqtt_add_topicsArr(void (*fp) (MessageData*));

extern int mqtt_remove_topicsArr(void);

extern bool mqtt_add_topictArr(const char *topicName);

extern int mqtt_print_topicsArr(char *buf);

extern void mqtt_subMsgHandler(MessageData* md);

extern bool mqtt_is_topicExist(char *topicName);

extern int readPacket_Async(Client* c);
extern int readPacket_Async1(Client* c);

extern int mqtt_sendPacket(Client* c, int length);

#endif
