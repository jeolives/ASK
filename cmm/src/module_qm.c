/*
 *
 *  Copyright (C) 2007 Mindspeed Technologies, Inc.
 *  Copyright 2014-2016 Freescale Semiconductor, Inc.
 *  Copyright 2017,2021 NXP
 *
 * SPDX-License-Identifier:    GPL-2.0+
 * The GPL-2.0+ license for this file can be found in the COPYING.GPL file
 * included with this distribution or at http://www.gnu.org/licenses/gpl-2.0.html
 *
 *
 */

#include "cmm.h"
#include "fpp.h"
#include <ctype.h>
#include <limits.h>

/************************************************************
 *
 *
 *
 ************************************************************/
void cmmQmShowPrintHelp()
{
	cmm_print(DEBUG_STDOUT, "show qm not yet supported\n");
}


/************************************************************
 *
 *
 *
 ************************************************************/
int cmmQmShowProcess(char ** keywords, int tabStart, daemon_handle_t daemon_handle)
{
	
//help:
	cmmQmShowPrintHelp();
	return -1;
}

int cmmQmExptRateQueryProcess(char ** keywords, int tabStart, daemon_handle_t daemon_handle)
{
	int cpt = tabStart;
	int rcvBytes = 0;
	union u_rxbuf rxbuf;
	short rc;
	fpp_qm_expt_rate_cmd_t *pExptRateCmd = ( fpp_qm_expt_rate_cmd_t *)&rxbuf.rcvBuffer;

	if(!keywords[cpt])
		goto help;
	memset(pExptRateCmd, 0, sizeof(fpp_qm_expt_rate_cmd_t));
	if (strcasecmp(keywords[cpt], "eth") == 0)
		pExptRateCmd->if_type = FPP_EXPT_TYPE_ETH;
	else
		goto help;
#ifdef LS1043
	if (keywords[++cpt]) {
		if (strcmp(keywords[cpt], "reset") == 0)
			pExptRateCmd->clear = 1;		
	} 
#endif

   	rcvBytes = cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QUERY_EXPT_RATE , 
                            pExptRateCmd, sizeof(fpp_qm_expt_rate_cmd_t), rxbuf.rcvBuffer);
	
   	if (rcvBytes < sizeof( fpp_qm_expt_rate_cmd_t)  ) {
   		rc = (rcvBytes < sizeof(unsigned short) ) ? 0 : rxbuf.result;
                if (rc == FPP_ERR_UNKNOWN_ACTION) {
                    cmm_print(DEBUG_STDERR, "ERROR: does not support ACTION_QUERY\n");
                } else {
                    cmm_print(DEBUG_STDERR, "ERROR: Unexpected result returned from FPP FPP_CMD_QM_QUERY_EXPT_RATE cmd, rc:%d\n", rc);
                }
                return CLI_OK;
	}
   	cmm_print(DEBUG_STDOUT, "QM Exception RATE (packets/sec): %d \nBurst size(packets/usec):\t%d \n\n", 
					pExptRateCmd->pkts_per_sec,  pExptRateCmd->burst_size);
#ifdef LS1043
	cmm_print(DEBUG_STDOUT, "Red (dropped) packets   \t%u\n", pExptRateCmd->counterval[RED_TOTAL]);
	cmm_print(DEBUG_STDOUT, "Yellow packets          \t%u\n", pExptRateCmd->counterval[YELLOW_TOTAL]);
	cmm_print(DEBUG_STDOUT, "Green packets           \t%u\n", pExptRateCmd->counterval[GREEN_TOTAL]);
	cmm_print(DEBUG_STDOUT, "packets recolored red   \t%u\n", pExptRateCmd->counterval[RED_RECOLORED]);
	cmm_print(DEBUG_STDOUT, "packets recolored yellow\t%u\n", pExptRateCmd->counterval[YELLOW_RECOLORED]);
#endif
   	return CLI_OK;
help:
	cmm_print(DEBUG_STDOUT, "Usage: query qmexptrate {eth}\n");
	return CLI_OK;
}

#ifdef LS1043
/*
 * This function query DSCP FQ mapping. It gets mapping status on interface, if it is enable
 * it also gets each DSCP mapped FQID value.
*/
int cmmQmDSCPFqMapQueryProcess(char ** keywords, int cpt, daemon_handle_t daemon_handle)
{
	int rcvBytes = 0;
	union u_rxbuf rxbuf;
	short rc;
	short index;
	fpp_qm_iface_dscp_fqid_map_cmd_t *pDscpFqMapCmd = (fpp_qm_iface_dscp_fqid_map_cmd_t *)&rxbuf.rcvBuffer;

	if(!keywords[cpt])
		goto help;
	memset(pDscpFqMapCmd, 0, sizeof(fpp_qm_iface_dscp_fqid_map_cmd_t));
	if (get_port_id(keywords[cpt]) >= 0)
	{
		STR_TRUNC_COPY(pDscpFqMapCmd->interface, keywords[cpt], sizeof(pDscpFqMapCmd->interface));
	}
	else {
		cmm_print(DEBUG_CRIT, "ERROR: invalid interface name(%s)\n", keywords[cpt]);
		goto help;
	}

	rcvBytes = cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QUERY_IFACE_DSCP_FQID_MAP,
			pDscpFqMapCmd, sizeof(fpp_qm_iface_dscp_fqid_map_cmd_t), rxbuf.rcvBuffer);

	if (rcvBytes < sizeof(fpp_qm_iface_dscp_fqid_map_cmd_t)  ) {
		rc = (rcvBytes < sizeof(unsigned short) ) ? 0 : rxbuf.result;
		cmm_print(DEBUG_STDERR, "ERROR: Unexpected result returned from FPP_CMD_QM_QUERY_IFACE_DSCP_FQ_MAP cmd, rc:%d rcvBytes %d\n", rc, rcvBytes);
		return CLI_OK;
	}

	cmm_print(DEBUG_STDOUT, "QM DSCP FQ Map :\n");
	cmm_print(DEBUG_STDOUT, "Status : %s\n", pDscpFqMapCmd->enable ? "Enable" : "Disable");
	if (pDscpFqMapCmd->enable)
	{
		cmm_print(DEBUG_STDOUT, "Below information is fqid configured for each dscp value(0 means fqid not configured)\n");
		for (index = 0; index < FPP_NUM_DSCP; index++)
			cmm_print(DEBUG_STDOUT, "dscp[%d] : 0x%x(policer profile: 0x%x(%d) fqid: 0x%x(%d))\n", 
				index, pDscpFqMapCmd->fqid[index], 
				pDscpFqMapCmd->fqid[index] >> 24, pDscpFqMapCmd->fqid[index] >> 24,
				(pDscpFqMapCmd->fqid[index] << 8) >>8, (pDscpFqMapCmd->fqid[index] << 8) >>8);
	}
	return CLI_OK;
help:
	cmm_print(DEBUG_STDOUT, "Usage: query qm-dscp-fqmap {physical interface name}\n");
	return CLI_OK;
}

int cmmQmFFRateQueryProcess(char ** keywords, int tabStart, daemon_handle_t daemon_handle)
{
        int cpt = tabStart;
        int rcvBytes = 0;
        union u_rxbuf rxbuf;
        short rc;
        fpp_qm_ff_rate_cmd_t *pFFRateCmd = ( fpp_qm_ff_rate_cmd_t *)&rxbuf;

        if(!keywords[cpt])
                goto help;

	memset(pFFRateCmd, 0, sizeof(fpp_qm_ff_rate_cmd_t));
	strncpy((char *)(&pFFRateCmd->interface[0]),(keywords[cpt]), IFNAMSIZ);
	if (keywords[++cpt]) {
		if (strcmp(keywords[cpt], "reset") == 0) {
			pFFRateCmd->clear = 1;		
		} else 	
			goto help;
	}
        rcvBytes = cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QUERY_FF_RATE,
                            pFFRateCmd, sizeof(fpp_qm_ff_rate_cmd_t) , &rxbuf);

        if (rcvBytes < sizeof( fpp_qm_ff_rate_cmd_t)  ) {
                rc = (rcvBytes < sizeof(unsigned short) ) ? 0 : rxbuf.result;
                if (rc == FPP_ERR_UNKNOWN_ACTION) {
                    cmm_print(DEBUG_STDERR, "ERROR: does not support ACTION_QUERY\n");
                } else {
                    cmm_print(DEBUG_STDERR, "ERROR: Unexpected result returned from FPP rc:%d, rcvbytes %d \n", rc, rcvBytes);
                }
                return CLI_OK;
        }
        cmm_print(DEBUG_STDOUT, "QM FF RATE (packets/sec) port %s, cir: %u, pir %u\n",
                                        pFFRateCmd->interface, pFFRateCmd->cir, pFFRateCmd->pir);
	cmm_print(DEBUG_STDOUT, "Red (dropped) packets   \t%u\n", pFFRateCmd->counterval[RED_TOTAL]);
	cmm_print(DEBUG_STDOUT, "Yellow packets          \t%u\n", pFFRateCmd->counterval[YELLOW_TOTAL]);
	cmm_print(DEBUG_STDOUT, "Green packets           \t%u\n", pFFRateCmd->counterval[GREEN_TOTAL]);
	cmm_print(DEBUG_STDOUT, "packets recolored red   \t%u\n", pFFRateCmd->counterval[RED_RECOLORED]);
	cmm_print(DEBUG_STDOUT, "packets recolored yellow\t%u\n", pFFRateCmd->counterval[YELLOW_RECOLORED]);
        return CLI_OK;
help:
	cmm_print(DEBUG_STDOUT, "Usage: query qmffrate portname\n");
        return CLI_OK;
}
#endif



#define NUM_INTERFACES GEM_PORTS


int cmmQmQueryProcess(char **keywords, int tabStart, daemon_handle_t daemon_handle) 
{
	cmm_print(DEBUG_STDOUT, "Egress Qos support disabled\n");
	
        return CLI_OK;
}

int cmmQmIngressQueryProcess(char **keywords, int tabStart, daemon_handle_t daemon_handle)
{
		cmm_print(DEBUG_STDOUT, "Ingress Qos support disabled\n");
	return CLI_OK;
}



/************************************************************
 *
 *
 *
 ************************************************************/

#define QRANGE "{0-31}"

#ifdef LS1043
#define PQ_RANGE 	"{0 - 7}"
#define WBFQ_RANGE 	"{8 - 15}"
#define MAX_PQS		8
void cmmQmSetPrintHelp(void)
{
	char buf[128];

	print_all_gemac_ports(buf, 128);
	cmm_print(DEBUG_STDOUT, 
		"	set qm expt_rate {eth} {%d - %d or 0} {%d - %d}\n"
                "\n"
                "	set qm ff_rate portname [cir {%d - %d}] [pir {%d - %d}]\n"
                "\n"
                "\n"
                "	set qm ingress queue <0-7> policer [on | off]\n"
                "	set qm ingress queue <1-7> [cir {1 - 20971250}] [pir {1 - 20971250}]\n"
                "	set qm ingress queue default [cir {1 - 20971250}] [pir {1 - 20971250}]\n"
                "	set qm ingress reset \n"
                "\n"
		,
		QM_EXPTRATE_MINVAL, QM_EXPTRATE_MAXVAL, QM_EXPTRATE_MIN_BS, QM_EXPTRATE_MAX_BS, 
		QM_FFRATE_MIN_CIR, QM_FFRATE_MAX_CIR,
		QM_FFRATE_MIN_PIR, QM_FFRATE_MAX_PIR
		);
}
#else
void cmmQmSetPrintHelp()
{
	char buf[128];

	print_all_gemac_ports(buf, 128);

	cmm_print(DEBUG_STDOUT, 
		  "Usage: set qm interface {%s}\n"
		  "                                  reset\n"
                  "\n"
		  "                                  qos\n"
                  "                                       [on | off]\n"
                  "                                       [max_txdepth {bytes}]\n"
                  "                                       [scheduler {pq|cbwfq|dwrr}] **\n"
                  "                                       [nhigh_queue {number of queues}] **\n"
                  "                                       [qweight {queue number} {weight}] **\n"
                  "                                       [qdepth {queue number} {depth}] **\n"
                  "\n"
                  "                                  shaper {0-7}\n"
                  "                                       [on | off]\n"
                  "                                       [rate {Kbps}]\n"
                  "                                       [ifg {bytes}]\n"
                  "                                       [bucket_size {bits}]\n"
                  "                                       [queue " QRANGE "] [queue " QRANGE "] ...\n"                  
                  "\n"
                  "                                  scheduler {0-7}\n"
                  "                                       [algorithm {pq | cbwfq | dwrr | rr}]\n"
                  "                                       [queue " QRANGE "] [queue " QRANGE "] ...\n"                  
                  "\n"
                  "                                  queue " QRANGE "\n"                  
                  "                                       [qos {on | off}] \n"
                  "                                       [shaper {0-7}]\n"
                  "                                       [scheduler {0-7}]\n"
                  "                                       [qweight {weight}]\n"
                  "                                       [qdepth {depth}]\n"
                  "\n"
                  "                                  rate_limiting {on|off} **\n"
                  "                                       [rate {Kbps}]\n"
                  "                                       [bucket_size {bits}]\n"
                  "                                       [queue " QRANGE "] [queue " QRANGE "] ...\n"                  
                  "\n"
		  "                                  ** Deprecated\n"

                  "\n"
                  "\n"
		    "       set qm dscp_queue\n"
		    "						[queue {0-31}] \n"
                  "						[dscp {0-63}-{0-63}]  \n"
		,
	          buf);
}
#endif

/************************************************************
 *
 *
 *
 ************************************************************/
#ifdef LS1043
int qm_get_num(char **keywords, int *pcpt, uint32_t max_val, uint32_t *val, char *errmsg)
{
	char *endptr;
	unsigned int tmp;
	int cpt;

	cpt = *pcpt;
	if(!keywords[++cpt])
		return QM_ERROR;
	/* Get number from the string */
	endptr = NULL;
	tmp = strtoul(keywords[cpt], &endptr, 0);
	if (keywords[cpt] == endptr)
		return QM_ERROR; 
	if (tmp > max_val) {
		cmm_print(DEBUG_CRIT, "%s", errmsg);
		return QM_ERROR;
	}
	*pcpt = (cpt + 1);
	*val = tmp;
	return QM_SUCCESS;
}


static int qm_exptrate_cfg(char **keywords, int cpt, daemon_handle_t daemon_handle)
{
	/* Exception packet rate limit */
	fpp_qm_expt_rate_cmd_t exptRateCmd;
	union u_rxbuf rxbuf;
	/* Use aligned local variable for qm_get_num() to avoid taking
	 * address of packed struct members (causes alignment issues on arm64) */
	uint32_t tmp_val;

	if(!keywords[++cpt])
		return QM_ERROR;

	memset(&exptRateCmd, 0, sizeof(exptRateCmd));
	if(strcasecmp(keywords[cpt], "eth") != 0 )
		return QM_ERROR;
	exptRateCmd.if_type = FPP_EXPT_TYPE_ETH;
	/* Get an integer from the string */
	if (qm_get_num(keywords, &cpt, UINT_MAX, &tmp_val,
		"invalid value for expt rate\n"))
		return QM_ERROR;
	exptRateCmd.pkts_per_sec = tmp_val;
	if ((exptRateCmd.pkts_per_sec != 0 &&
		(exptRateCmd.pkts_per_sec < QM_EXPTRATE_MINVAL || exptRateCmd.pkts_per_sec > QM_EXPTRATE_MAXVAL))) {
		cmm_print(DEBUG_CRIT, "CMD_QM_EXPT_RATE ERROR: rate must be zero (to disable) or a number between %d and %d\n",
			QM_EXPTRATE_MINVAL, QM_EXPTRATE_MAXVAL);
		return QM_ERROR;
	}
	cpt--;
	/* Get an integer from the string*/
	if (qm_get_num(keywords, &cpt, UINT_MAX, &tmp_val, "invalid value for burst_size value\n"))
		return QM_ERROR;
	exptRateCmd.burst_size = tmp_val;
	/* pps values for 64 bytes frames 10 Gbps max */
	if ((exptRateCmd.burst_size < QM_EXPTRATE_MIN_BS) || (exptRateCmd.burst_size > QM_EXPTRATE_MAX_BS))
	{
		cmm_print(DEBUG_CRIT, "CMD_QM_EXPT_RATE ERROR: invalid burst size\n");
		return QM_ERROR;
	}
	/* Send CMD_QM_EXPT_RATE command */
	if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_EXPT_RATE, &exptRateCmd, 
		sizeof(exptRateCmd), &rxbuf.rcvBuffer) == 2)
	{
		if (rxbuf.result != 0)
			showErrorMsg("CMD_QM_EXPT_RATE", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
	}
	return QM_SUCCESS;
}

static int qm_ffrate_cfg(char **keywords, int cpt, daemon_handle_t daemon_handle)
{
	union u_rxbuf rxbuf;
	/* Use aligned local variable for qm_get_num() to avoid taking
	 * address of packed struct members (causes alignment issues on arm64) */
	uint32_t tmp_val;

	/* fast forward rate limit */
	fpp_qm_ff_rate_cmd_t ffRateCmd;

	memset(&ffRateCmd, 0, sizeof(fpp_qm_ff_rate_cmd_t));

	if(!keywords[cpt + 1])
	{
		cmm_print(DEBUG_STDERR, "Error : invalid keyword. It expects portname and cir/pir configuration.\n");
		return QM_ERROR;
	}
	if (strlen(keywords[++cpt]) > (IFNAMSIZ - 1)) {
		cmm_print(DEBUG_STDERR, "Error : interface name %s limited to %d characters\n", keywords[cpt], (IFNAMSIZ - 1));
		return QM_ERROR;
	}
	if (get_port_id(keywords[cpt]) < 0) {
		cmm_print(DEBUG_STDERR, "Error : invalid interface name %s \n", keywords[cpt]);
		return QM_ERROR;
	}
	strncpy((char *)&ffRateCmd.interface[0], keywords[cpt], IFNAMSIZ);

	if((!keywords[cpt + 1]) || (strcasecmp(keywords[++cpt], "cir") != 0))  {
		cmm_print(DEBUG_STDERR, "Error : invalid keyword. It expects cir parameter and its value.\n");
		return QM_ERROR;
	}
	/* Get an integer from the string */
	if (qm_get_num(keywords, &cpt, UINT_MAX, &tmp_val,
		"invalid value for port cir rate\n"))
		return QM_ERROR;
	ffRateCmd.cir = tmp_val;
	if ((ffRateCmd.cir < QM_FFRATE_MIN_CIR) || (ffRateCmd.cir > QM_FFRATE_MAX_CIR))
	{
		cmm_print(DEBUG_CRIT, "CMD_QM_FF_RATE ERROR: invalid cir rate\n");
		return QM_ERROR;
	}

	if((!keywords[cpt]) || (strcasecmp(keywords[cpt], "pir") != 0))  {
		cmm_print(DEBUG_STDERR, "Error : invalid keyword. It expects pir parameter and its value.\n");
		return QM_ERROR;
	}
	/* Get an integer from the string*/
	if (qm_get_num(keywords, &cpt, UINT_MAX, &tmp_val,
		"invalid value for port pir rate\n"))
		return QM_ERROR;
	ffRateCmd.pir = tmp_val;
	/* pps values for 64 bytes frames 10 Gbps max */
	if ((ffRateCmd.pir < QM_FFRATE_MIN_PIR) || (ffRateCmd.pir > QM_FFRATE_MAX_PIR))
	{
		cmm_print(DEBUG_CRIT, "CMD_QM_FF_RATE ERROR: invalid pir rate\n");
		return QM_ERROR;
	}
	if (ffRateCmd.pir < ffRateCmd.cir) {
		cmm_print(DEBUG_CRIT, "CMD_QM_FF_RATE ERROR: pir < cir\n");
		return QM_ERROR;
	}
	/* Send CMD_QM_FF_RATE command */
	if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_FF_RATE, &ffRateCmd, sizeof(ffRateCmd), &rxbuf) == 2)
	{
		if (rxbuf.result != 0)
			showErrorMsg("CMD_QM_FF_RATE", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
	}
	return QM_SUCCESS;
}


int cmmQmSetProcess(char **keywords, int tabStart, daemon_handle_t daemon_handle)
{
	int cpt;
	int retval;

	cpt = tabStart;
	if (!keywords[cpt])  {
		retval = QM_ERROR;
		goto err_ret;
	} else
		retval = QM_INVALID_KEYWORD;
	while(1)
	{
		if(strcasecmp(keywords[cpt], "expt_rate") == 0) {
			retval = qm_exptrate_cfg(keywords, cpt, daemon_handle);
			break;
		}

		if(strcasecmp(keywords[cpt], "ff_rate") == 0)  {
			retval = qm_ffrate_cfg(keywords, cpt, daemon_handle);
			break;
		}


		break;
	} 
err_ret:
	switch(retval) {
		case QM_INVALID_KEYWORD:
			cmm_print(DEBUG_CRIT, "ERROR: Unknown keyword %s\n", keywords[cpt]);
		case QM_ERROR:
			cmmQmSetPrintHelp();
			break;
		default:
			return 0;
	}
	return -1;
}
#else
int cmmQmSetProcess(char ** keywords, int tabStart, daemon_handle_t daemon_handle)
{
	int cpt = tabStart;
	unsigned int tmp, tmp1;
	unsigned int cmdToSend = 0; /* bits field*/
	char * endptr;
	unsigned char first_dscp = 0, last_dscp = 0, dscp_range = 0;
	int num_dscp = 0;
	int i;
	unsigned char dscp_value[FPP_NUM_DSCP] = {0};

	
	fpp_qm_qos_enable_cmd_t enableCmd;
	fpp_qm_qos_alg_cmd_t algCmd;
	fpp_qm_nhigh_cmd_t nHighCmd;
	fpp_qm_max_qdepth_cmd_t maxQdepthCmd;
	fpp_qm_max_txdepth_cmd_t maxTxDepthCmd;
	fpp_qm_max_weight_cmd_t maxWeightCmd;
	fpp_qm_rate_limit_cmd_t rateLimitCmd;
	fpp_qm_expt_rate_cmd_t exptRateCmd;
	fpp_qm_scheduler_cfg_t schedulerCmd;
	fpp_qm_shaper_cfg_t shaperCmd;
	fpp_qm_reset_cmd_t resetCmd;
	fpp_qm_dscp_queue_mod_t dscpCmd;
	fpp_qm_queue_qos_enable_cmd_t queueenableCmd;
    
	union u_rxbuf rxbuf;

	memset(&enableCmd, 0, sizeof(enableCmd));
	memset(&algCmd, 0, sizeof(algCmd));
	memset(&nHighCmd, 0, sizeof(nHighCmd));
	memset(&maxQdepthCmd, 0, sizeof(maxQdepthCmd));
	memset(&maxTxDepthCmd, 0, sizeof(maxTxDepthCmd));
	memset(&maxWeightCmd, 0, sizeof(maxWeightCmd));
	memset(&rateLimitCmd, 0, sizeof(rateLimitCmd));
	memset(&exptRateCmd, 0, sizeof(exptRateCmd));
	memset(&schedulerCmd, 0, sizeof(schedulerCmd));
	memset(&shaperCmd, 0, sizeof(shaperCmd));
	memset(&resetCmd, 0, sizeof(resetCmd));
	memset(&dscpCmd, 0, sizeof(dscpCmd));
	memset(&queueenableCmd, 0, sizeof(queueenableCmd));


	if(!keywords[cpt])
		goto help;

	if(strcasecmp(keywords[cpt], "interface") == 0)
	{
		int port_id;

		if(!keywords[++cpt])
			goto help;

		if ((port_id = get_port_id(keywords[cpt])) >= 0)
		{
			enableCmd.interface = port_id;
			algCmd.interface = port_id;
			nHighCmd.interface = port_id;
			maxQdepthCmd.interface = port_id;
			maxTxDepthCmd.interface = port_id;
			maxWeightCmd.interface = port_id;
			rateLimitCmd.interface = port_id;
			shaperCmd.interface = port_id;
			schedulerCmd.interface = port_id;
			resetCmd.interface = port_id;
			queueenableCmd.interface = port_id;
		}
		else
			goto keyword_error;
	}
	else if(strcasecmp(keywords[cpt], "expt_rate") == 0)
	{
		if(!keywords[++cpt])
			goto help;
		memset(&exptRateCmd, 0, sizeof(exptRateCmd));

		if(strcasecmp(keywords[cpt], "eth") == 0 )
			exptRateCmd.if_type = FPP_EXPT_TYPE_ETH;
		else
			goto help;

		if(!keywords[++cpt])
			goto help;

		/*Get an integer from the string*/
		endptr = NULL;
		tmp = strtoul(keywords[cpt], &endptr, 0);
		if ((keywords[cpt] == endptr) || (tmp != 0 && (tmp < 1000 || tmp > 5000000)))
		{
			cmm_print(DEBUG_CRIT, "CMD_QM_EXPT_RATE ERROR: rate must be zero (to disable) or a number between 1000 and 5000000\n");
			goto help;
		}
		if(keywords[++cpt])
			goto help;
		//exptRateCmd.pkts_per_msec = tmp / 1000;
		exptRateCmd.pkts_per_sec = tmp;
		// Send CMD_QM_EXPT_RATE command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_EXPT_RATE, &exptRateCmd, sizeof(exptRateCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_EXPT_RATE", ERRMSG_SOURCE_FPP,rxbuf.rcvBuffer);
		}
		return 0;
	}
	else if(strcasecmp(keywords[cpt], "dscp_queue") == 0)
	{
		if(!keywords[++cpt])
			goto help;

		if(strcasecmp(keywords[cpt], "queue") == 0)
		{
			if(!keywords[++cpt])
				goto help;

			 /*Get an integer from the string*/
			endptr = NULL;
			tmp = strtoul(keywords[cpt], &endptr, 0);
			if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
			{
				cmm_print(DEBUG_STDERR, "dscp_queue ERROR: selected queue must be a number between 0 and %d\n", (FPP_NUM_QUEUES-1));
				goto help;
			}
			dscpCmd.queue = tmp;
			cmm_print(DEBUG_INFO, "dscp_queue - queue %d selected\n", dscpCmd.queue);

			if(!keywords[++cpt])
				goto help;
		}
		else
		   goto keyword_error;

		if(strcasecmp(keywords[cpt], "dscp") == 0)
		{
			/* get list of dscp values assigned to the selected queue */
			if(!keywords[++cpt])
				goto help;
			num_dscp = 0;
			first_dscp = 0;
			cmm_print(DEBUG_INFO, "dscp_queue - parsing dscp list for queue %d\n", dscpCmd.queue);
			while(keywords[cpt] && (num_dscp < FPP_NUM_DSCP))
			{
				cmm_print(DEBUG_INFO, "dscp_queue - processing arg '%s' \n", keywords[cpt]);
				if(strcasecmp(keywords[cpt], "-") == 0)
				{
					dscp_range = 1;
					cmm_print(DEBUG_INFO, "dscp_queue - dscp range detected\n");
				}
				else
				{
					endptr = NULL;
					tmp = strtoul(keywords[cpt], &endptr, 0);
					if ((keywords[cpt] == endptr) || (tmp > FPP_MAX_DSCP))
					{
						cmm_print(DEBUG_STDERR, "dscp_queue ERROR: DSCP value out of range\n");
						goto help;
					}
					else
					{
						cmm_print(DEBUG_INFO, "dscp_queue - one more dscp added\n");
						/* save low-end dscp value i.e. the first value specified*/
						if(num_dscp == 0)
							first_dscp = tmp;
						last_dscp = tmp; /* save high end dscp i.e. the last one specified*/
						dscp_value[num_dscp++] = tmp;
					}
				}
				cpt++;
			}

			/* no dscp specified means all dscp */
			if(num_dscp == 0) 
			{
				for(i = 0; i < FPP_NUM_DSCP; i++)
					dscpCmd.dscp[i] = i;
				dscpCmd.num_dscp = FPP_NUM_DSCP;
				cmm_print(DEBUG_INFO, "dscp_queue - all dscp assigned\n");
			}
			else if (dscp_range)
			{
				if(last_dscp <= first_dscp)
				{
					cmm_print(DEBUG_STDERR, "dscp_queue: wrong DSCP range\n");
					goto help;
				}
				for(i = first_dscp; i <= last_dscp; i++)
					dscpCmd.dscp[i - first_dscp] = i;
				dscpCmd.num_dscp = (last_dscp - first_dscp) + 1; 
				cmm_print(DEBUG_INFO, "dscp_queue - dscp range %d to %d\n", first_dscp, last_dscp);
			}
			else
			{
				cmm_print(DEBUG_INFO, "dscp_queue - dscp non-ordered list\n");
				dscpCmd.num_dscp = num_dscp;
				for(i = 0; i < dscpCmd.num_dscp; i++)
					dscpCmd.dscp[i] = dscp_value[i];
			}
			cmm_print(DEBUG_INFO, "dscp_queue - %d dscp assigned ->\n", dscpCmd.num_dscp);
			for(i = 0; i < dscpCmd.num_dscp; i++)
				cmm_print(DEBUG_INFO, "%d ", dscpCmd.dscp[i]);
			cmm_print(DEBUG_INFO, "\n");

			if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_DSCP_MAP, &dscpCmd, sizeof(fpp_qm_dscp_queue_mod_t), rxbuf.rcvBuffer) == 2)
			{
				if (rxbuf.result != 0)
					showErrorMsg("CMD_QM_DSCP_MAP", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
				return (rxbuf.result);
			}
		}
		else {
			cmm_print(DEBUG_STDERR, "ERROR: Unknown keyword %s\n", keywords[cpt]);
			goto help;
		}

		return 0;
	}
	else
		goto keyword_error;

	if(!keywords[++cpt])
		goto help;
	
	if(strcasecmp(keywords[cpt], "qos") == 0)
	{		
		if(!keywords[++cpt])
			goto help;
		
		while (keywords[cpt] != NULL)
		{
			if(strcasecmp(keywords[cpt], "on") == 0)
			{
				cmdToSend |= CMD_BIT(FPP_CMD_QM_QOSENABLE);
				enableCmd.enable = 1;
			}
			else if(strcasecmp(keywords[cpt], "off") == 0)
			{
				cmdToSend |= CMD_BIT(FPP_CMD_QM_QOSENABLE);
				enableCmd.enable = 0;
			}
			else if(strcasecmp(keywords[cpt], "scheduler") == 0)
			{
				if(!keywords[++cpt])
					goto help;


				cmdToSend |= CMD_BIT(FPP_CMD_QM_QOSALG);

				if(strcasecmp(keywords[cpt], "pq") == 0)
				{
					algCmd.scheduler = 0;
				}
				else if (strcasecmp(keywords[cpt], "cbwfq") == 0)
				{
					algCmd.scheduler = 1;
				}
				else if (strcasecmp(keywords[cpt], "dwrr") == 0)
				{
					algCmd.scheduler = 2;
				}
				else
					goto keyword_error;
			}
			else if(strcasecmp(keywords[cpt], "nhigh_queue") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_CRIT, "qos ERROR: nhigh_queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
					goto help;
				}

				nHighCmd.number_high_queues = tmp;
				
				cmdToSend |= CMD_BIT(FPP_CMD_QM_NHIGH);
			}
			else if(strcasecmp(keywords[cpt], "max_txdepth") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || tmp < 1 || (tmp > USHRT_MAX))
				{
					cmm_print(DEBUG_CRIT, "qos ERROR: max_txdepth must be a number between 1 and %d\n", USHRT_MAX);
					goto help;
				}
		
				maxTxDepthCmd.max_bytes = tmp;

				cmdToSend |= CMD_BIT(FPP_CMD_QM_MAX_TXDEPTH);
			}
			else if(strcasecmp(keywords[cpt], "qweight") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_STDERR, "qos ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1) );
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;
				
				/*Get an integer from the string*/
				endptr = NULL;
				tmp1 = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || tmp1 < 1 || (tmp1 > USHRT_MAX))
				{
					cmm_print(DEBUG_STDERR, "qos ERROR: weight must be a number between 1 and %d\n", USHRT_MAX);
					goto help;
				}
				
				maxWeightCmd.qxweight[tmp] = tmp1;
				cmdToSend |= CMD_BIT(FPP_CMD_QM_MAX_WEIGHT);
			}

			else if(strcasecmp(keywords[cpt], "qdepth") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_STDERR, "qos ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;
				
				/*Get an integer from the string*/
				endptr = NULL;
				tmp1 = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || tmp1 < 1 || (tmp1 > USHRT_MAX))
				{
					cmm_print(DEBUG_STDERR, "qos ERROR: depth must be a number between 1 and %d\n", USHRT_MAX);
					goto help;
				}
				
				maxQdepthCmd.qtxdepth[tmp] = tmp1;
				cmdToSend |= CMD_BIT(FPP_CMD_QM_MAX_QDEPTH);
			}
			else
				goto keyword_error;

			cpt++;
		}
	}
	else if(strcasecmp(keywords[cpt], "rate_limiting") == 0)
	{
		if(!keywords[++cpt])
			goto help;
		
		if(strcasecmp(keywords[cpt], "on") == 0)
		{
			cmdToSend |= CMD_BIT(FPP_CMD_QM_RATE_LIMIT);
			rateLimitCmd.enable = 1;
	
			cpt++;
			while (keywords[cpt] != NULL)
			{
				if(strcasecmp(keywords[cpt], "queue") == 0)
				{
					if(!keywords[++cpt])
						goto help;

					/*Get an integer from the string*/
					endptr = NULL;
					tmp = strtoul(keywords[cpt], &endptr, 0);
					if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
					{
						cmm_print(DEBUG_CRIT, "rate_limiting ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
						goto help;
					}

					rateLimitCmd.queues |= (1 << tmp);
				}
				else if(strcasecmp(keywords[cpt], "rate") == 0)
				{
					if(!keywords[++cpt])
						goto help;

					/*Get an integer from the string*/
					endptr = NULL;
					tmp = strtoul(keywords[cpt], &endptr, 0);
					if ((keywords[cpt] == endptr) || (tmp < 8) || (tmp > UINT_MAX))
					{
						cmm_print(DEBUG_CRIT, "rate_limiting ERROR: rate must be a number between 8 and %d (Kbps)\n", (unsigned int)UINT_MAX);
						goto help;
					}

					rateLimitCmd.rate = tmp;
				}
				else if(strcasecmp(keywords[cpt], "bucket_size") == 0)
				{
					if(!keywords[++cpt])
						goto help;

					/*Get an integer from the string*/
					endptr = NULL;
					tmp = strtoul(keywords[cpt], &endptr, 0);
					if ((keywords[cpt] == endptr) || (tmp < 8) || (tmp > UINT_MAX))
					{
						cmm_print(DEBUG_CRIT, "rate_limiting ERROR: bucket_size must be a number between 8 and %d\n", (unsigned int)UINT_MAX);
						goto help;
					}

					rateLimitCmd.bucket_size = tmp;
				}
				else
					goto keyword_error;
			
				cpt++;
			}

			/*Dependencies check*/
			if (rateLimitCmd.queues == 0)
			{
				cmm_print(DEBUG_CRIT, "Rate Limiting ERROR: At least one queue must be specified\n");
				goto help;
			}
			
			if(rateLimitCmd.rate == 0)
			{
				cmm_print(DEBUG_CRIT, "Rate Limiting ERROR: The bandwidth have to be specified\n");
				goto help;
			}
		}
		else if(strcasecmp(keywords[cpt], "off") == 0)
		{
			cmdToSend |= CMD_BIT(FPP_CMD_QM_RATE_LIMIT);
			rateLimitCmd.enable = 0;
		}
		else
			goto keyword_error;
		
	}
	else if(strcasecmp(keywords[cpt], "shaper") == 0)
	{
		if(!keywords[++cpt])
			goto help;

		/*Get an integer from the string*/
		{
			endptr = NULL;
			tmp = strtoul(keywords[cpt], &endptr, 0);
			if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_SHAPERS))
			{
				cmm_print(DEBUG_CRIT, "shaper ERROR: shaper number must be between 0 and %d\n", FPP_NUM_SHAPERS);
				goto help;
			}
		}

		shaperCmd.shaper = tmp;
		
		if(!keywords[++cpt])
			goto help;

		cmdToSend |= CMD_BIT(FPP_CMD_QM_SHAPER_CFG);

		while (keywords[cpt] != NULL)
		{
			if(strcasecmp(keywords[cpt], "on") == 0)
			{
				shaperCmd.enable = 1;
			}
			else if(strcasecmp(keywords[cpt], "off") == 0)
			{
				shaperCmd.enable = 2;
			}
			else if(strcasecmp(keywords[cpt], "queue") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_CRIT, "shaper ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
					goto help;
				}

				shaperCmd.queues |= (1 << tmp);
			}
			else if(strcasecmp(keywords[cpt], "ifg") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp > 255))
				{
					cmm_print(DEBUG_CRIT, "shaper ERROR: ifg must be a number between 0 and 255\n");
					goto help;
				}

				shaperCmd.ifg = tmp;
				shaperCmd.ifg_change_flag = 1;
			}
			else if(strcasecmp(keywords[cpt], "rate") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/* Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
                                if ((keywords[cpt] == endptr) || (tmp < 8) || (tmp > UINT_MAX))
                                {
                                        cmm_print(DEBUG_CRIT, "shaper ERROR: rate must be a number between 8 and %d (Kbps)\n", (unsigned int)UINT_MAX);
                                        goto help;
                                }


				shaperCmd.rate = tmp;
			}
			else if(strcasecmp(keywords[cpt], "bucket_size") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp < 8) || (tmp > UINT_MAX))
				{
					cmm_print(DEBUG_CRIT, "shaper ERROR: bucket_size must be a number between 8 and %d\n", (unsigned int)UINT_MAX);
					goto help;
				}

				shaperCmd.bucket_size = tmp;
			}
			else
				goto keyword_error;
		
			cpt++;
		}
	}
	else if(strcasecmp(keywords[cpt], "scheduler") == 0)
	{
		if(!keywords[++cpt])
			goto help;

		/*Get an integer from the string*/
		endptr = NULL;
		tmp = strtoul(keywords[cpt], &endptr, 0);
		if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_SCHEDULERS))
		{
			cmm_print(DEBUG_CRIT, "scheduler ERROR: scheduler number must be between 0 and 3\n");
			goto help;
		}

		schedulerCmd.scheduler = tmp;
		
		if(!keywords[++cpt])
			goto help;

		cmdToSend |= CMD_BIT(FPP_CMD_QM_SCHED_CFG);
	
		while (keywords[cpt] != NULL)
		{
			if(strcasecmp(keywords[cpt], "queue") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_CRIT, "scheduler ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
					goto help;
				}

				schedulerCmd.queues |= (1 << tmp);
			}
			else if(strcasecmp(keywords[cpt], "algorithm") == 0)
			{
				if(!keywords[++cpt])
					goto help;

				if(strcasecmp(keywords[cpt], "pq") == 0)
				{
					schedulerCmd.algo = 0;
					schedulerCmd.algo_change_flag = 1;
				}
				else if (strcasecmp(keywords[cpt], "cbwfq") == 0)
				{
					schedulerCmd.algo = 1;
					schedulerCmd.algo_change_flag = 1;
				}
				else if (strcasecmp(keywords[cpt], "dwrr") == 0)
				{
					schedulerCmd.algo = 2;
					schedulerCmd.algo_change_flag = 1;
				}
				else if (strcasecmp(keywords[cpt], "rr") == 0)
				{
					schedulerCmd.algo = 3;
					schedulerCmd.algo_change_flag = 1;
				}
				else
					goto keyword_error;
			}			
			else
				goto keyword_error;
		
			cpt++;
		}

	}

	else if(strcasecmp(keywords[cpt], "queue") == 0)
	{
		unsigned int qmask=0; /* Bit mask of single or set of queues that are programmed */

		if(!keywords[++cpt])
			goto help;

		/*Get an integer from the string*/
		endptr = NULL;
		tmp = strtoul(keywords[cpt], &endptr, 0);
		if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
		{
			cmm_print(DEBUG_CRIT, "queue ERROR: queue must be a number between 0 and %d\n", (FPP_NUM_QUEUES-1));
			goto help;
		}
		qmask |= (1<<tmp);

		if(!keywords[++cpt])
			goto help;

		while (keywords[cpt] != NULL)
		{
			if(strcasecmp(keywords[cpt], "queue") == 0)
			{
			       if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_QUEUES))
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: queue must be a number between 0 and %d \n", (FPP_NUM_QUEUES -1));
					goto help;
				}

				qmask |= (1<<tmp);
			}
			else if(strcasecmp(keywords[cpt], "qos") == 0)
			{
				if (qmask ==0)
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: One or more queues need to be specified \n");
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;
		
				
				if(strcasecmp(keywords[cpt], "on") == 0)
					queueenableCmd.enable_flag = 1;
				else if(strcasecmp(keywords[cpt], "off") == 0)
					queueenableCmd.enable_flag = 0;
				
				queueenableCmd.queue_qosenable_mask = qmask;
				cmdToSend |= CMD_BIT(FPP_CMD_QM_QUEUE_QOSENABLE);
				
			}
			else if(strcasecmp(keywords[cpt], "shaper") == 0)
			{
				if (qmask ==0)
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: One or more queues need to be specified \n");
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_SHAPERS))
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: shaper number must be between 0 and 4\n");
					goto help;
				}

				shaperCmd.shaper = tmp;
				shaperCmd.queues = qmask;
				cmdToSend |= CMD_BIT(FPP_CMD_QM_SHAPER_CFG);
			}
			else if(strcasecmp(keywords[cpt], "scheduler") == 0)
			{
				if (qmask ==0)
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: One or more queues need to be specified \n");
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || (tmp >= FPP_NUM_SCHEDULERS))
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: scheduler number must be between 0 and 3\n");
					goto help;
				}

				schedulerCmd.scheduler = tmp;
				schedulerCmd.queues = qmask;
				cmdToSend |= CMD_BIT(FPP_CMD_QM_SCHED_CFG);
			}
			else if(strcasecmp(keywords[cpt], "qweight") == 0)
			{
				if (qmask ==0)
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: One or more queues need to be specified \n");
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp1 = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || tmp1 < 1 || (tmp1 > USHRT_MAX))
				{
					cmm_print(DEBUG_STDERR, "queue ERROR: weight must be a number between 1 and %d\n", USHRT_MAX);
					goto help;
				}

				for(i=0; i < FPP_NUM_QUEUES; i++) {
					if(qmask & (1 << i))
						maxWeightCmd.qxweight[i] = tmp1;
				}
				cmdToSend |= CMD_BIT(FPP_CMD_QM_MAX_WEIGHT);
			}

			else if(strcasecmp(keywords[cpt], "qdepth") == 0)
			{
				if (qmask ==0)
				{
					cmm_print(DEBUG_CRIT, "queue ERROR: One or more queues need to be specified \n");
					goto help;
				}
				
				if(!keywords[++cpt])
					goto help;

				/*Get an integer from the string*/
				endptr = NULL;
				tmp1 = strtoul(keywords[cpt], &endptr, 0);
				if ((keywords[cpt] == endptr) || tmp1 < 1 || (tmp1 > USHRT_MAX))
				{
					cmm_print(DEBUG_STDERR, "queue ERROR: depth must be a number between 1 and %d\n", USHRT_MAX);
					goto help;
				}

				for(i=0; i < FPP_NUM_QUEUES; i++) {
					if(qmask & (1 << i))
						maxQdepthCmd.qtxdepth[i] = tmp1;
				}
				cmdToSend |= CMD_BIT(FPP_CMD_QM_MAX_QDEPTH);
			}
			else
				goto keyword_error;
		
			cpt++;
		}

	}

	else if(strcasecmp(keywords[cpt], "reset") == 0)
	{
		if(keywords[++cpt])
			goto help;

		cmdToSend |= CMD_BIT(FPP_CMD_QM_RESET);	
	}
	else
		goto keyword_error;

	/*
	 * Parsing have been performed
	 * Now send the right commands
	 */

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_RESET))
	{
		// Send CMD_QM_RATE_LIMIT command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_RESET, &resetCmd, sizeof(resetCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_RESET", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}
	
	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_QOSENABLE))
	{
		// Send CMD_QM_QOSENABLE command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QOSENABLE, & enableCmd, sizeof(enableCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_QOSENABLE", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_QUEUE_QOSENABLE))
	{
		// Send FPP_CMD_QM_QUEUE_QOSENABLE command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QUEUE_QOSENABLE, &queueenableCmd, sizeof(queueenableCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_QUEUE_QOSENABLE", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}
	
	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_QOSALG))
	{
		// Send CMD_QM_QOSALG command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_QOSALG, & algCmd, sizeof(algCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_QOSALG", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_NHIGH))
	{
		// Send CMD_QM_NHIGH command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_NHIGH, & nHighCmd, sizeof(nHighCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_NHIGH", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_MAX_TXDEPTH))
	{
		// Send CMD_QM_MAX_TXDEPTH command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_MAX_TXDEPTH, &maxTxDepthCmd, sizeof(maxTxDepthCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_MAX_TXDEPTH", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_MAX_QDEPTH))
	{
		// Send CMD_QM_MAX_QDEPTH command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_MAX_QDEPTH, & maxQdepthCmd , sizeof(maxQdepthCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_MAX_QDEPTH", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_MAX_WEIGHT))
	{
		// Send CMD_QM_MAX_WEIGHT command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_MAX_WEIGHT, &maxWeightCmd , sizeof(maxWeightCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_MAX_WEIGHT", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_RATE_LIMIT))
	{
		// Send CMD_QM_RATE_LIMIT command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_RATE_LIMIT, &rateLimitCmd, sizeof(rateLimitCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_RATE_LIMIT", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_SHAPER_CFG))
	{
		// Send CMD_QM_RATE_LIMIT command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_SHAPER_CFG, &shaperCmd, sizeof(shaperCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_SHAPER_CFG", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	if(TEST_CMD_BIT(cmdToSend, FPP_CMD_QM_SCHED_CFG))
	{
		// Send CMD_QM_RATE_LIMIT command
		if(cmmSendToDaemon(daemon_handle, FPP_CMD_QM_SCHED_CFG, &schedulerCmd, sizeof(schedulerCmd), &rxbuf.rcvBuffer) == 2)
		{
			if (rxbuf.result != 0)
				showErrorMsg("CMD_QM_SCHED_CFG", ERRMSG_SOURCE_FPP, rxbuf.rcvBuffer);
		}
	}

	return 0;

keyword_error:
	cmm_print(DEBUG_CRIT, "ERROR: Unknown keyword %s\n", keywords[cpt]);

help:
	cmmQmSetPrintHelp();
	return -1;
}
#endif


void cmmQmResetQ2Prio(fpp_qm_reset_cmd_t *cmdp, int cmdlen)
{
	char fname[128];
	FILE *fp;

	if (cmdlen != sizeof(fpp_qm_reset_cmd_t))
	{
		cmm_print(DEBUG_ERROR, "%s: Wrong length for cmd, expected %zu, got %d\n", __func__,
						sizeof(fpp_qm_scheduler_cfg_t), cmdlen);
		return;
	}

	/* cmdp->interface already contains the interface name as a string */
	snprintf(fname, sizeof(fname), "/sys/class/net/%s/q2prio", (const char *)cmdp->interface);
	fp = fopen(fname, "w");
	if (!fp)
	{
		cmm_print(DEBUG_WARNING, "%s: Cannot open %s\n", __func__, fname);
		return;
	}
	fprintf(fp, "reset\n");
	fclose(fp);
}


void cmmQmUpdateQ2Prio(fpp_qm_scheduler_cfg_t *cmdp, int cmdlen)
{
	u_int16_t interface;
        u_int16_t scheduler;
        u_int32_t queues;
	char fname[128], ifname[IFNAMSIZ];
	FILE *fp;

	if (cmdlen != sizeof(fpp_qm_scheduler_cfg_t))
	{
		cmm_print(DEBUG_ERROR, "%s: Wrong length for cmd, expected %zu, got %d\n", __func__,
						sizeof(fpp_qm_scheduler_cfg_t), cmdlen);
		return;
	}

	interface = cmdp->interface;
	scheduler = cmdp->scheduler;
	queues = cmdp->queues;

	snprintf(fname, 128, "/sys/class/net/%s/q2prio", get_port_name(interface, ifname, IFNAMSIZ));
	fp = fopen(fname, "w");
	if (!fp)
	{
		cmm_print(DEBUG_WARNING, "%s: Cannot open %s\n", __func__, fname);
		return;
	}
	fprintf(fp, "%d 0x%x\n", scheduler, queues);
	fclose(fp);
}

