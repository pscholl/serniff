#include <stdio.h>
#include "jendefs.h"
#include <AppHardwareApi.h>
#include <AppApi.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "ieee_mac_sap.h"
#include "ieee_mac_pib.h"
#include "conf.h"
#include "ieee802_queue.c"

#define UART_LCR_OFFSET           0x0C
#define UART_DLM_OFFSET           0x04
#define UART_EER_OFFSET           0x20

#define UNALIGNED_ACCESS         *((volatile uint32 *)(0x4000018))
#define UNALIGNED_ACCESS_HANDLER  0x00012D70

#if UART == E_AHI_UART_0
# define CTS E_AHI_DIO4_INT
# define RTS E_AHI_DIO5_INT
#elif UART == E_AHI_UART_1
# define CTS E_AHI_DIO17_INT
# define RTS E_AHI_DIO18_INT
#else
# error "unknown uart"
#endif

#define DATAREADY(uart) (u8AHI_UartReadLineStatus(uart)&E_AHI_UART_LS_DR)
#define BUFREADY(uart)  (u8AHI_UartReadLineStatus(dev)&E_AHI_UART_LS_THRE)

static volatile bool connected = false;
extern uint32_t apvReg[];

void
vAHI_UartSetBaudrate(uint32_t handle, uint32_t u32BaudRate)
{
    uint8 *pu8Reg;
    uint8  u8TempLcr;
    uint16 u16Divisor;
    uint32 u32Remainder;
    uint32 UART_START_ADR;

    if(handle==UART) UART_START_ADR=0x30000000UL;
    if(handle==E_AHI_UART_1) UART_START_ADR=0x40000000UL;

    /* Put UART into clock divisor setting mode */
    pu8Reg    = (uint8 *)(UART_START_ADR + UART_LCR_OFFSET);
    u8TempLcr = *pu8Reg;
    *pu8Reg   = u8TempLcr | 0x80;

    /* Write to divisor registers:
       Divisor register = 16MHz / (16 x baud rate) */
    u16Divisor = (uint16)(16000000UL / (16UL * u32BaudRate));

    /* Correct for rounding errors */
    u32Remainder = (uint32)(16000000UL % (16UL * u32BaudRate));

    if (u32Remainder >= ((16UL * u32BaudRate) / 2))
    {
        u16Divisor += 1;
    }

    pu8Reg  = (uint8 *)UART_START_ADR;
    *pu8Reg = (uint8)(u16Divisor & 0xFF);
    pu8Reg  = (uint8 *)(UART_START_ADR + UART_DLM_OFFSET);
    *pu8Reg = (uint8)(u16Divisor >> 8);

    /* Put back into normal mode */
    pu8Reg    = (uint8 *)(UART_START_ADR + UART_LCR_OFFSET);
    u8TempLcr = *pu8Reg;
    *pu8Reg   = u8TempLcr & 0x7F;
}

int32_t
uart_write(int32_t dev, char *buf, size_t n)
{
  size_t i;
  for(i=0; i<n; i++, buf++) {
    while (!BUFREADY(UART) || (u32AHI_DioReadInput() & CTS))
      ;

    vAHI_UartWriteData(dev, *buf);
  }
  return i;
}

static void
dummy(void *p)
{
  rxq_tail_complete();
}

static bool
req_reset(bool setDefaultPib)
{
  MAC_MlmeReqRsp_s  mlmereq;
  MAC_MlmeSyncCfm_s mlmecfm;

  mlmereq.u8Type        = MAC_MLME_REQ_RESET;
  mlmereq.u8ParamLength = sizeof(MAC_MlmeReqReset_s);
  mlmereq.uParam.sReqReset.u8SetDefaultPib = setDefaultPib;
  vAppApiMlmeRequest(&mlmereq, &mlmecfm);
  return mlmecfm.u8Status!=MAC_MLME_CFM_OK;
}

static int
req_start(uint8_t chan, bool coord)
  /* this function configures the coordinator.
   * NOTE: when in periodic mode, active scan will NOT be answered immediatly.
   */
{
  void      *mac = pvAppApiGetMacHandle();
  MAC_Pib_s *pib = MAC_psPibGetHandle(mac);

  MAC_MlmeReqRsp_s  mlmereq;
  MAC_MlmeSyncCfm_s mlmecfm;

  memcpy(&pib->sCoordExtAddr, pvAppApiGetMacAddrLocation(),
         sizeof(MAC_ExtAddr_s));

  mlmereq.u8Type = MAC_MLME_REQ_START;
  mlmereq.u8ParamLength = sizeof(MAC_MlmeReqScan_s);
  mlmereq.uParam.sReqStart.u16PanId          = 0xbeef;
  mlmereq.uParam.sReqStart.u8Channel         = chan;
  mlmereq.uParam.sReqStart.u8BeaconOrder     = 15;
  mlmereq.uParam.sReqStart.u8SuperframeOrder = 15;
  mlmereq.uParam.sReqStart.u8PanCoordinator  = coord;
  mlmereq.uParam.sReqStart.u8BatteryLifeExt  = false;
  mlmereq.uParam.sReqStart.u8Realignment     = false;
  mlmereq.uParam.sReqStart.u8SecurityEnable  = false;

  vAppApiMlmeRequest(&mlmereq, &mlmecfm);

  return mlmecfm.uParam.sCfmStart.u8Status;
}

int
start_sniffer(int channel) {
  void      *mac;
  MAC_Pib_s *pib;
  int res;

  rxq_init();

  /* setup mac <-> app interface */
  u32AppApiInit((PR_GET_BUFFER) rxq_mlme_alloc, (PR_POST_CALLBACK) dummy, NULL,
                (PR_GET_BUFFER) rxq_mcps_alloc, (PR_POST_CALLBACK) dummy, NULL);

  /* get mac and pib handles */
  mac   = pvAppApiGetMacHandle();
  pib   = MAC_psPibGetHandle(mac);

  /* do a full reset */
  req_reset(true);
  MAC_vPibSetShortAddr(mac, 0x0001);
  MAC_vPibSetPromiscuousMode(mac, true, true);
  res = req_start(channel, true);
  MAC_vPibSetPromiscuousMode(mac, true, false);
  return res;
}

void
AppColdStart(void)
{
  /* initialize unaligned access handler */
  UNALIGNED_ACCESS = UNALIGNED_ACCESS_HANDLER;

  /* initialize uart to 8N1 at 115200 baud, that gives a throughput og
   * ~14.4 kb/s, maxmimum packet rate on Ieee802.15.4 is 248 packets/sec at
   * 127 bytes payload + header, which allows to return about 50bytes on the
   * uart per packet. */
  vAHI_UartEnable(UART);
  vAHI_UartReset(UART, true, true);
  vAHI_UartReset(UART, false, false);
  vAHI_UartSetControl(UART, E_AHI_UART_EVEN_PARITY,
                            E_AHI_UART_PARITY_DISABLE,
                            E_AHI_UART_WORD_LEN_8,
                            E_AHI_UART_1_STOP_BIT,
                            E_AHI_UART_RTS_HIGH);
  vAHI_UartSetBaudrate(UART, BAUD);
  vAHI_UartSetRTSCTS(UART, false);
  vAHI_DioSetDirection(CTS, RTS);
  vAHI_DioSetOutput(0x00, RTS);

  /* run the main loop, wait for channel number, then start reception
   * and packet delivery. Ack by sending "okay\n". */
  while (1) {
    static int     started = MAC_ENUM_NO_DATA;
    static char    input[10];
    static uint8_t i=0;

    /* write one rxd packet to uart */
    if (started==MAC_ENUM_SUCCESS) {
      MAC_DcfmIndHdr_s *ind;

      if (rxq_peektype() == MCPS) {
        ind = rxq_peek();

        if (ind->u8Type == MAC_MCPS_IND_DATA) {
          MAC_McpsDcfmInd_s *s = ind;
          uart_write(UART, (char*) &s->uParam.sIndData, sizeof(s->uParam.sIndData));
        }
      }

      rxq_dequeue();
    }

    /* read from uart into buf */
    while (i<sizeof(input) && DATAREADY(UART)) {
      input[i] = u8AHI_UartReadData(UART);

      if ((i+1)==sizeof(input)) { /* buffer overrun, discard input */
        memset(input, '\0', i); i = 0;
      } else if (input[i]=='\n' || input[i]=='\r') {  /* read as channel number */
        int channel;

        input[9] = '\0';            /* terminate string */
        channel  = atoi(input);     /* convert string to num */
        started  = start_sniffer(channel);

        if (started != MAC_ENUM_SUCCESS)
          printf("not started, error code: 0x%x, see MAC_Enum_e\r\n", started);
        else
          printf("started on channel %d\r\n", channel);

        memset(input, '\0', i); i = 0;
      } else
        i++;
    }
  }
}

void
AppWarmStart(void)
{
    AppColdStart();
}
