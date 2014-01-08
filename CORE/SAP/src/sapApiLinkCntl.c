/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================

                      s a p A p i L i n k C n t l . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN SAP modules
  Link Control functions.
  
  The functions externalized by this module are to be called ONLY by other 
  WLAN modules (HDD) 

  DEPENDENCIES: 

  Are listed for each API below. 
  
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header: /cygdrive/c/Dropbox/M7201JSDCAAPAD52240B/WM/platform/msm7200/Src/Drivers/SD/ClientDrivers/WLAN/QCT_SAP_PAL/CORE/SAP/src/sapApiLinkCntl.c,v 1.7 2008/12/18 19:44:11 jzmuda Exp jzmuda $$DateTime$$Author: jzmuda $


  when        who     what, where, why
----------    ---    --------------------------------------------------------
2010-03-15              Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_trace.h"
// Pick up the CSR callback definition
#include "csrApi.h"
#include "sme_Api.h"
// SAP Internal API header file
#include "sapInternal.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/*==========================================================================
  FUNCTION    WLANSAP_ScanCallback()

  DESCRIPTION 
    Callback for Scan (scan results) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    tHalHandle  : tHalHandle passed in with the scan request
    *pContext   : The second context pass in for the caller (sapContext)
    scanID      : scanID got after the scan
    status      : Status of scan -success, failure or abort
   
  RETURN VALUE
    The eHalStatus code associated with performing the operation  

    eHAL_STATUS_SUCCESS: Success
  
  SIDE EFFECTS 
============================================================================*/
eHalStatus
WLANSAP_ScanCallback
(
  tHalHandle halHandle, 
  void *pContext,           /* Opaque SAP handle */
  v_U32_t scanID, 
  eCsrScanStatus scanStatus
)
{
    tScanResultHandle pResult = NULL;
    eHalStatus scanGetResultStatus = eHAL_STATUS_FAILURE;
    ptSapContext psapContext = (ptSapContext)pContext;
    tWLAN_SAPEvent sapEvent; /* State machine event */
    v_U8_t operChannel = 0;
    VOS_STATUS sapstatus;
#ifdef SOFTAP_CHANNEL_RANGE
    v_U32_t operatingBand;
    v_U32_t event;
#endif

    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, before switch on scanStatus = %d", __func__, scanStatus);

    switch (scanStatus) 
    {
        case eCSR_SCAN_SUCCESS:
            // sapScanCompleteCallback with eCSR_SCAN_SUCCESS
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR scanStatus = %s (%d)", __func__, "eCSR_SCAN_SUCCESS", scanStatus);

            // Get scan results, Run channel selection algorithm, select channel and keep in pSapContext->Channel
            scanGetResultStatus = sme_ScanGetResult(halHandle, 0, NULL, &pResult);

            if ((NULL == pResult) || (scanGetResultStatus != eHAL_STATUS_SUCCESS))
            {
                // No scan results
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, sme_ScanGetResult = NULL", __func__);
                break;
            }

            operChannel = sapSelectChannel(halHandle, psapContext, pResult);

            sme_ScanResultPurge(halHandle, pResult);
            event = eSAP_MAC_SCAN_COMPLETE;
            break;

        default:
            event = eSAP_CHANNEL_SELECTION_FAILED;
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR scanStatus = %s (%d)", __func__, "eCSR_SCAN_ABORT/FAILURE", scanStatus);
    }
    
    if (operChannel == SAP_CHANNEL_NOT_SELECTED)
#ifdef SOFTAP_CHANNEL_RANGE
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: No suitable channel selected",
             __func__);

        if ( eCSR_BAND_ALL ==  psapContext->scanBandPreference ||
                     psapContext->allBandScanned == eSAP_TRUE)
        {
             if(psapContext->channelList != NULL)
             {
                 psapContext->channel = psapContext->channelList[0];
             }
             else
             {
                 /* if the channel list is empty then there is no valid channel in
                    the selected sub-band so select default channel in the
                    BAND(2.4GHz/5GHZ) */
                 ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND, &operatingBand);
                 if(RF_SUBBAND_2_4_GHZ == operatingBand )
                     psapContext->channel = SAP_DEFAULT_CHANNEL;
                 else
                     psapContext->channel = SAP_DEFAULT_5GHZ_CHANNEL;
             }
        }
        else
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Has scan band preference",
             __func__);
            if (eCSR_BAND_24 == psapContext->currentPreferredBand)
                psapContext->currentPreferredBand = eCSR_BAND_5G;
            else
                psapContext->currentPreferredBand = eCSR_BAND_24;

            psapContext->allBandScanned = eSAP_TRUE;
            //go back to DISCONNECT state, scan next band
            psapContext->sapsMachine = eSAP_DISCONNECTED;
            event = eSAP_CHANNEL_SELECTION_FAILED;
         }
    }
#else
        psapContext->channel = SAP_DEFAULT_CHANNEL;
#endif
    else
    {
        psapContext->channel = operChannel;
    }

    sme_SelectCBMode(halHandle,
          sapConvertSapPhyModeToCsrPhyMode(psapContext->csrRoamProfile.phyMode),
          psapContext->channel);
#ifdef SOFTAP_CHANNEL_RANGE
    if(psapContext->channelList != NULL)
    {
        /* Always free up the memory for channel selection whatever
         * the result */
        vos_mem_free(psapContext->channelList);
        psapContext->channelList = NULL;
    }
#endif    

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Channel selected = %d", __func__, psapContext->channel);

    /* Fill in the event structure */
    sapEvent.event = event;
    sapEvent.params = 0;        // pCsrRoamInfo;
    sapEvent.u1 = scanStatus;   // roamstatus
    sapEvent.u2 = 0;            // roamResult

    /* Handle event */ 
    sapstatus = sapFsm(psapContext, &sapEvent);

    return sapstatus;
}// WLANSAP_ScanCallback

/*==========================================================================
  FUNCTION    WLANSAP_RoamCallback()

  DESCRIPTION 
    Callback for Roam (connection status) Events  

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
      pContext      : pContext passed in with the roam request
      pCsrRoamInfo  : Pointer to a tCsrRoamInfo, see definition of eRoamCmdStatus and
      eRoamCmdResult: For detail valid members. It may be NULL
      roamId        : To identify the callback related roam request. 0 means unsolicited
      roamStatus    : Flag indicating the status of the callback
      roamResult    : Result
   
  RETURN VALUE
    The eHalStatus code associated with performing the operation  

    eHAL_STATUS_SUCCESS: Success
  
  SIDE EFFECTS 
============================================================================*/
eHalStatus
WLANSAP_RoamCallback
(
    void *pContext,           /* Opaque SAP handle */ 
    tCsrRoamInfo *pCsrRoamInfo,
    v_U32_t roamId, 
    eRoamCmdStatus roamStatus, 
    eCsrRoamResult roamResult
)
{
    /* sapContext value */    
    ptSapContext sapContext = (ptSapContext) pContext; 
    tWLAN_SAPEvent sapEvent; /* State machine event */
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    eHalStatus halStatus = eHAL_STATUS_SUCCESS;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, before switch on roamStatus = %d\n", __func__, roamStatus);
    switch(roamStatus)
    {
        case eCSR_ROAM_SESSION_OPENED:
        {
            /* tHalHandle */
            tHalHandle hHal = VOS_GET_HAL_CB(sapContext->pvosGCtx);

            if (NULL == hHal)
            {
               VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                          "In %s invalid hHal", __func__);
               halStatus = eHAL_STATUS_FAILED_ALLOC;
            }
            else
            {
               VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          "In %s calling sme_RoamConnect with eCSR_BSS_TYPE_INFRA_AP", __func__);
               sapContext->isSapSessionOpen = eSAP_TRUE;
               halStatus = sme_RoamConnect(hHal, sapContext->sessionId,
                                           &sapContext->csrRoamProfile,
                                           &sapContext->csrRoamId);
            }
            break;
        }

        case eCSR_ROAM_INFRA_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                      __func__, "eCSR_ROAM_INFRA_IND", roamStatus);
            if(roamResult == eCSR_ROAM_RESULT_INFRA_START_FAILED)
            {
                /* Fill in the event structure */ 
                sapEvent.event = eSAP_MAC_START_FAILS; 
                sapEvent.params = pCsrRoamInfo;
                sapEvent.u1 = roamStatus;
                sapEvent.u2 = roamResult; 
                
                /* Handle event */ 
                vosStatus = sapFsm(sapContext, &sapEvent);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                    halStatus = eHAL_STATUS_FAILURE;
                }
            }
            break;

        case eCSR_ROAM_LOSTLINK:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                       __func__, "eCSR_ROAM_LOSTLINK", roamStatus);
            break;

        case eCSR_ROAM_MIC_ERROR_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                __func__, "eCSR_ROAM_MIC_ERROR_IND", roamStatus);
            break;

        case eCSR_ROAM_SET_KEY_COMPLETE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                __func__, "eCSR_ROAM_SET_KEY_COMPLETE", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_SET_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_REMOVE_KEY_COMPLETE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                        __func__, "eCSR_ROAM_REMOVE_KEY_COMPLETE", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_DEL_KEY_EVENT, (v_PVOID_t)eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_ASSOCIATION_COMPLETION:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                       __func__, "eCSR_ROAM_ASSOCIATION_COMPLETION", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_FAILURE )
            {
                /* Format the SET KEY complete information pass to HDD... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_REASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_FAILURE);
            }
            break;

        case eCSR_ROAM_DISASSOCIATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                       __func__, "eCSR_ROAM_DISASSOCIATED", roamStatus);
            if (roamResult == eCSR_ROAM_RESULT_MIC_FAILURE)
            {
                /* Format the MIC failure event to return... */
                sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) eSAP_STATUS_FAILURE);
            }
            break;
                        
        case eCSR_ROAM_WPS_PBC_PROBE_REQ_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamStatus = %s (%d)\n",
                       __func__, "eCSR_ROAM_WPS_PBC_PROBE_REQ_IND", roamStatus);
            break;        

        case eCSR_ROAM_INDICATE_MGMT_FRAME:
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                              eSAP_INDICATE_MGMT_FRAME, 
                              (v_PVOID_t) eSAP_STATUS_SUCCESS);
            break;
        case eCSR_ROAM_REMAIN_CHAN_READY:
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                              eSAP_REMAIN_CHAN_READY, 
                              (v_PVOID_t) eSAP_STATUS_SUCCESS);
            break;
        case eCSR_ROAM_SEND_ACTION_CNF:
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_SEND_ACTION_CNF,
                            (v_PVOID_t)((eSapStatus)((roamResult == eCSR_ROAM_RESULT_NONE)
                            ? eSAP_STATUS_SUCCESS : eSAP_STATUS_FAILURE)));
            break;

       case eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS:
            sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_DISCONNECT_ALL_P2P_CLIENT, 
                            (v_PVOID_t) eSAP_STATUS_SUCCESS );
            break;
            
       case eCSR_ROAM_SEND_P2P_STOP_BSS:
           VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Received stopbss", __func__);
           sapSignalHDDevent(sapContext, pCsrRoamInfo, 
                            eSAP_MAC_TRIG_STOP_BSS_EVENT, 
                            (v_PVOID_t) eSAP_STATUS_SUCCESS );
        break;

       case eCSR_ROAM_DFS_RADAR_IND:
           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                     "In %s, Received Radar Indication", __func__);
           sapContext->SapDfsInfo.target_channel =
                     sapIndicateRadar(sapContext, &pCsrRoamInfo->dfs_event);
           break;

       case eCSR_ROAM_DFS_CHAN_SW_NOTIFY:
           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                     "In %s, Received Chan Sw Update Notification", __func__);
           break;

       case eCSR_ROAM_SET_CHANNEL_RSP:
           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                     "In %s, Received set channel response", __func__);
           break;

       default:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, CSR roamStatus not handled roamStatus = %s (%d)\n",
                       __func__, get_eRoamCmdStatus_str(roamStatus), roamStatus);
            break;

    }

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, before switch on roamResult = %d\n",
               __func__, roamResult);

    switch (roamResult)
    {
        case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND", roamResult);
            sapContext->nStaWPARSnReqIeLength = pCsrRoamInfo->rsnIELen;
             
            if(sapContext->nStaWPARSnReqIeLength)
                vos_mem_copy( sapContext->pStaWpaRsnReqIE,
                              pCsrRoamInfo->prsnIE, sapContext->nStaWPARSnReqIeLength);

            sapContext->nStaAddIeLength = pCsrRoamInfo->addIELen;
             
            if(sapContext->nStaAddIeLength)
                vos_mem_copy( sapContext->pStaAddIE,
                        pCsrRoamInfo->paddIE, sapContext->nStaAddIeLength);

            sapContext->SapQosCfg.WmmIsEnabled = pCsrRoamInfo->wmmEnabledSta;
            // MAC filtering
            vosStatus = sapIsPeerMacAllowed(sapContext, (v_U8_t *)pCsrRoamInfo->peerMac);
            
            if ( VOS_STATUS_SUCCESS == vosStatus )
            {
                vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_ASSOC_IND, (v_PVOID_t)eSAP_STATUS_SUCCESS);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                   VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, 
                      "In %s, CSR roamResult = (%d) MAC"
                      "(%02X-%02X-%02X-%02X-%02X-%02X) fail",
                      __func__, roamResult, pCsrRoamInfo->peerMac[0],
                      pCsrRoamInfo->peerMac[1], pCsrRoamInfo->peerMac[2],
                      pCsrRoamInfo->peerMac[3], pCsrRoamInfo->peerMac[4],
                      pCsrRoamInfo->peerMac[5]);
                    halStatus = eHAL_STATUS_FAILURE;
                }
            }
            else
            {
                VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN, 
                   "In %s, CSR roamResult = (%d) MAC"
                   "(%02X-%02X-%02X-%02X-%02X-%02X) not allowed",
                   __func__, roamResult, pCsrRoamInfo->peerMac[0],
                   pCsrRoamInfo->peerMac[1], pCsrRoamInfo->peerMac[2],
                   pCsrRoamInfo->peerMac[3], pCsrRoamInfo->peerMac[4], pCsrRoamInfo->peerMac[5]);
                halStatus = eHAL_STATUS_FAILURE;
            } 

            break;

        case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                        __func__, "eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF", roamResult);

            sapContext->nStaWPARSnReqIeLength = pCsrRoamInfo->rsnIELen;
            if (sapContext->nStaWPARSnReqIeLength)
                vos_mem_copy( sapContext->pStaWpaRsnReqIE,
                              pCsrRoamInfo->prsnIE, sapContext->nStaWPARSnReqIeLength);

            sapContext->nStaAddIeLength = pCsrRoamInfo->addIELen;
            if(sapContext->nStaAddIeLength)
                vos_mem_copy( sapContext->pStaAddIE,
                    pCsrRoamInfo->paddIE, sapContext->nStaAddIeLength);

            sapContext->SapQosCfg.WmmIsEnabled = pCsrRoamInfo->wmmEnabledSta;
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_ASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_DISASSOC_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                        __func__, "eCSR_ROAM_RESULT_DISASSOC_IND", roamResult);
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_DEAUTH_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_DEAUTH_IND", roamResult);
            /* Fill in the event structure */
            //TODO: we will use the same event inorder to inform HDD to disassociate the station
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_GROUP:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                        __func__, "eCSR_ROAM_RESULT_MIC_ERROR_GROUP", roamResult);
            /* Fill in the event structure */
            //TODO: support for group key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_MIC_ERROR_UNICAST: 
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_MIC_ERROR_UNICAST", roamResult);
            /* Fill in the event structure */
            //TODO: support for unicast key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_MIC_FAILURE_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_AUTHENTICATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_AUTHENTICATED", roamResult);
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_SET_KEY_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_ASSOCIATED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_ASSOCIATED", roamResult);
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_REASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            break;

        case eCSR_ROAM_RESULT_INFRA_STARTED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_INFRA_STARTED", roamResult);

            /* In the current implementation, hostapd is not aware that
             * drive will support DFS. Hence, driver should inform
             * eSAP_MAC_START_BSS_SUCCESS to upper layers and then perform
             * CAC underneath
             */
            sapEvent.event = eSAP_MAC_START_BSS_SUCCESS;
            sapEvent.params = pCsrRoamInfo;
            sapEvent.u1 = roamStatus;
            sapEvent.u2 = roamResult;

            /*
             * If the Channel on which BSS started is a DFS channel
             * then initialize the NOL list in the sapDfsInfo to
             * a manitain a list of radar found DFS channels and
             * to make sure that SAP does not use these channels
             * for the next 30 mins.
             */
            if (vos_nv_getChannelEnabledState(sapContext->channel) ==
                                                            NV_CHANNEL_DFS)
            {
                sapInitDfsChannelNolList(sapContext);
            }

            vosStatus = sapFsm(sapContext, &sapEvent);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_INFRA_STOPPED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_INFRA_STOPPED", roamResult);
            /* Fill in the event structure */ 
            sapEvent.event = eSAP_MAC_READY_FOR_CONNECTIONS;
            sapEvent.params = pCsrRoamInfo;
            sapEvent.u1 = roamStatus;
            sapEvent.u2 = roamResult;

            /* Handle event */ 
            vosStatus = sapFsm(sapContext, &sapEvent);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                        __func__, "eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND", roamResult);
            /* Fill in the event structure */
            //TODO: support for group key MIC failure event to be handled
            vosStatus = sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_WPS_PBC_PROBE_REQ_EVENT,(v_PVOID_t) NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }
            break;

        case eCSR_ROAM_RESULT_FORCED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                       __func__, "eCSR_ROAM_RESULT_FORCED", roamResult);
            //This event can be used to inform hdd about user triggered disassoc event
            /* Fill in the event structure */
            sapSignalHDDevent( sapContext, pCsrRoamInfo, eSAP_STA_DISASSOC_EVENT, (v_PVOID_t)eSAP_STATUS_SUCCESS);
            break;

        case eCSR_ROAM_RESULT_NONE:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                    __func__, "eCSR_ROAM_RESULT_NONE", roamResult);
            //This event can be used to inform hdd about user triggered disassoc event
            /* Fill in the event structure */
            if ( roamStatus == eCSR_ROAM_SET_KEY_COMPLETE)
            {
                sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_SET_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_SUCCESS);
            }
            else if (roamStatus == eCSR_ROAM_REMOVE_KEY_COMPLETE )
            {
                sapSignalHDDevent( sapContext, pCsrRoamInfo,eSAP_STA_DEL_KEY_EVENT,(v_PVOID_t) eSAP_STATUS_SUCCESS);
            }
            break;

        case eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, CSR roamResult = %s (%d)\n",
                    __func__, "eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED", roamResult);
            /* Fill in the event structure */
            vosStatus = sapSignalHDDevent(sapContext, pCsrRoamInfo, eSAP_MAX_ASSOC_EXCEEDED, (v_PVOID_t)NULL);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
                halStatus = eHAL_STATUS_FAILURE;
            }

            break;

        case eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND:
            if (sapContext->csrRoamProfile.disableDFSChSwitch)
            {
                break;
            }
            if (eSAP_DFS_CAC_WAIT == sapContext->sapsMachine)
            {
                if (VOS_TRUE == sapContext->SapDfsInfo.sap_radar_found_status)
                {
                    /*
                     * If Radar is found, while in DFS CAC WAIT State then
                     * post stop and destroy the CAC timer and post a
                     * eSAP_DFS_CHANNEL_CAC_RADAR_FOUND  to sapFsm.
                     */
                    vos_timer_stop(&sapContext->SapDfsInfo.sap_dfs_cac_timer);
                    vos_timer_destroy(&sapContext->SapDfsInfo.sap_dfs_cac_timer);
                    sapContext->SapDfsInfo.is_dfs_cac_timer_running = 0;

                    sapEvent.event = eSAP_DFS_CHANNEL_CAC_RADAR_FOUND;
                    sapEvent.params = 0;
                    sapEvent.u1 = 0;
                    sapEvent.u2 = 0;
                    vosStatus = sapFsm(sapContext, &sapEvent);
                    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                    {
                        halStatus = eHAL_STATUS_FAILURE;
                    }
                }
            }
            else if(eSAP_STARTED == sapContext->sapsMachine)
            {
                /* Radar found on the operating channel in STARTED state,
                 * new operating channel has already been selected. Send
                 * request to SME-->PE for sending CSA IE
                 */
                sapEvent.event = eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START;
                sapEvent.params = 0;
                sapEvent.u1 = 0;
                sapEvent.u2 = 0;
                vosStatus = sapFsm(sapContext, &sapEvent);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                  halStatus = eHAL_STATUS_FAILURE;
                }
            }
            else
            {
                /* Further actions to be taken here */
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                         "In %s, eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND received in"
                         "(%d) state\n", __func__, sapContext->sapsMachine);
            }
            break;

        case eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_SUCCESS:
        case eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_FAILURE:
        {
            eCsrPhyMode phyMode =
               sapConvertSapPhyModeToCsrPhyMode(sapContext->csrRoamProfile.phyMode);
            tHalHandle hHal =
               (tHalHandle)vos_get_context( VOS_MODULE_ID_SME, sapContext->pvosGCtx);

            /* Both success and failure cases are handled intentionally handled
             * together. Irrespective of whether the channel switch IE was
             * sent out successfully or not, SAP should still vacate the
             * channel immediately
             */
            if (eSAP_STARTED == sapContext->sapsMachine)
            {
               VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          "In %s, from state %s => %s",
                          __func__, "eSAP_STARTED", "eSAP_DISCONNECTING");

               /* SAP to be moved to DISCONNECTING state */
               sapContext->sapsMachine = eSAP_DISCONNECTING;

               /* The associated stations have been informed to move
                * to a different channel. However, the AP may not always
                * select the advertised channel for operation if the radar
                * is seen. In that case, the stations will experience link-loss
                * and return back through scanning if they wish to
                */

               /* Send channel change request
                * From spec it is required that the AP should continue to
                * operate in the same mode as it is operating currently.
                * For e.g. 20/40/80 MHz operation
                */
                if (sapContext->SapDfsInfo.target_channel)
                {
                   sme_SelectCBMode(hHal, phyMode,
                                    sapContext->SapDfsInfo.target_channel);
                }

                /* Send channel switch request */
                sapEvent.event = eWNI_SME_CHANNEL_CHANGE_REQ;
                sapEvent.params = 0;
                sapEvent.u1 = 0;
                sapEvent.u2 = 0;

                /* Handle event */
                vosStatus = sapFsm(sapContext, &sapEvent);
                if(!VOS_IS_STATUS_SUCCESS(vosStatus))
                {
                   halStatus = eHAL_STATUS_FAILURE;
                }
            }
            else
            {
                /* Further actions to be taken here */
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_WARN,
                         "In %s, eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND received in"
                         "(%d) state\n", __func__, sapContext->sapsMachine);
            }
            break;
        }
        case eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS:
        {
            /* Channel change is successful. If the new channel is a DFS
             * channel, then we will to perform channel availability check
             * for 60 seconds
             */
            sapContext->channel =
                  sapContext->SapDfsInfo.target_channel;

            /* Identify if this is channel change in radar detected state */
            if (VOS_TRUE == sapContext->SapDfsInfo.sap_radar_found_status &&
                eSAP_DISCONNECTING == sapContext->sapsMachine)
            {
               /* check if currently selected channel is a DFS channel */
               if (NV_CHANNEL_DFS ==
                     vos_nv_getChannelEnabledState(sapContext->channel))
               {
                  sapContext->sapsMachine = eSAP_DISCONNECTED;

                  VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                            "In %s, from state %s => %s", __func__,
                            "eSAP_DISCONNECTING", "DISCONNECTED");

                  /* DFS Channel */
                  sapEvent.event = eSAP_DFS_CHANNEL_CAC_START;
                  sapEvent.params = pCsrRoamInfo;
                  sapEvent.u1 = 0;
                  sapEvent.u2 = 0;
               }
               else
               {
                  /* non-DFS channel */
                  sapContext->sapsMachine = eSAP_STARTING;

                  VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                            "In %s, from state %s => %s", __func__,
                            "eSAP_DISCONNECTING", "eSAP_STARTING");

                  sapContext->SapDfsInfo.sap_radar_found_status = VOS_FALSE;
                  sapEvent.event = eSAP_MAC_START_BSS_SUCCESS;
                  sapEvent.params = pCsrRoamInfo;
                  sapEvent.u1 = eCSR_ROAM_INFRA_IND;
                  sapEvent.u2 = eCSR_ROAM_RESULT_INFRA_STARTED;
               }

               /* Handle the event */
               vosStatus = sapFsm(sapContext, &sapEvent);
               if(!VOS_IS_STATUS_SUCCESS(vosStatus))
               {
                  halStatus = eHAL_STATUS_FAILURE;
               }

            }
            else
            {
               /* We may have a requirment in the future for SAP to perform
                * channel change, hence leaving this here
                */
            }

            break;
        }
        case eCSR_ROAM_RESULT_CHANNEL_CHANGE_FAILURE:
        {
            /* This is much more serious issue, we have to vacate the
             * channel due to the presence of radar but our channel change
             * failed, stop the BSS operation completely and inform hostapd
             */
            sapContext->sapsMachine = eSAP_DISCONNECTED;

            /* Inform cfg80211 and hostapd that BSS is not alive anymore */
        }
        break;

        default:
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, CSR roamResult = %s (%d) not handled\n",
                       __func__,get_eCsrRoamResult_str(roamResult),roamResult);
            break;
    }

    return halStatus;
}
