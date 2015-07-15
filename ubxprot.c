/*
 * ubxprot.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include "ubxprot.h"
#include "dbgif.h"

void ubx_addchecksum(U8 * msg);

#define MAX_MESSAGES_NUM    10
#define MAX_MESSAGEBUF_NUM    5

static U8 ubx_messages[MAX_MESSAGEBUF_NUM][MAX_MESSAGEBUF_LEN];

static Message_s gMessage[MAX_MESSAGES_NUM];

void ubx_init(void)
{
    I16 i;

    for (i = MAX_MESSAGES_NUM-1; i >= 0; i--)
    {
    	gMessage[i].id = MessageIdNone;
    	gMessage[i].confirmed = false;
    	gMessage[i].pMsgBuff = 0;
    }

    gMessage[MessageIdPollCfgNmea].id = MessageIdPollCfgNmea;
    gMessage[MessageIdPollCfgNmea].pMsgBuff = ubx_messages[BufferIdPollSetCfgNmea];
    gMessage[MessageIdPollCfgNmea].confirmType = TypeOfConfirmMsg;
    gMessage[MessageIdPollCfgNmea].pHead = (UbxPckHeader_s *) gMessage[MessageIdPollCfgNmea].pMsgBuff;

    gMessage[MessageIdPollCfgPrt].id = MessageIdPollCfgPrt;
    gMessage[MessageIdPollCfgPrt].pMsgBuff = ubx_messages[BufferIdPollSetCfgPrt];
    gMessage[MessageIdPollCfgPrt].confirmType = TypeOfConfirmMsg;
    gMessage[MessageIdPollCfgPrt].pHead = (UbxPckHeader_s *) gMessage[MessageIdPollCfgPrt].pMsgBuff;

    gMessage[MessageIdSetCfgPrt].id = MessageIdSetCfgPrt;
    gMessage[MessageIdSetCfgPrt].pMsgBuff = ubx_messages[BufferIdPollSetCfgPrt];
    gMessage[MessageIdSetCfgPrt].confirmType = TypeOfConfirmAck;
    gMessage[MessageIdSetCfgPrt].pHead = (UbxPckHeader_s *) gMessage[MessageIdSetCfgPrt].pMsgBuff;
}

const Message_s * ubx_get_msg(MessageId_e msgId)
{
	Message_s * retVal;

	switch(msgId)
	{
	case MessageIdPollCfgNmea:
		retVal = &gMessage[MessageIdPollCfgNmea];
		ubx_poll_cfgnmea(retVal->pMsgBuff);
		break;
	case MessageIdPollCfgPrt:
		retVal = &gMessage[MessageIdPollCfgPrt];
		ubx_poll_cfgprt(retVal->pMsgBuff);
		break;
	case MessageIdSetCfgPrt:
		retVal = &gMessage[MessageIdSetCfgPrt];
		ubx_set_cfgprt(retVal->pMsgBuff);
		break;
	default:
		retVal = MessageIdNone;
		break;
	}

	retVal->confirmed = false;
	return retVal;
}

U16 ubx_poll_cfgnmea(U8 * msg)
{
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) msg;

	pHead->ubxClass = ubxClassCfg;
	pHead->ubxId = UbxClassIdCfgNmea;
	pHead->length = 0;

	ubx_addchecksum(msg);

	return 0;
}

U16 ubx_poll_cfgprt(U8 * msg)
{
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) msg;

	pHead->ubxClass = ubxClassCfg;
	pHead->ubxId = UbxClassIdCfgPrt;
	pHead->length = 0;

	ubx_addchecksum(msg);

	return 0;
}


U16 ubx_set_cfgprt(U8 * msg)
{
    UbxCfgPrt_s *pBody;
    U16 retVal = 0;

    pBody = (UbxCfgPrt_s *) (msg + sizeof(UbxPckHeader_s));
    pBody->outProtoMask = 1; //keep only UBX messages, disable NMEA

    ubx_addchecksum(msg);

    return retVal;
}

/** @brief Add U-BLOX checksum to the end of message */
void ubx_addchecksum(U8 * msg)
{
    UbxPckHeader_s * pHead;
    UbxPckChecksum_s * pCheckSum;
    U8 * buffForChecksum;
    U16 checksumByteRange;

    pHead = (UbxPckHeader_s *) msg;

    pCheckSum = (UbxPckChecksum_s *) (msg + sizeof(UbxPckHeader_s) + pHead->length);

    checksumByteRange = pHead->length + 4; //4 stays for class, ID, length field
    buffForChecksum = (U8 *) &pHead->ubxClass;

    ubx_genchecksum(buffForChecksum, checksumByteRange, &pCheckSum->cka, &pCheckSum->ckb);
}

/** @brief Compute U-BLOX proprietary protocol checksum
 *
 * @param pBuff pointer to the buffer over which is the
 * checksum computed
 * @param len length of the input buffer
 * @param pCka pointer to the first byte of the checksum
 * @param pCkb pointer to the second byte of the checksum */
void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb)
{
	U16 i;

	*pCka = 0;
	*pCkb = 0;

	for(i = 0; i < len; i++)
	{
		*pCka = *pCka + pBuff[i];
		*pCkb = *pCkb + *pCka;
	}
}

/** @brief Process U-BLOX proprietary protocol message
 *
 * @param msg pointer to UBX message */
U16 ubx_checkmsg(U8 * pMsg)
{
	UbxPckHeader_s * pMsgHead;
	UbxPckChecksum_s * pMsgCs;
	UbxPckChecksum_s cs;
	U8 * buffForChecksum;
	U16 checksumByteRange;

	U16 retVal = 0;

	pMsgHead = (UbxPckHeader_s *) pMsg;
	pMsgCs = (UbxPckChecksum_s *) (pMsg + sizeof(UbxPckHeader_s) + pMsgHead->length);

	checksumByteRange = pMsgHead->length + 4; //4 stays for class, ID, length field
	buffForChecksum = (U8 *) &pMsgHead->ubxClass;

	ubx_genchecksum(buffForChecksum, checksumByteRange, &cs.cka, &cs.ckb);

	if ((pMsgCs->cka != cs.cka) || (pMsgCs->cka != cs.cka))
	{
		retVal = 1;
	}

	return retVal;
}

void ubx_msgst(const Message_s * pLastMsg, const U8 * pNewMsgData)
{
    UbxPckHeader_s * pNewHead = (UbxPckHeader_s *) pNewMsgData;
    UbxPckHeader_s * pLastHead = (UbxPckHeader_s *) pLastMsg->pMsgBuff;
    UbxAckMsg_s * pAckHead;

    switch (pLastMsg->id)
    {
    case MessageIdPollCfgPrt:
    	//check if the same message came
    	if ((pNewHead->ubxClass == pLastHead->ubxClass) &&
    			(pNewHead->ubxId == pLastHead->ubxId))
    	{
    		ubx_msg_polled(pLastMsg, pNewMsgData);
    	}
    	break;
    case MessageIdSetCfgPrt:
    	//check if it a ACK was received
    	if (pNewHead->ubxClass == UbxClassIdAck &&
    			pNewHead->ubxId == UbxClassIdAckAck)
    	{
    		//we got ACK, check message ack class, id
    		pAckHead = (UbxAckMsg_s *) (pNewMsgData + sizeof(UbxPckHeader_s));
    		if (pAckHead->ackMsgCls == pLastHead->ubxClass &&
    				pAckHead->ackMsgId == pLastHead->ubxId)
    		{
    			//message ACKnowledged:
    			ubx_msg_confirmed(pLastMsg);
    		}
    		else
    		{
    			dbg_txerrmsg(4);
    		}
    	}
    	else
    	{
    		dbg_txerrmsg(5);
    	}
    	break;
    default:
    	dbg_txerrmsg(2);
    	break;
    }
}

void ubx_msg_polled(const Message_s * pLastMsg, const U8 * pNewMsgData)
{
	U16 i;
	Message_s * pUpdateMsg;
	//message polled, update fields:
	pUpdateMsg = &gMessage[pLastMsg->id];

	for (i = 0; i < MAX_MESSAGEBUF_LEN; i++)
	{
		pUpdateMsg->pMsgBuff[i] = pNewMsgData[i];
	}
	pUpdateMsg->confirmed = true;
}

void ubx_msg_confirmed(const Message_s * pLastMsg)
{
	Message_s * pUpdateMsg;

	pUpdateMsg = &gMessage[pLastMsg->id];
	pUpdateMsg->confirmed = true;
}
