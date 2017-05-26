/*****************************************************************************
*  \brief mqtt进程，用于处理mqtt相关信息
*  Copyright (c) 2016 China Mobile IOT Company.
*
*  **************************************************************
*
*  \file mqtt_tsk.c
*  \author LinZW
*  \create Time: 2016-5-24
*  \modified Time:
******************************************************************************/

#include "stdio.h"
#include "string.h"
#include "sockets.h"
#include "tcpip.h"
#include "at_trace.h"
#include "at_common.h"
#include "at_cmd_tcpip.h"
#include "at_errcode.h"
#include "cs_types.h"
#include "cos.h"
#include "at_utility.h"
#include "at_define.h"
#include "cfw.h"

#include "mqtt_tsk.h"
#include "MQTTClient.h"

MQTTPacket_connectData mqtt_con_config = MQTTPacket_connectData_initializer;

static Client s_mqttClient;
static Network s_mqttNet;
static char s_mqtt_buf[MQTT_BUFSIZE];
static char s_mqtt_readBuf[MQTT_RDBUFSIZE];
static int s_mqtt_con_timeout = 32 * 16348 / 2;

MQTT_CFG_T g_mqtt_cfg;
MQTT_STAT_T g_mqtt_stat;

#define MQTT_SERVER_CONNECT_TIMER (1)
#define MQTT_MSG_PINGREQ_TIMER    (2)
#define MQTT_MSG_CMD_TIMER        (3)
#define BAL_MQTT_STACK_SIZE (4 * 1024)
#define BAL_MQTT_TASK_PRIORITY 230

HANDLE g_mqtt_task;

#undef AT_TC
#include<stdio.h>
static void AT_TC(
		int buffer, 		// Storage location for output
		CONST TCHAR *format, // Format-control string
		... // Optional arguments
		)
{
	va_list args;
	char tmp[1024] = {0}; \
	INT32 Tmp;

	if (buffer != 1)
		return;
	va_start(args, format);
	AT_Sprintf(tmp, format, args); \
	Tmp = SUL_StrVPrint(tmp, format, args);
	va_end(args);
	AT_WriteUart(tmp, sizeof(tmp));
}

static void mqtt_init_conData(MQTTPacket_connectData *data)
{
	data->MQTTVersion = 4;

	data->clientID.cstring = g_mqtt_cfg.clientId;
	
	if(g_mqtt_stat.conMsg.usrFlag == TRUE)
		data->username.cstring = g_mqtt_cfg.user;
	if(g_mqtt_stat.conMsg.pwdFlag == TRUE)
		data->password.cstring = g_mqtt_cfg.passwd;

	data->cleansession = g_mqtt_cfg.cleanSession;
	data->willFlag = g_mqtt_stat.conMsg.willFlag;
	data->will.qos = g_mqtt_stat.conMsg.willQos;
	data->will.retained = g_mqtt_stat.conMsg.willRetain;

	data->will.message.cstring =  g_mqtt_stat.conMsg.willMsg.cstring;
	data->will.topicName.cstring =  g_mqtt_stat.conMsg.willTopic.cstring;

	data->keepAliveInterval = g_mqtt_cfg.keepAlive;
}

/*****************************************************************************
*  \brief 将缓存topic从列表中删除
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
int mqtt_remove_topicsArr(void)
{
	int loop = sizeof(s_mqttClient.messageHandlers) / sizeof(s_mqttClient.messageHandlers[0]);

	while(--loop >= 0)
	{
		//找到对应队列的topic
		if(strcmp(s_mqttClient.tmpTopics,s_mqttClient.messageHandlers[loop].topicFilter) ==0)
		{
			s_mqttClient.messageHandlers[loop].avail = FALSE;
			s_mqttClient.tmpTopics[0] = '\0';//清空临时缓冲区
			return MQTT_SUCCESS;
		}
	}

	return MQTT_FAILURE;
}

/*****************************************************************************
*  \brief 清空订阅的topics列表
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
static void mqtt_clear_topicsArr(void)
{
	int loop = sizeof(s_mqttClient.messageHandlers) / sizeof(s_mqttClient.messageHandlers[0]);

	memset(s_mqttClient.tmpTopics,0,sizeof(s_mqttClient.tmpTopics)) ;
	while(--loop >= 0)
	{
		memset(s_mqttClient.messageHandlers[loop].topicFilter,0,sizeof(s_mqttClient.messageHandlers[loop].topicFilter));
		s_mqttClient.messageHandlers[loop].avail = FALSE;
	}
}

/*****************************************************************************
*  \brief 打印订阅列表
*
*  \param [in] buf 存放订阅列表的缓冲区
*  \return Return_Description
*
*  \details Details
******************************************************************************/
int mqtt_print_topicsArr(char *buf)
{
	int ret = 0;
	int loop = sizeof(s_mqttClient.messageHandlers) / sizeof(s_mqttClient.messageHandlers[0]);

	buf[0] = '\0';
	while(--loop >= 0)
	{
		if(TRUE == s_mqttClient.messageHandlers[loop].avail )
		{
			strcat(buf,s_mqttClient.messageHandlers[loop].topicFilter);
			strcat(buf,"\r\n");
			ret++;
		}
	}

	return ret;
}

/*****************************************************************************
*  \brief at+mqttsub/unsub添加临时topics到缓冲区，如果添加成功，则移至队列
*
*  \param [in] topicName 将处理的topic
*  \return Return_Description
*
*  \details Details
******************************************************************************/
bool mqtt_add_topictArr(const char *topicName)
{
	int len = strlen(topicName);

	if (len  > sizeof(s_mqttClient.tmpTopics) - 1) {
		strncpy(s_mqttClient.tmpTopics, topicName, sizeof(s_mqttClient.tmpTopics) - 1);
		return FALSE;
	}

	strcpy(s_mqttClient.tmpTopics, topicName);

	return TRUE;
}

/*****************************************************************************
*  \brief 将缓存topic加入列表
*
*  \param [in] fp topic有public时的处理回调函数
*  \return Return_Description
*
*  \details Details
******************************************************************************/
int  mqtt_add_topicsArr(void (*fp)(MessageData *))
{
	int loop = sizeof(s_mqttClient.messageHandlers) / sizeof(s_mqttClient.messageHandlers[0]);

	while(--loop >= 0)
	{
		//找到对应队列的topic
		if(s_mqttClient.messageHandlers[loop].avail == FALSE)
		{
			strcpy(s_mqttClient.messageHandlers[loop].topicFilter,s_mqttClient.tmpTopics) ;
			s_mqttClient.messageHandlers[loop].avail = TRUE;
			s_mqttClient.messageHandlers[loop].fp = fp;
			s_mqttClient.curTopic= loop;
			s_mqttClient.tmpTopics[0] = '\0';//清空临时缓冲区
			return MQTT_SUCCESS;
		}
	}

	return MQTT_FAILURE;
}

/*****************************************************************************
*  \brief 判断topic是否已经订阅过
*
*  \param [in] topicName
*  \return Return_Description
*
*  \details Details
******************************************************************************/
bool mqtt_is_topicExist(char *topicName)
{
	int loop = sizeof(s_mqttClient.messageHandlers) / sizeof(s_mqttClient.messageHandlers[0]);

	while(--loop >= 0)
	{
		if(TRUE == s_mqttClient.messageHandlers[loop].avail )
		{
			if(strcmp(topicName, s_mqttClient.messageHandlers[loop].topicFilter) == 0)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*****************************************************************************
*  \brief 初始化客户端参数
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void init_mqtt_client(void)
{
	memset(&g_mqtt_stat,0,sizeof(g_mqtt_stat));
	g_mqtt_stat.isAtcDisc = FALSE;
	memset(&s_mqttClient,0,sizeof(s_mqttClient));
	memset(&s_mqttNet,0,sizeof(s_mqttNet));
	memset(&s_mqtt_buf,0,sizeof(s_mqtt_buf));
	memset(&s_mqtt_readBuf,0,sizeof(s_mqtt_readBuf));
	g_mqtt_stat.mqttClient = &s_mqttClient;

	s_mqttClient.defaultMessageHandler = mqtt_subMsgHandler;
	MQTTClient(&s_mqttClient, &s_mqttNet,
			MQTT_DEFAULT_SVR_TIMEOUT * 100000,
			s_mqtt_buf, sizeof(s_mqtt_buf),
			s_mqtt_readBuf, sizeof(s_mqtt_readBuf));

	s_mqttClient.isconnected = MQTT_STAT_DISCONNECT;
	s_mqttClient.needByte = 0;
	s_mqttClient.reconNum = 0;
	s_mqttClient.pingEchoEn = TRUE;
	s_mqttClient.reconEn = TRUE;
	s_mqttClient.keepAliveInterval = g_mqtt_cfg.keepAlive;

	extern UINT32 CFW_TcpipInetAddr(const INT8 *cp);
	s_mqttClient.ip = CFW_TcpipInetAddr(g_mqtt_cfg.host);
	s_mqttClient.port = g_mqtt_cfg.port;

	AT_TC(g_sw_TCPIP, "[MQTT] mqtt_initTask done");
}

/*****************************************************************************
*  \brief 重置MQTTping定时器
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_reset_pingTimer(UINT32 timeout)
{
	COS_KillTimer(g_mqtt_task, MQTT_MSG_PINGREQ_TIMER);
	COS_SetTimer(g_mqtt_task,
			MQTT_MSG_PINGREQ_TIMER,
			COS_TIMER_MODE_SINGLE,
			timeout);
}

/*****************************************************************************
*  \brief 停止MQTT ping定时器，当断开连接时，需要停止定时
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_stop_pingTimer(void)
{
	COS_KillTimer(g_mqtt_task, MQTT_MSG_PINGREQ_TIMER);
}

/*****************************************************************************
*  \brief 重启ACK超时定时器，当需要多步ACK时需要重置定时器，以对下一步ACK重新计时
*
*  \param [in] timeOut 超时时间设置，单位秒
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_reset_cmdTimer(int timeout)//milli-second
{
	COS_KillTimer(g_mqtt_task, MQTT_MSG_CMD_TIMER);
	COS_SetTimer(g_mqtt_task,
			MQTT_MSG_CMD_TIMER,
			COS_TIMER_MODE_SINGLE,
			timeout);
}

/*****************************************************************************
*  \brief 停止MQTT ACK定时器，当断开连接或者收到ACK时，需要停止定时
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_stop_cmdTimer(void)
{
	COS_KillTimer(g_mqtt_task, MQTT_MSG_CMD_TIMER);
}

/*****************************************************************************
*  \brief 停止MQTT 连接超时定时器，当socket连接成功建立时需要停止定时
*
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_stop_conTimer(void)
{
	COS_KillTimer(g_mqtt_task, MQTT_SERVER_CONNECT_TIMER);
}

/*****************************************************************************
*  \brief 重启MQTT连接定时器，当需要多步ACK时需要重置定时器，以对下一步ACK重新计时
*
*  \param [in] timeOut 超时时间设置，单位秒
*  \return Return_Description
*
*  \details Details
******************************************************************************/
void mqtt_reset_contimer(uint32 timeOut)//second
{
	COS_KillTimer(g_mqtt_task, MQTT_SERVER_CONNECT_TIMER);
	COS_SetTimer(g_mqtt_task,
			MQTT_SERVER_CONNECT_TIMER,
			COS_TIMER_MODE_SINGLE,
			timeOut);
}

extern UINT8 CFW_TcpipSocketGetStatus(SOCKET nSocket);
static int mqtt_socket()
{
	UINT8 acIPAddr[18] = {0}, uilen;
	UINT32 dat1;
	INT32 iResult;
	SOCKET mqtt_fd;
	CFW_TCPIP_SOCKET_ADDR stLocalAddr = {0};
	CFW_TCPIP_SOCKET_ADDR nDestAddr = {0};

	mqtt_fd = CFW_TcpipSocket(CFW_TCPIP_AF_INET, CFW_TCPIP_SOCK_STREAM, CFW_TCPIP_IPPROTO_TCP);
	AT_TC(g_sw_TCPIP, "MQTT_SocketOpen ok ,fd is id= %d", mqtt_fd);

	extern UINT32 CFW_GprsGetPdpAddr(UINT8 nCid, UINT8 *nLength, UINT8 *pPdpAdd, CFW_SIM_ID nSimID);
	CFW_GprsGetPdpAddr(g_uATTcpipCid[0], &uilen, acIPAddr, 0);
	stLocalAddr.sin_len 		= 0;
	stLocalAddr.sin_family		= CFW_TCPIP_AF_INET;
	stLocalAddr.sin_port		= 0;
	stLocalAddr.sin_addr.s_addr = htonl(acIPAddr[0] << 24 | acIPAddr[1] << 16 | acIPAddr[2] << 8 | acIPAddr[3]);

	AT_TC(g_sw_TCPIP, "\r%d.%d.%d.%d:%x\n", (stLocalAddr.sin_addr.s_addr >> 0) & 0xff
			, (stLocalAddr.sin_addr.s_addr >> 8) & 0xff
			, (stLocalAddr.sin_addr.s_addr >> 16) & 0xff
			, (stLocalAddr.sin_addr.s_addr >> 24) & 0xff, stLocalAddr.sin_port);
	CFW_TcpipSocketBind(mqtt_fd, &stLocalAddr, sizeof(CFW_TCPIP_SOCKET_ADDR));

	dat1  = O_NONBLOCK | TCPIP_IO_ASYNC;
	iResult = CFW_TcpipSocketIoctl(mqtt_fd, FIONBIO, &dat1);
	if (AT_TCPIP_ERR == iResult) {
		AT_TC(g_sw_TCPIP, "MQTT_SocketOpen () :CFW_TcpipSocketIoctl() failed");
		return AT_TCPIP_ERR;
	}

	nDestAddr.sin_family = CFW_TCPIP_AF_INET;
	nDestAddr.sin_port	 = htons(s_mqttClient.port);
	nDestAddr.sin_addr.s_addr = s_mqttClient.ip;
	AT_TC(g_sw_TCPIP, "\r\n%d.%d.%d.%d:%x\r\n", (s_mqttClient.ip >> 0) & 0xff
			, (s_mqttClient.ip >> 8) & 0xff
			, (s_mqttClient.ip >> 16) & 0xff
			, (s_mqttClient.ip >> 24) & 0xff, nDestAddr.sin_port);
	iResult = CFW_TcpipSocketConnect(mqtt_fd, &nDestAddr, SIZEOF(CFW_TCPIP_SOCKET_ADDR));
	if (SOCKET_ERROR == iResult) {
		AT_TC(g_sw_TCPIP, "MQTT_SocketOpen() :CFW_TcpipSocketConnect()failed");
		return AT_TCPIP_ERR;
	}

	return mqtt_fd;
}

/*****************************************************************************
*  \brief MQTT任务处理函数
*
*  \param [in] argc 暂时不用
*  \param [in] argv 暂时不用
*  \return Return_Description
*
*  \details Details
******************************************************************************/
static void bal_mqtt_tsk_func(void *p_data)
{
	Client *c = &s_mqttClient;

    COS_EVENT ev;

	while (1)
	{
		COS_WaitEvent(g_mqtt_task, &ev, COS_WAIT_FOREVER);
		switch (ev.nEventId)
		{

			case EV_CFW_MQTT_CONNECT:
				{
					mqtt_reset_contimer(s_mqtt_con_timeout);

					NewNetwork(&s_mqttNet);
					s_mqttNet.socket = mqtt_socket();
					if (s_mqttNet.socket < 0) {
						AT_TC(g_sw_TCPIP, "TCP SOCKET ERROR\r\n");
						AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
					}
				}
				break;

			case EV_CFW_TCPIP_SOCKET_CONNECT_RSP:
				{
					mqtt_stop_conTimer();

					mqtt_init_conData(&mqtt_con_config);
					mqtt_reset_cmdTimer(c->command_timeout_ms);
					if(MQTTConnect_Async(&s_mqttClient, &mqtt_con_config) != MQTT_SUCCESS)
					{
						AT_TC(g_sw_TCPIP, "MQTT CONNECTED PACK SEND ERROR!\r\n");
						(s_mqttNet.disconnect)(&s_mqttNet);
						c->isconnected = MQTT_STAT_DISCONNECT;
						AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
					}

					c->isconnected = MQTT_STAT_WAITCONNACK;
				}
				break;
			case EV_CFW_TCPIP_SOCKET_SEND_RSP:
				{

				}
				break;

			case EV_CFW_MQTT_PINGREQ:
				{
					int rc = MQTT_FAILURE;
					int len = MQTTSerialize_pingreq(c->buf, c->buf_size);
					if (len > 0 && (rc = mqtt_sendPacket(c, len)) == MQTT_SUCCESS) {
						c->ping_outstanding = 1;
					}
				}
				break;

			case EV_CFW_TCPIP_REV_DATA_IND:
				{
					int len = 0, rc = MQTT_SUCCESS;
					unsigned short packet_type;
					AT_TC(g_sw_TCPIP, "MQTT PACKET RECEIVED\r\n");
					if (c->needByte > 0) {
						int rcvLen = c->ipstack->mqttread(c->ipstack, c->readbuf , c->needByte, 0) ;
						if (rcvLen <= 0) {
							AT_TC(g_sw_TCPIP, "MQTT READ ERROR\r\n");
							break;
						}

						c->needByte -= rcvLen;
						if (c->needByte <= 0) {
							c->readbuf[rcvLen] = '\r';
							c->readbuf[rcvLen + 1] = '\n';
							AT_WriteUart(c->readbuf, rcvLen + 2);
						} else {
							AT_WriteUart(c->readbuf, rcvLen);
						}

						while (c->ipstack->rx_left > 0) {
							rcvLen = c->ipstack->mqttread(c->ipstack,  c->readbuf , c->ipstack->rx_left, 0) ;
							AT_TC(g_sw_TCPIP, "[MQTT]mqtt-task: receive data exceed!!!");
						}
						break;
					}

					packet_type = readPacket_Async1(c);
					switch (packet_type)
					{
						case CONNACK:
							{
								unsigned char connack_rc = 255;
								char sessionPresent = 0;

								mqtt_stop_cmdTimer();
								rc = MQTT_FAILURE;
								if (MQTTDeserialize_connack((unsigned char*)&sessionPresent,
											&connack_rc, c->readbuf,c->readbuf_size) == 1) {
									rc = connack_rc;
								}

								if (rc == MQTT_SUCCESS) {
									c->isconnected = MQTT_STAT_CONNECTED;
									mqtt_reset_pingTimer(c->keepAliveInterval * 16348 / 2);
									AT_TC(g_sw_TCPIP, "MQTT CONNECTED OK\r\n");
									AT_TC(g_sw_TCPIP, "[MQTT]CONACK-server session-present is %d", sessionPresent);
									if (c->reconNum == 0 || c->reconEn == FALSE) {
										AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
									}
									c->reconNum = 0;
								} else {
									AT_TC(g_sw_TCPIP, "MQTT CONNECTED FAILED\r\n");
									if (c->reconNum == 0 || c->reconEn == FALSE) {
										AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
									}
								}
							}
							break;
						case PUBLISH:
							{
								MQTTString topicName;
								MQTTMessage msg;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_publish((unsigned char*)&msg.dup,
											(int*)&msg.qos, (unsigned char*)&msg.retained,
											(unsigned short*)&msg.id, &topicName,
											(unsigned char**)&msg.payload, (int*)&msg.payloadlen,
											c->readbuf, c->readbuf_len) != 1) {
									AT_TC(g_sw_TCPIP, "MQTT PUBLISH ERROR\r\n");
									break;
								}

								deliverMessage(c, &topicName, &msg);
								if (msg.qos != QOS0)
								{
									if (msg.qos == QOS1) {
										len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
									} else if (msg.qos == QOS2) {
										len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
									}

									if (len <= 0) {
										rc = MQTT_FAILURE;
									} else {
										mqtt_reset_cmdTimer(c->command_timeout_ms);
										rc = mqtt_sendPacket(c, len);
									}

									if (rc == MQTT_FAILURE) {
										AT_TC(g_sw_TCPIP, "MQTT PUBLISH ERROR\r\n");
									}
								}
								break;
							}
						case PUBACK:
							{
								unsigned short mypacketid;
								unsigned char dup, type;

								mqtt_stop_cmdTimer();

								if (MQTTDeserialize_ack(&type, &dup, &mypacketid,
											c->readbuf, c->readbuf_size) != 1) {
									rc = MQTT_FAILURE;
									AT_TC(g_sw_TCPIP, "MQTT PUBACK ERROR\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								} else {
									AT_TC(g_sw_TCPIP, "MQTT PUBACK: %d,%d\r\n", dup, mypacketid);
									AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
								}
							}
							break;
						case PUBREC:
							{
								unsigned short mypacketid;
								unsigned char dup, type;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_ack(&type, &dup, &mypacketid,
											c->readbuf, c->readbuf_size) != 1) {
									rc = MQTT_FAILURE;
								} else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0) {
									rc = MQTT_FAILURE;
								} else {
									mqtt_reset_cmdTimer(c->command_timeout_ms);
									rc = mqtt_sendPacket(c, len);
									if ( rc != MQTT_SUCCESS) {
										rc = MQTT_FAILURE;
									}
								}

								if (rc == MQTT_FAILURE) {
									AT_TC(g_sw_TCPIP, "MQTT PUBREC ERROR\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								} else {
									AT_TC(g_sw_TCPIP, "MQTT PUBREC: %d,%d\r\n", dup, mypacketid);
								}
								break;
							}
						case PUBREL:
							{
								unsigned short mypacketid;
								unsigned char dup = 0, type = 0;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1) {
									rc = MQTT_FAILURE;
									AT_TC(g_sw_TCPIP, "MQTT PUBREL ERROR\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								} else {
									len = MQTTSerialize_ack(c->buf, c->buf_size, PUBCOMP, 0, mypacketid);
									rc = mqtt_sendPacket(c, len);
									AT_TC(g_sw_TCPIP, "[MQTT]PUBREL-type=%d,dup=%d", type, dup);
									AT_TC(g_sw_TCPIP, "MQTT PUBREL: %d,%d\r\n", dup, mypacketid);
									AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
								}
							}
							break;
						case PUBCOMP:
							{
								unsigned short mypacketid;
								unsigned char dup, type;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1) {
									rc = MQTT_FAILURE;
									AT_TC(g_sw_TCPIP, "MQTT PUBCOMP ERROR\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								} else {
									AT_TC(g_sw_TCPIP, "MQTT PUBCOMP: %d,%d\r\n", dup, mypacketid);
									AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
								}
							}
							break;
						case SUBACK:
							{
								int count = 0, grantedQoS = -1;
								unsigned short mypacketid;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_suback(&mypacketid, 1, &count,
											&grantedQoS, c->readbuf, c->readbuf_size) == 1) {
									rc = grantedQoS; // 0, 1, 2 or 0x80
								} else {
									AT_TC(g_sw_TCPIP, "MQTT SUBACK ERROR\r\n");
									break;
								}

								if (rc != 0x80) {
									mqtt_add_topicsArr(mqtt_subMsgHandler);
									AT_TC(g_sw_TCPIP, "MQTT SUBACK: %d,%d\r\n", mypacketid, grantedQoS);
									AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
								} else {
									AT_TC(g_sw_TCPIP, "MQTT SUBACK FAILED\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								}
							}
							break;
						case UNSUBACK:
							{
								unsigned short mypacketid;
								unsigned char dup, type;

								mqtt_stop_cmdTimer();
								if (MQTTDeserialize_ack(&type, &dup, &mypacketid,
											c->readbuf, c->readbuf_size) != 1) {
									rc = MQTT_FAILURE;
									AT_TC(g_sw_TCPIP,"MQTT UNSUBACK ERROR\r\n");
									AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, ev.nParam1);
								} else {
									mqtt_remove_topicsArr();
									AT_TC(g_sw_TCPIP,"MQTT UNSUBACK: %d\r\n", mypacketid);
									AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, NULL, 0, ev.nParam1);
								}
							}
							break;
						case MQTT_PINGRESP:
							{
								c->ping_outstanding = 0;
								if (c->pingEchoEn) {
									char tmp[] = "MQTT PINGRSP OK\r\n";
									AT_WriteUart(tmp, sizeof(tmp));
								}
							}
							break;
						default:
							AT_TC(g_sw_TCPIP, "MQTT RECEIVE UNKOWN TYPE PACKET %d\r\n", packet_type);
							break;
					}
				}
				break;

			case EV_CFW_TCPIP_CLOSE_IND:
			case EV_CFW_MQTT_DISCONNECT:
				{
					mqtt_clear_topicsArr();

					g_mqtt_stat.mqttClient->isconnected = MQTT_STAT_DISCONNECT;
					MQTTDisconnect(&s_mqttClient);
					(s_mqttNet.disconnect)(&s_mqttNet);

					AT_TC(g_sw_TCPIP, "MQTT DISC OK\r\n");

					if (s_mqttClient.reconEn == TRUE) {
						mqtt_reset_contimer(s_mqtt_con_timeout);
					} else {
						AT_TC(g_sw_TCPIP, "[MQTT]NO RECONNECTING");
					}

					mqtt_stop_pingTimer();
				}
				break;

			case EV_TIMER:
				if (ev.nParam1 == MQTT_SERVER_CONNECT_TIMER) {
					c->ipstack->disconnect(c->ipstack);

					if (s_mqttClient.reconEn == TRUE) {
						ev.nEventId = EV_CFW_MQTT_CONNECT;
						COS_SendEvent(g_mqtt_task, &ev, COS_WAIT_FOREVER, COS_EVENT_PRI_NORMAL);
						c->reconNum++;
					}
				}

				if (ev.nParam1 == MQTT_MSG_PINGREQ_TIMER) {
					int rc = MQTT_FAILURE;
					int len = MQTTSerialize_pingreq(c->buf, c->buf_size);
					if (len > 0 && (rc = mqtt_sendPacket(c, len)) == MQTT_SUCCESS)
						c->ping_outstanding = 1;
					mqtt_reset_pingTimer(c->keepAliveInterval * 16348 / 2);
				}

				if (ev.nParam1 == MQTT_MSG_CMD_TIMER) {
					MQTTDisconnect(&s_mqttClient);
					(s_mqttNet.disconnect)(&s_mqttNet);

					AT_TC(g_sw_TCPIP,"WAIT CMD ACK TIMEOUT, RECONNECTED\r\n");
					c->isconnected = MQTT_STAT_DISCONNECT;

					if (s_mqttClient.reconEn == TRUE) {
						ev.nEventId = EV_CFW_MQTT_CONNECT;
						COS_SendEvent(g_mqtt_task, &ev, COS_WAIT_FOREVER, COS_EVENT_PRI_NORMAL);
						c->reconNum++;
					}
				}
				break;

			default:
				AT_TC(g_sw_TCPIP, "INPUT PARAM ERROR,id=%d\r\n", ev.nEventId);
				break;
		}
	}
}

extern HANDLE COS_CreateTask_Prv(PTASK_ENTRY, PVOID, PVOID, UINT16, UINT8, UINT16, UINT16, PCSTR);
void mqtt_init(MQTT_CFG_T *mqtt_cfg)
{
	if (g_mqtt_task == NULL) {
		//memset(&g_mqtt_cfg, 0, sizeof(g_mqtt_cfg));
		memset(&g_mqtt_stat, 0, sizeof(g_mqtt_stat));

		init_mqtt_client();

		g_mqtt_task = COS_CreateTask_Prv(bal_mqtt_tsk_func, NULL, NULL,
				BAL_MQTT_STACK_SIZE, BAL_MQTT_TASK_PRIORITY, COS_CREATE_DEFAULT, 0, "bal_mqtt_tsk_func");
	}
}

void set_mqtt_cfg(MQTT_CFG_T *p_mqtt_cfg)
{
	memcpy(&g_mqtt_cfg, p_mqtt_cfg, sizeof(g_mqtt_cfg));
}

void get_mqtt_cfg(MQTT_CFG_T *p_mqtt_cfg)
{
	memcpy(p_mqtt_cfg, &g_mqtt_cfg, sizeof(g_mqtt_cfg));
}

void mqtt_exit(void)
{
	if (g_mqtt_task != NULL) {
		COS_DeleteTask(g_mqtt_task); 
		g_mqtt_task = NULL;
	}
}

