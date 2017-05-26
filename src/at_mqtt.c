/*****************************************************************************
 *  \brief MQTT AT命令接口
 *  Copyright (c) 2016 China Mobile IOT Company.
 *
 *  **************************************************************
 *
 *  \file at_mqtt.c
 *  \author 智能模组软件一组
 *  \create Time: 2016-5-20
 *  \modified Time:
 ******************************************************************************/

#include "stdio.h"
#include "string.h"
#include "at.h"
#include "at_common.h"
#include "at_module.h"
#include "at_trace.h"
#include "at_cmd_id.h"
#include "cs_types.h"
#include "at_cmd_tcpip.h"
#include "at_utility.h"
#include "at_errcode.h"
#include "at_define.h"

#include "mqtt_tsk.h"
#include "MQTTClient.h"

extern MQTT_STAT_T g_mqtt_stat;

static char mqtt_rsp_str[1024];
static char mqtt_pub_paload[1024];
extern uint16 g_auto_send_len; 
extern uint8  g_send_fixed_size_flag;

extern HANDLE g_mqtt_task;

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

	va_start(args, format);
	AT_Sprintf(tmp, format, args); \
	Tmp = SUL_StrVPrint(tmp, format, args);
	va_end(args);
	AT_WriteUart(tmp, sizeof(tmp));
}

/*****************************************************************************
 *  \brief 读取当前nv中mqtt参数
 *
 *  \return 读取成功或失败
 *
 *  \details 读取失败则原配置不变
 ******************************************************************************/
static bool mqtt_cfg_readnv(void)
{
	return TRUE;
}

/*****************************************************************************
 *  \brief 保存当前mqtt配置参数到NV
 *
 *  \return 保存成功或者失败
 *
 *  \details 如果在保存过程中失败，有可能部分参数已保存，部分未保存
 ******************************************************************************/
static bool mqtt_cfg_setnv(void)
{
	return TRUE;
}

/*****************************************************************************
 *  \brief 配置MQTT传输连接参数
 *  AT+MQTTCFG=<host>,<port>,<id>,<keepAlive>,<user>,<passwd>,<clean>
 *  \param [in] host，port	-->MQTT服务器地址与端口
 *  \param [in] id			-->终端ID
 *  \param [in] keepAlive	-->保持连接的时间
 *  \param [in] user		-->MQTT登录用户名
 *  \param [in] passwd		-->MQTT登录密码
 *  \param [in] clean		-->是否清除回话
 *  \return Return_Description
 *
 *  \details Details
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTCFG(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);
	MQTT_CFG_T tmp_mqtt_cfg;

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	memset(&tmp_mqtt_cfg, 0, sizeof(tmp_mqtt_cfg));
	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				if (iCount != 7) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(tmp_mqtt_cfg.host);
				memset(tmp_mqtt_cfg.host, 0, sizeof(tmp_mqtt_cfg.host));
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_STRING, tmp_mqtt_cfg.host, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.port);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT16, &tmp_mqtt_cfg.port, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.clientId);
				memset(tmp_mqtt_cfg.clientId, 0, sizeof(tmp_mqtt_cfg.clientId));
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_STRING, tmp_mqtt_cfg.clientId, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.keepAlive);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT16, &tmp_mqtt_cfg.keepAlive, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.user);
				memset(tmp_mqtt_cfg.user, 0, sizeof(tmp_mqtt_cfg.user));
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_STRING, tmp_mqtt_cfg.user, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.passwd);
				memset(tmp_mqtt_cfg.passwd, 0, sizeof(tmp_mqtt_cfg.passwd));
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_STRING, tmp_mqtt_cfg.passwd, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(tmp_mqtt_cfg.cleanSession);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &tmp_mqtt_cfg.cleanSession, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
			}
			mqtt_cfg_setnv();
			set_mqtt_cfg(&tmp_mqtt_cfg);

			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			break;
		case AT_CMD_READ:
			get_mqtt_cfg(&tmp_mqtt_cfg);
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTCFG: \"%s\",%d,\"%s\",%d,\"%s\",\"%s\",%d",
					tmp_mqtt_cfg.host, tmp_mqtt_cfg.port, tmp_mqtt_cfg.clientId,
					tmp_mqtt_cfg.keepAlive, tmp_mqtt_cfg.user,  tmp_mqtt_cfg.passwd,
					tmp_mqtt_cfg.cleanSession);
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0,
					mqtt_rsp_str, AT_StrLen(mqtt_rsp_str), pParam->nDLCI);
			break;
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

void init_mqtt_cfg(void)
{
	MQTT_CFG_T mqtt_cfg;
	memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
	set_mqtt_cfg(&mqtt_cfg);
	mqtt_cfg_readnv();
	AT_TC(g_sw_TCPIP, "[MQTT] init_mqtt_cfg done");
}

/*****************************************************************************
 *  \brief 打开MQTT服务器连接
 *  AT+MQTTOPEN=<usrFlag>,<pwdFlag>,<willFlag>,<willRetain>,<willQos>,<will-topic>,<will-mesg>
 *  \param [in] usrFlag	-->是否启动用户名
 *  \param [in] pwdFlag	-->是否启用密码
 *  \param [in] willFlag	-->是否启用will
 *  \param [in] willRetain	-->服务器是否保存will消息
 *  \param [in] willQos		-->will Qos级别
 *  \param [in] will-topic	-->连接异常时发布消息到对应topic
 *  \param [in] will-mesg	-->连接异常时发布的消息
 *  \return Return_Description
 *
 *  \details 从NV中读取设置后自动打开服务器连接(改为开机从nv中读取一次，后续不从nv读取以减少flash访问)
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTOPEN(AT_CMD_PARA *pParam)
{
	COS_EVENT ev;
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			mqtt_init(NULL);
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				if (iCount != 7) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len =  sizeof(g_mqtt_stat.conMsg.usrFlag);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &g_mqtt_stat.conMsg.usrFlag, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len =  sizeof(g_mqtt_stat.conMsg.pwdFlag);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &g_mqtt_stat.conMsg.pwdFlag, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len =  sizeof(g_mqtt_stat.conMsg.willFlag);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &g_mqtt_stat.conMsg.willFlag, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len =  sizeof(g_mqtt_stat.conMsg.willRetain);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &g_mqtt_stat.conMsg.willRetain, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len =  sizeof(g_mqtt_stat.conMsg.willQos);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &g_mqtt_stat.conMsg.willQos, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				if (g_mqtt_stat.conMsg.willTopic.cstring == NULL) {
					g_mqtt_stat.conMsg.willTopic.cstring = (UINT8 *)AT_MALLOC(MQTT_TOPIC_MAX_LEN);
					if (g_mqtt_stat.conMsg.willTopic.cstring == NULL) {
						AT_TCPIP_Result_Err(ERR_AT_CME_NO_MEMORY, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
						return;
					}
					memset(g_mqtt_stat.conMsg.willTopic.cstring, 0, MQTT_TOPIC_MAX_LEN);
				}
				len = MQTT_TOPIC_MAX_LEN;
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++, AT_UTIL_PARA_TYPE_STRING,
						g_mqtt_stat.conMsg.willTopic.cstring, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				if (g_mqtt_stat.conMsg.willMsg.cstring == NULL) {
					g_mqtt_stat.conMsg.willMsg.cstring = (UINT8 *)AT_MALLOC(MQTT_TOPIC_MAX_LEN);
					if (g_mqtt_stat.conMsg.willMsg.cstring == NULL) {
						AT_TCPIP_Result_Err(ERR_AT_CME_NO_MEMORY, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
						return;
					}
					memset(g_mqtt_stat.conMsg.willMsg.cstring, 0, MQTT_TOPIC_MAX_LEN);
				}
				len = MQTT_TOPIC_MAX_LEN;
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++, AT_UTIL_PARA_TYPE_STRING,
						g_mqtt_stat.conMsg.willMsg.cstring, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
			}
			break;
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			return;
	}

	if (g_mqtt_stat.mqttClient->isconnected == MQTT_STAT_DISCONNECT) {
		g_mqtt_stat.mqttClient->isconnected = MQTT_STAT_CONNECTING;
		ev.nEventId = EV_CFW_MQTT_CONNECT;
		ev.nParam1 = pParam->nDLCI;
		COS_SendEvent(g_mqtt_task, &ev, COS_WAIT_FOREVER, COS_EVENT_PRI_NORMAL);
		AT_TCPIP_Result_OK(CMD_FUNC_SUCC_ASYN, CMD_RC_CR, 0, 0, 0, pParam->nDLCI);
	} else {
		AT_TC(g_sw_TCPIP, "+CME ERROR: ERR_MQTT_CONOPR\r\n");
		AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
	}

	return;
}

/*****************************************************************************
 *  \brief 读取MQTT状态
 *  AT+MQTTSTAT?
 *  \param [in] None
 *  \param [in]
 *  \return Return_Description
 *
 *  \details 返回连接状态
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTSTAT(AT_CMD_PARA *pParam)
{
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_READ:
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTSTAT: %d", g_mqtt_stat.mqttClient->isconnected);
			AT_WriteUart(mqtt_rsp_str, AT_StrLen(mqtt_rsp_str));
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			break;
		case AT_CMD_SET:
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

//+MQTTPUBLISH: <dup>,<qos>,<retained>,<packId>,<topic>,<msg_len>,<mesg><\r\n>
//如果订阅消息的topic有推送，则会自动显示此消息
//TODO--如果是长信息，可能会分包抵达，此处只能显示第一包数据
void mqtt_subMsgHandler(MessageData* md)
{
	static char g_rsp_str[1024];
	MQTTMessage* message = md->message;
	int lenm;
	int tmp;
	Client* c = g_mqtt_stat.mqttClient;

	int payLoadLen = c->ipstack->packLen - message->payloadlen;//本包消息长度

	memset(g_rsp_str,0, sizeof(g_rsp_str));
	sprintf(g_rsp_str,"\r\n+MQTTPUBLISH: %d,%d,%d,%d,\"",message->dup,message->qos,message->retained,message->id);
	lenm = strlen(g_rsp_str);
	memcpy(g_rsp_str + lenm, md->topicName->lenstring.data, md->topicName->lenstring.len);
	lenm += md->topicName->lenstring.len;
	tmp = sprintf(g_rsp_str + lenm,"\",%d\r\n",c->payLoadSize - message->payloadlen);//消息总长度
	lenm += tmp;
	if (payLoadLen > 0) {
		memcpy(g_rsp_str + lenm, (char*) message->payload,payLoadLen);
		lenm += payLoadLen;
	}

	if (g_mqtt_stat.mqttClient->needByte == 0) {
		g_rsp_str [lenm] = '\r';
		g_rsp_str [lenm + 1] = '\n';
		lenm += 2;
	}

	AT_WriteUart(g_rsp_str, lenm);	
}

/*****************************************************************************
 *  \brief 订阅MQTT消息
 *  AT+MQTTSUB=<topic>,<qos>
 *  \param [in] topic		-->topic
 *  \param [in] qos			-->qos(0-2)
 *  \return Return_Description
 *
 *  \details 设置命令用于订阅topic，查询命令用于查询当前已订阅topic
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTSUB(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);
	UINT8 qos = QOS0;

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				UINT8 topic_Name[MQTT_TOPIC_MAX_LEN];

				if (iCount != 2) {
					AT_TC(g_sw_TCPIP, "MQTTSUB:INPUT PARAM NUMBER EEROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(topic_Name);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++, AT_UTIL_PARA_TYPE_STRING,
						topic_Name, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "MQTTSUB:INPUT PARAM TOPIC NAME EEROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				if (mqtt_is_topicExist(topic_Name) == TRUE) {
					memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
					AT_Sprintf(mqtt_rsp_str, "ERR MQTT SUBTOPICEXIST\r\n");
					AT_WriteUart(mqtt_rsp_str, AT_StrLen(mqtt_rsp_str));
					AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				mqtt_add_topictArr(topic_Name);

				len = sizeof(qos);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &qos, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "MQTTSUB:INPUT PARAM QOS EEROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				eResult = MQTTSubscribe(g_mqtt_stat.mqttClient, g_mqtt_stat.mqttClient->tmpTopics,
						qos, mqtt_subMsgHandler);
				if (eResult != ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTSUB SUBSCRIBE ERROR, %d\r\n", eResult);
					AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				AT_TCPIP_Result_OK(CMD_FUNC_SUCC_ASYN, CMD_RC_CR, 0, 0, 0, pParam->nDLCI);
			}
			break;

		case AT_CMD_READ:
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTSUB: \r\n");
			mqtt_print_topicsArr(mqtt_rsp_str + strlen(mqtt_rsp_str));
			AT_WriteUart(mqtt_rsp_str, AT_StrLen(mqtt_rsp_str));
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			break;

		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief 发布MQTT消息
 *  AT+MQTTPUB=<topic>,<qos>,<retain>,<dup>,<message>
 *  \param [in] topic			-->topic
 *  \param [in] qos				-->qos(0-2)
 *  \param [in] retain			-->服务器是否保存消息
 *  \param [in] dup				-->
 *  \param [in] message			-->消息内容(string)或消息长度(numeric)
 *  \return Return_Description
 *
 *  \details 当message为数字时，输入固定长度数据自动发布消息，
 *  会根据设置的qos，返回不同的ACK URC消息
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTPUB(AT_CMD_PARA *pParam)
{
	MQTTMessage msg;

	INT32 eResult;
	UINT8 iCount = 0, i, qos;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TC(g_sw_TCPIP, "HAVE NOT MQTT CLIENT\r\n");
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				UINT8 topic_Name[MQTT_TOPIC_MAX_LEN];

				if (iCount != 5) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB INPUT PARAM NUM ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(topic_Name);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++, AT_UTIL_PARA_TYPE_STRING,
						topic_Name, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB TOPICNAME ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(qos);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &qos, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB QOS ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				msg.qos = qos;

				len = sizeof(msg.retained);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &msg.retained, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB RETAINED ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(msg.dup);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &msg.dup, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB DUP ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(mqtt_pub_paload);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++, AT_UTIL_PARA_TYPE_STRING,
						mqtt_pub_paload, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB MESSAGE ERROR\r\n");
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				msg.payloadlen = len;
				msg.payload = mqtt_pub_paload;
				eResult = MQTTPublish(g_mqtt_stat.mqttClient, topic_Name, &msg);
				if (eResult != ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTPUB PUBLISH ERROR, %d\r\n", eResult);
					AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				if (msg.qos == QOS0) {
					AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
				} else {
					AT_TCPIP_Result_OK(CMD_FUNC_SUCC_ASYN, CMD_RC_CR, 0, 0, 0, pParam->nDLCI);
				}
			} else {
				AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			}
			break;

		default:
			AT_TC(g_sw_TCPIP, "UNSURPORED OPERASION\r\n");
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief 取消订阅的topic
 *  AT+MQTTUNSUB=<topic>
 *
 *  \param [in] topic			-->topic
 *  \return Return_Description
 *
 *  \details 取消订阅之后将不再接收该topic消息，并且使用+MQTTSUB将不再显示该topic
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTUNSUB(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				UINT8 topic_Name[MQTT_TOPIC_MAX_LEN];

				if (iCount != 1) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				len = sizeof(topic_Name);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, 0, AT_UTIL_PARA_TYPE_STRING,
						topic_Name, &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				if (mqtt_is_topicExist(topic_Name) != TRUE) {
					memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
					AT_Sprintf(mqtt_rsp_str, "ERR MQTT SUBTOPICNONEXIST\r\n");
					AT_WriteUart(mqtt_rsp_str, AT_StrLen(mqtt_rsp_str));
					AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				mqtt_add_topictArr(topic_Name);

				eResult = MQTTUnsubscribe(g_mqtt_stat.mqttClient, topic_Name);
				if (eResult != ERR_SUCCESS) {
					AT_TC(g_sw_TCPIP, "+MQTTUNSUB UNSUBSCRIBE ERROR, %d\r\n", eResult);
					AT_TCPIP_Result_Err(ERR_AT_CME_EXE_FAIL, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				AT_TCPIP_Result_OK(CMD_FUNC_SUCC_ASYN, CMD_RC_CR, 0, 0, 0, pParam->nDLCI);
			}
			break;

		default:
			AT_TC(g_sw_TCPIP, "MQTTUNSUB CAMMAND TYPE ERROR\r\n");
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief 断开服务器连接
 *  AT+MQTTDISC
 *
 *  \return Return_Description
 *
 *  \details 发送DISCONNECT包
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTDISC(AT_CMD_PARA *pParam)
{
	COS_EVENT ev;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (g_mqtt_stat.mqttClient->isconnected) {
		case MQTT_STAT_DISCONNECT:
			{
				memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
				AT_Sprintf(mqtt_rsp_str,"UNEXISTED CONNECTED MQTT CLIENT\r\n");
				AT_WriteUart(mqtt_rsp_str, AT_StrLen(mqtt_rsp_str));
				AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			}
			break;

		default:
			ev.nEventId = EV_CFW_MQTT_DISCONNECT;
			COS_SendEvent(g_mqtt_task, &ev, COS_WAIT_FOREVER, COS_EVENT_PRI_NORMAL);
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief 发送ping包-->时间不能设置，时间由KeepAlive参数指定
 *  AT+MQTTPING=<time>,<rspEcho>  --->AT+MQTTPING=<rspEcho>
 *  \param [in] time			-->发送ping包的时间间隔->连接之后设置无效
 *  \param [in] rspEcho			-->是否显示pingresp
 *  \return Return_Description
 *
 *  \details 如果无数据传输，则ping时间间隔需要小于Keep Alive时间*1.5，
 *  否则服务器会主动中断连接；
 *  如果一定时间检测不到PINGRESP，则终端需要主动断开连接
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTPING(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				if (iCount != 1) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(g_mqtt_stat.mqttClient->pingEchoEn);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &(g_mqtt_stat.mqttClient->pingEchoEn), &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			}
			break;
		case AT_CMD_READ:
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTPING: %d", g_mqtt_stat.mqttClient->pingEchoEn);
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0,
					mqtt_rsp_str, sizeof(mqtt_rsp_str), pParam->nDLCI);
			break;
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief 设置是否开启自动重连（非主动关闭mqtt连接）
 *  AT+MQTTREC=<recon>
 *  \param [in] recon 0-关闭自动重连 1-开启         
 *  \return Return_Description
 *  查询命令返回是否重连以及已经重连的次数
 *  \details 当开启时，由于超时或网络原因引起的掉线将触发自动重连
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTREC(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				if (iCount != 1) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(g_mqtt_stat.mqttClient->reconEn);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT8, &(g_mqtt_stat.mqttClient->reconEn), &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			}
			break;
		case AT_CMD_READ:
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTREC: %d", g_mqtt_stat.mqttClient->reconEn);
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0,
					mqtt_rsp_str, sizeof(mqtt_rsp_str), pParam->nDLCI);
			break;
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

/*****************************************************************************
 *  \brief  设置终端命令超时时间
 *  AT+MQTTTO=<timeout>
 *  \param [in] timeout 超时时间
 *  \return Return_Description
 *  
 *  \details 当超时时间到还未收到服务器ACK则终端将断开连接
 ******************************************************************************/
void AT_MQTT_Cmd_MQTTTO(AT_CMD_PARA *pParam)
{
	INT32 eResult;
	UINT8 iCount = 0, i;
	UINT16 len  = 0;
	UINT8 nSim = AT_SIM_ID(pParam->nDLCI);

	if (pParam == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_MEMORY_FAILURE, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	if (g_mqtt_stat.mqttClient == NULL) {
		AT_TCPIP_Result_Err(ERR_AT_CME_EXE_NOT_SURPORT, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
		return;
	}

	switch (pParam->iType) {
		case AT_CMD_SET:
			eResult = AT_Util_GetParaCount(pParam->pPara, &iCount);
			if (eResult == ERR_SUCCESS) {
				if (iCount != 1) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}

				i = 0;
				len = sizeof(g_mqtt_stat.mqttClient->command_timeout_ms);
				eResult = AT_Util_GetParaWithRule(pParam->pPara, i++,
						AT_UTIL_PARA_TYPE_UINT32, &(g_mqtt_stat.mqttClient->command_timeout_ms), &len);
				if (eResult !=  ERR_SUCCESS) {
					AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
					return;
				}
				g_mqtt_stat.mqttClient->command_timeout_ms *=  1000; //s->ms
				AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0, 0, 0, pParam->nDLCI);
			}
			break;
		case AT_CMD_READ:
			memset(mqtt_rsp_str, 0, sizeof(mqtt_rsp_str));
			AT_Sprintf(mqtt_rsp_str, "+MQTTREC: %d", g_mqtt_stat.mqttClient->command_timeout_ms / 1000);
			AT_TCPIP_Result_OK(CMD_FUNC_SUCC, CMD_RC_OK, 0,
					mqtt_rsp_str, sizeof(mqtt_rsp_str), pParam->nDLCI);
			break;
		default:
			AT_TCPIP_Result_Err(ERR_AT_CME_PARAM_INVALID, CMD_ERROR_CODE_TYPE_CME, pParam->nDLCI);
			break;
	}

	return;
}

