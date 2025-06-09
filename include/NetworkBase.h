// Firmware_Teensy_AiO-NG-v6 is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-NG-v6 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-NG-v6 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef NETWORKBASE_H_
#define NETWORKBASE_H_
#include "Arduino.h"
#include "EEPROM.h"
#include "mongoose.h"
#include "RTCMHandler.h"

// EEPROM storage layout
#define eeVersionStore 1  // 100 bytes
#define ipStore 100       // 100 bytes

uint16_t ip_ver;

// Networking variables 
struct NetConfigStruct
{
  static constexpr uint8_t defaultIP[5] = {192, 168, 5, 126};
  uint8_t currentIP[5] = {192, 168, 5, 126};
  uint8_t gatewayIP[5] = {192, 168, 5, 1};
  uint8_t broadcastIP[5] = {192, 168, 5, 255};
};
NetConfigStruct const defaultNet;
NetConfigStruct netConfig = defaultNet;

struct mg_connection *sendAgio;

// void rtcmHandler(void);
// void pgnHandler(void);

// Send byte arrays to AgIO
void sendUDPbytes(uint8_t *message, int msgLen)
{
  if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
    return; // Check if IP stack is up.
  // Send data
  if (mg_send(sendAgio, message, msgLen) <= 0)
  {
    Serial.println("UDP Send to AgIO failed.\r\n");
  }
  else
  {
    mg_iobuf_del(&sendAgio->send, 0, sendAgio->send.len);
  }
}

// Send char arrays to AgIO
void sendUDPchars(char *stuff)
{
  if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
    return; // Check if IP stack is up.
  mg_printf(sendAgio, stuff);
}

// pgnHandler stub Feel free to move me into your code but make a reference so NetworkBase can find me.
void pgnHandler(struct mg_connection *udpPacket, int ev, void *ev_data, void *fn_data)
{
  if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
    return; // Check if IP stack is up.
  if (ev == MG_EV_ERROR)
  {
    Serial.printf("Error: %s", (char *)ev_data);
  }
  if (ev == MG_EV_READ && mg_ntohs(udpPacket->rem.port) == 9999 && udpPacket->recv.len >= 5)
  {
    Serial.println("I am the pgnHandler stub. Populate me."); // The actual handling code should be outside NetworkBase.h. Make sure there is a reference to it.
    // Verify first 3 PGN header bytes
    if (udpPacket->recv.buf[0] != 128 || udpPacket->recv.buf[1] != 129 || udpPacket->recv.buf[2] != 127)
      return;
  }
}

// rtcmHandler stub. Feel free to move me into your code but make a reference so NetworkBase can find me.
// void rtcmHandler(struct mg_connection *udpPacket, int ev, void *ev_data, void *fn_data)
// {
//   if (g_mgr.ifp->state != MG_TCPIP_STATE_READY)
//     return; // Check if IP stack is up.
//   if (ev == MG_EV_ERROR)
//   {
//     Serial.printf("Error: %s", (char *)ev_data);
//   }
//   if (ev == MG_EV_READ && mg_ntohs(udpPacket->rem.port) == 9999 && udpPacket->recv.len >= 5)
//   {
//     Serial.println("I am the rtcmHandler stub. Populate me."); // The actual handling code should be outside NetworkBase.h. Make sure there is a reference to it.
//     // Verify first 3 PGN header bytes
//     if (udpPacket->recv.buf[0] != 128 || udpPacket->recv.buf[1] != 129 || udpPacket->recv.buf[2] != 127)
//       return;
//   }
// }

// Write default IP to EEPROM
void save_default_net()
{
  // IP stored in 300
  EEPROM.put(ipStore, defaultNet);
}

// Load current IP from EEPROM
void load_current_net()
{
  // IP loaded from 300
  EEPROM.get(ipStore, netConfig);
}

void storedCfgSetup()
{
  uint16_t eth_ee_read;
  EEPROM.get(eeVersionStore, eth_ee_read);

  if (eth_ee_read != ip_ver)
  { // if EE is out of sync, write defaults to EE
    EEPROM.put(eeVersionStore, ip_ver);
    save_default_net();
    load_current_net();
    Serial.print("\r\n\nWriting IP address defaults to EEPROM\r\n");
  }
  else
  {
    load_current_net();
    Serial.print("\r\n\nLoaded IP address from EEPROM\r\n");
  }
}

// Setup UDP comms channels
static uint32_t ipv4ary(const uint8_t input[])
{
  char buf[16];
  mg_snprintf(buf, sizeof(buf), "%d.%d.%d.%d", input[0], input[1], input[2], input[3]);
  struct mg_addr a = {};
  mg_aton(mg_str(buf), &a);
  return *(uint32_t *)&a.ip;
}

void udpSetup()
{
  g_mgr.ifp->enable_dhcp_client = 0;
  g_mgr.ifp->ip = ipv4ary(netConfig.currentIP);
  g_mgr.ifp->gw = ipv4ary(netConfig.gatewayIP);
  g_mgr.ifp->mask = MG_IPV4(255, 255, 255, 0);

  RTCMHandler::init();

  char pgnListenURL[50];
  char rtcmListen[150];
  mg_snprintf(pgnListenURL, sizeof(pgnListenURL), "udp://%d.%d.%d.126:8888", netConfig.currentIP[0], netConfig.currentIP[1], netConfig.currentIP[2]);
  mg_snprintf(rtcmListen, sizeof(rtcmListen), "udp://%d.%d.%d.126:2233", netConfig.currentIP[0], netConfig.currentIP[1], netConfig.currentIP[2]);

  if (mg_listen(&g_mgr, pgnListenURL, pgnHandler, NULL) != NULL)
  // if (mg_listen(&g_mgr, pgnListenURL, NULL, NULL) != NULL)
  {
    // listenPGNs = true;
    MG_DEBUG(("Listening for AgIO on UDP 8888"));
  }
  else
  {
    MG_DEBUG(("AgIO on UDP 8888 did not open"));
  }

  if (mg_listen(&g_mgr, rtcmListen, RTCMHandler::handleRTCM, NULL) != NULL)
  /// if (mg_listen(&g_mgr, rtcmListen, NULL, NULL) != NULL)
  {
    // listenRtcm = true;
    MG_DEBUG(("Listening for RTCM on UDP 2233"));
  }
  else
  {
    MG_DEBUG(("RTCM on UDP 2233 did not open"));
  }

  // Create UDP connection to broadcast address
  char agioURL[25];
  strcpy(agioURL, "udp://");
  itoa(netConfig.currentIP[0], agioURL + strlen(agioURL), 10);
  strcat(agioURL, ".");
  itoa(netConfig.currentIP[1], agioURL + strlen(agioURL), 10);
  strcat(agioURL, ".");
  itoa(netConfig.currentIP[2], agioURL + strlen(agioURL), 10);
  strcat(agioURL, ".255:9999");

  sendAgio = mg_connect(&g_mgr, agioURL, NULL, NULL);

  if (sendAgio->is_client)
  {
    // agioConnect = true;
    MG_DEBUG(("Connected to AgIO"));
  }
  else
  {
    MG_DEBUG(("Trying to connect to AgIO"));
    return;
  }
}

//---------------------------------- Don't touch anything below this line. Here be dragons. You have been warned. ---------------------------------------

    extern "C"
    {
// #include "mongoose_glue.h"
#define TRNG_ENT_COUNT 16
  void ENET_IRQHandler(void);
  uint64_t mg_millis(void)
  {
    return millis();
  }
  bool mg_random(void *buf, size_t len)
  {
    static bool initialised;
    static uint32_t rng_index = TRNG_ENT_COUNT;
    uint32_t r, i;

    if (!initialised)
    {
      initialised = true;
      CCM_CCGR6 |= CCM_CCGR6_TRNG(CCM_CCGR_ON);
      TRNG_MCTL = TRNG_MCTL_RST_DEF | TRNG_MCTL_PRGM; // reset to program mode
      TRNG_MCTL = TRNG_MCTL_SAMP_MODE(2);             // start run mode, vonneumann
      TRNG_ENT15;                                     // discard any stale data, start gen cycle
    }

    for (i = 0; i < len; i++)
    {
      if (rng_index >= TRNG_ENT_COUNT)
      {
        rng_index = 0;
        while ((TRNG_MCTL & TRNG_MCTL_ENT_VAL) == 0 &&
               (TRNG_MCTL & TRNG_MCTL_ERR) == 0)
          ; // wait for entropy ready
      }
      r = *(&TRNG_ENT0 + rng_index++);
      ((uint8_t *)buf)[i] = (uint8_t)(r & 255);
    }
    return true;
  }

// Custom log
#if MG_ENABLE_CUSTOM_LOG
  static void *s_log_func_param = NULL;

  static mg_pfn_t s_log_func = mg_pfn_stdout;

  static void logc(unsigned char c)
  {
    s_log_func((char)c, s_log_func_param);
  }

  static void logs(const char *buf, size_t len)
  {
    size_t i;
    for (i = 0; i < len; i++)
      logc(((unsigned char *)buf)[i]);
  }

  void mg_log_prefix(int level, const char *file, int line, const char *fname)
  {
    const char *p = strrchr(file, '/');
    char buf[60];
    size_t n;
    if (p == NULL)
      p = strrchr(file, '\\');
    n = mg_snprintf(buf, sizeof(buf), "%-6lld %d %s:%d:%s", mg_millis(), level,
                    p == NULL ? file : p + 1, line, fname);
    if (n > sizeof(buf) - 2)
      n = sizeof(buf) - 2;
    while (n < sizeof(buf))
      buf[n++] = '-';
    buf[sizeof(buf) - 2] = '>';
    logs(buf, n - 1);
  }

  void mg_log(const char *fmt, ...)
  {
    va_list ap;
    va_start(ap, fmt);
    mg_vxprintf(s_log_func, s_log_func_param, fmt, &ap);
    va_end(ap);
    logs("\r\n", 2);
  }
#endif
  // end custom log
}

#define CLRSET(reg, clear, set) ((reg) = ((reg) & ~(clear)) | (set))
#define RMII_PAD_INPUT_PULLDOWN 0x30E9
#define RMII_PAD_INPUT_PULLUP 0xB0E9
#define RMII_PAD_CLOCK 0x0031

void trng_init()
{
}

// initialize the ethernet hardware
void ethernet_init(void)
{
  CCM_CCGR1 |= CCM_CCGR1_ENET(CCM_CCGR_ON);
  // configure PLL6 for 50 MHz, pg 1173
  CCM_ANALOG_PLL_ENET_CLR =
      CCM_ANALOG_PLL_ENET_POWERDOWN | CCM_ANALOG_PLL_ENET_BYPASS | 0x0F;
  CCM_ANALOG_PLL_ENET_SET = CCM_ANALOG_PLL_ENET_ENABLE |
                            CCM_ANALOG_PLL_ENET_BYPASS
                            /*| CCM_ANALOG_PLL_ENET_ENET2_REF_EN*/
                            | CCM_ANALOG_PLL_ENET_ENET_25M_REF_EN
                            /*| CCM_ANALOG_PLL_ENET_ENET2_DIV_SELECT(1)*/
                            | CCM_ANALOG_PLL_ENET_DIV_SELECT(1);
  while (
      !(CCM_ANALOG_PLL_ENET & CCM_ANALOG_PLL_ENET_LOCK))
    ; // wait for PLL lock
  CCM_ANALOG_PLL_ENET_CLR = CCM_ANALOG_PLL_ENET_BYPASS;
  // configure REFCLK to be driven as output by PLL6, pg 326

  CLRSET(IOMUXC_GPR_GPR1,
         IOMUXC_GPR_GPR1_ENET1_CLK_SEL | IOMUXC_GPR_GPR1_ENET_IPG_CLK_S_EN,
         IOMUXC_GPR_GPR1_ENET1_TX_CLK_DIR);

  // Configure pins
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_14 = 5; // Reset   B0_14 Alt5 GPIO7.15
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_15 = 5; // Power   B0_15 Alt5 GPIO7.14
  GPIO7_GDIR |= (1 << 14) | (1 << 15);
  GPIO7_DR_SET = (1 << 15);                                   // Power on
  GPIO7_DR_CLEAR = (1 << 14);                                 // Reset PHY chip
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_04 = RMII_PAD_INPUT_PULLDOWN; // PhyAdd[0] = 0
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_06 = RMII_PAD_INPUT_PULLDOWN; // PhyAdd[1] = 1
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_05 = RMII_PAD_INPUT_PULLUP;   // Slave mode
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_11 = RMII_PAD_INPUT_PULLDOWN; // Auto MDIX
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_07 = RMII_PAD_INPUT_PULLUP;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_08 = RMII_PAD_INPUT_PULLUP;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_09 = RMII_PAD_INPUT_PULLUP;
  IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_10 = RMII_PAD_CLOCK;
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_05 = 3;        // RXD1    B1_05 Alt3, pg 525
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_04 = 3;        // RXD0    B1_04 Alt3, pg 524
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_10 = 6 | 0x10; // REFCLK  B1_10 Alt6, pg 530
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_11 = 3;        // RXER    B1_11 Alt3, pg 531
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_06 = 3;        // RXEN    B1_06 Alt3, pg 526
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_09 = 3;        // TXEN    B1_09 Alt3, pg 529
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_07 = 3;        // TXD0    B1_07 Alt3, pg 527
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_08 = 3;        // TXD1    B1_08 Alt3, pg 528
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_15 = 0;        // MDIO    B1_15 Alt0, pg 535
  IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_14 = 0;        // MDC     B1_14 Alt0, pg 534
  IOMUXC_ENET_MDIO_SELECT_INPUT = 2;           // GPIO_B1_15_ALT0, pg 792
  IOMUXC_ENET0_RXDATA_SELECT_INPUT = 1;        // GPIO_B1_04_ALT3, pg 792
  IOMUXC_ENET1_RXDATA_SELECT_INPUT = 1;        // GPIO_B1_05_ALT3, pg 793
  IOMUXC_ENET_RXEN_SELECT_INPUT = 1;           // GPIO_B1_06_ALT3, pg 794
  IOMUXC_ENET_RXERR_SELECT_INPUT = 1;          // GPIO_B1_11_ALT3, pg 795
  IOMUXC_ENET_IPG_CLK_RMII_SELECT_INPUT = 1;   // GPIO_B1_10_ALT6, pg 791
  delay(1);
  GPIO7_DR_SET = (1 << 14); // Start PHY chip
  // ENET_MSCR = ENET_MSCR_MII_SPEED(9);
  delay(1);

  SCB_ID_CSSELR = 0;                // Disable DC cache for Ethernet DMA to work
  asm volatile("dsb" ::: "memory"); // Perhaps the alternative way
  SCB_CCR &= ~SCB_CCR_DC;           // would be to invalidate DC cache
  asm volatile("dsb" ::: "memory"); // after each IO in the driver

  // Setup IRQ handler
  attachInterruptVector(IRQ_ENET, ENET_IRQHandler);
  NVIC_ENABLE_IRQ(IRQ_ENET);
}

#endif // NETWORKBASE_H_