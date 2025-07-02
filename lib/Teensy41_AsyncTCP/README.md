# Teensy41_AsyncTCP 

[![arduino-library-badge](https://www.ardu-badge.com/badge/Teensy41_AsyncTCP.svg?)](https://www.ardu-badge.com/Teensy41_AsyncTCP)
[![GitHub release](https://img.shields.io/github/release/khoih-prog/Teensy41_AsyncTCP.svg)](https://github.com/khoih-prog/Teensy41_AsyncTCP/releases)
[![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](#Contributing)
[![GitHub issues](https://img.shields.io/github/issues/khoih-prog/Teensy41_AsyncTCP.svg)](http://github.com/khoih-prog/Teensy41_AsyncTCP/issues)

<a href="https://www.buymeacoffee.com/khoihprog6" title="Donate to my libraries using BuyMeACoffee"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Donate to my libraries using BuyMeACoffee" style="height: 50px !important;width: 181px !important;" ></a>
<a href="https://www.buymeacoffee.com/khoihprog6" title="Donate to my libraries using BuyMeACoffee"><img src="https://img.shields.io/badge/buy%20me%20a%20coffee-donate-orange.svg?logo=buy-me-a-coffee&logoColor=FFDD00" style="height: 20px !important;width: 200px !important;" ></a>

---
---

## Table of contents

* [Why do we need this Teensy41_AsyncTCP library](#why-do-we-need-this-Teensy41_AsyncTCP-library)
  * [Features](#features)
  * [Why Async is better](#why-async-is-better)
  * [Currently supported Boards](#currently-supported-boards)
* [Changelog](changelog.md)
* [Prerequisites](#prerequisites)
* [Installation](#installation)
  * [Use Arduino Library Manager](#use-arduino-library-manager)
  * [Manual Install](#manual-install)
  * [VS Code & PlatformIO](#vs-code--platformio)
* [HOWTO Fix `Multiple Definitions` Linker Error](#howto-fix-multiple-definitions-linker-error)
* [Orignal documentation](#Orignal-documentation)
  * [AsyncClient and AsyncServer](#AsyncClient-and-AsyncServer)
  * [AsyncPrinter](#AsyncPrinter)
  * [AsyncTCPbuffer](#AsyncTCPbuffer)
  * [SyncClient](#SyncClient)
* [Libraries currently depend on this library](#Libraries-currently-depend-on-this-library) 
* [Examples](#examples)
  * [1. multiFileProject](examples/multiFileProject)
* [Debug Terminal Output Samples](#debug-terminal-output-samples) 
  * [1. FullyFeatured_QNEthernet on TEENSY 4.1 using DHCP](#1-FullyFeatured_QNEthernet-on-TEENSY-41-using-DHCP)
  * [2. FullyFeatured_QNEthernet on TEENSY 4.1 using StaticIP](#2-FullyFeatured_QNEthernet-on-TEENSY-41-using-StaticIP)
* [Debug](#debug)
* [Troubleshooting](#troubleshooting)
* [Issues](#issues)
* [TO DO](#to-do)
* [DONE](#done)
* [Contributions and Thanks](#contributions-and-thanks)
* [Contributing](#contributing)
* [License](#license)
* [Copyright](#copyright)

---
---


### Why do we need this [Teensy41_AsyncTCP library](https://github.com/khoih-prog/Teensy41_AsyncTCP)

#### Features

This library is based on, modified from:

1. [Hristo Gochkov's ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
2. [Hristo Gochkov's AsyncTCP](https://github.com/me-no-dev/AsyncTCP)

to apply the better and faster **asynchronous** feature of the **powerful** [ESPAsyncTCP Library](https://github.com/me-no-dev/ESPAsyncTCP) without SSL to Teensy 4.1 using LwIP-based [QNEthernet Library](https://github.com/ssilverman/QNEthernet), and will be the base for future or current and more advanced Async libraries, such as [AsyncHTTPRequest_Generic](https://github.com/khoih-prog/AsyncHTTPRequest_Generic), [AsyncHTTPRequest_Generic](https://github.com/khoih-prog/AsyncHTTPRequest_Generic), [AsyncMQTT_Generic](https://github.com/khoih-prog/AsyncMQTT_Generic), [AsyncWebServer_Teensy41](https://github.com/khoih-prog/AsyncWebServer_Teensy41), [AsyncUDP_Teensy41](https://github.com/khoih-prog/AsyncUDP_Teensy41), [AsyncDNSServer_Teensy41](https://github.com/khoih-prog/AsyncDNSServer_Teensy41), AsyncTCP_SSL_Teensy41, etc.


#### Why Async is better

- Using asynchronous network means that you can handle **more than one connection at the same time**
- **You are called once the request is ready and parsed**
- When you send the response, you are **immediately ready** to handle other connections while the server is taking care of sending the response in the background
- **Speed is OMG**
- **Easy to use API, HTTP Basic and Digest MD5 Authentication (default), ChunkedResponse**
- Easily extensible to handle **any type of content**
- Supports Continue 100
- **Async WebSocket plugin offering different locations without extra servers or ports**
- Async EventSource (Server-Sent Events) plugin to send events to the browser
- URL Rewrite plugin for conditional and permanent url rewrites
- ServeStatic plugin that supports cache, Last-Modified, default index and more
- Simple template processing engine to handle templates


### Currently supported Boards

1. **Teensy 4.1 using QNEthernet Library**


---
---

## Prerequisites

 1. [`Arduino IDE 1.8.19+` for Arduino](https://github.com/arduino/Arduino). [![GitHub release](https://img.shields.io/github/release/arduino/Arduino.svg)](https://github.com/arduino/Arduino/releases/latest)
 2. [`Teensy core v1.57+`](https://www.pjrc.com/teensy/td_download.html) for Teensy 4.1
 3. [`QNEthernet Library version v0.15.0+`](https://github.com/ssilverman/QNEthernet) for Teensy 4.1 built-in Ethernet.

---
---

## Installation

### Use Arduino Library Manager

The best and easiest way is to use `Arduino Library Manager`. Search for [**Teensy41_AsyncTCP**](https://github.com/khoih-prog/Teensy41_AsyncTCP), then select / install the latest version.
You can also use this link [![arduino-library-badge](https://www.ardu-badge.com/badge/Teensy41_AsyncTCP.svg?)](https://www.ardu-badge.com/Teensy41_AsyncTCP) for more detailed instructions.

### Manual Install

Another way to install is to:

1. Navigate to [**Teensy41_AsyncTCP**](https://github.com/khoih-prog/Teensy41_AsyncTCP) page.
2. Download the latest release `Teensy41_AsyncTCP-main.zip`.
3. Extract the zip file to `Teensy41_AsyncTCP-main` directory 
4. Copy whole `Teensy41_AsyncTCP-main` folder to Arduino libraries' directory such as `~/Arduino/libraries/`.

### VS Code & PlatformIO

1. Install [VS Code](https://code.visualstudio.com/)
2. Install [PlatformIO](https://platformio.org/platformio-ide)
3. Install [**Teensy41_AsyncTCP** library](https://registry.platformio.org/libraries/khoih-prog/Teensy41_AsyncTCP) by using [Library Manager](https://registry.platformio.org/libraries/khoih-prog/Teensy41_AsyncTCP/installation). Search for **Teensy41_AsyncTCP** in [Platform.io Author's Libraries](https://platformio.org/lib/search?query=author:%22Khoi%20Hoang%22)
4. Use included [platformio.ini](platformio/platformio.ini) file from examples to ensure that all dependent libraries will installed automatically. Please visit documentation for the other options and examples at [Project Configuration File](https://docs.platformio.org/page/projectconf.html)

---
---

### HOWTO Fix `Multiple Definitions` Linker Error

The current library implementation, using `xyz-Impl.h` instead of standard `xyz.cpp`, possibly creates certain `Multiple Definitions` Linker error in certain use cases.

You can include this `.hpp` file

```
// Can be included as many times as necessary, without `Multiple Definitions` Linker Error
#include "Teensy41_AsyncTCP.hpp"     //https://github.com/khoih-prog/Teensy41_AsyncTCP
```

in many files. But be sure to use the following `.h` file **in just 1 `.h`, `.cpp` or `.ino` file**, which must **not be included in any other file**, to avoid `Multiple Definitions` Linker Error

```
// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include "Teensy41_AsyncTCP.h"       //https://github.com/khoih-prog/Teensy41_AsyncTCP
```

Check the new [**multiFileProject** example](examples/multiFileProject) for a `HOWTO` demo.


---
---


## Orignal documentation

Check [AsyncTCP Library](https://github.com/me-no-dev/AsyncTCP)

This is a fully asynchronous TCP library, aimed at enabling trouble-free, multi-connection network environment for **Teensy 4.1 using QNEthernet Library**.

### AsyncClient and AsyncServer

The base classes on which everything else is built. They expose all possible scenarios, but are really raw and require more skills to use.

### AsyncPrinter

This class can be used to send data like any other `Print` interface (`Serial` for example).

The object then can be used outside of the Async callbacks (the loop) and receive asynchronously data using `onData`. The object can be checked if the underlying `AsyncClient`is connected, or hook to the `onDisconnect` callback.

### AsyncTCPbuffer

This class is really similar to the `AsyncPrinter`, but it can buffer some of the incoming data.

### SyncClient

It is exactly what it sounds like. This is a standard, synchronous blocking TCP Client you're used to.


---
---

### Libraries currently depend on this library

1. [AsyncWebServer_Teensy41](https://github.com/khoih-prog/AsyncWebServer_Teensy41) [![GitHub release](https://img.shields.io/github/release/khoih-prog/AsyncWebServer_Teensy41.svg)](https://github.com/khoih-prog/AsyncWebServer_Teensy41/releases/latest)
2. [AsyncHTTPRequest_Teensy41](https://github.com/khoih-prog/AsyncHTTPRequest_Teensy41) [![GitHub release](https://img.shields.io/github/release/khoih-prog/AsyncHTTPRequest_Teensy41.svg)](https://github.com/khoih-prog/AsyncHTTPRequest_Teensy41/releases/latest)
3. [AsyncMQTT_Generic](https://github.com/khoih-prog/AsyncMQTT_Generic) [![GitHub release](https://img.shields.io/github/release/khoih-prog/AsyncMQTT_Generic.svg)](https://github.com/khoih-prog/AsyncMQTT_Generic/releases/latest)


---
---

### Examples

 1. [multiFileProject](examples/multiFileProject).

---
---

### Debug Terminal Output Samples

#### 1. FullyFeatured_QNEthernet on TEENSY 4.1 using DHCP

Following is the debug terminal when running example [FullyFeatured_QNEthernet](https://github.com/khoih-prog/AsyncMQTT_Generic/tree/main/examples/QNEthernet/FullyFeatured_QNEthernet) on **Teensy 4.1 using QNEthernet Library** to demonstrate the operation of Async MQTT request, relied on [Teensy41_AsyncTCP Library](https://github.com/khoih-prog/Teensy41_AsyncTCP)


```
Starting FullyFeatured_QNEthernet on TEENSY 4.1
AsyncMQTT_Generic v1.7.0 for Teensy 4.1 QNEthernet
Initialize Ethernet using DHCP => Connected! IP address:192.168.2.107
Connecting to MQTT...
Connected to MQTT broker: broker.emqx.io, port: 1883
PubTopic: async-mqtt/Teensy41_QNEthernet_Pub
************************************************
Session present: 0
Subscribing at QoS 2, packetId: 1
Publishing at QoS 0
Publishing at QoS 1, packetId: 2
Publishing at QoS 2, packetId: 3
************************************************
Subscribe acknowledged.
  packetId: 1
  qos: 2
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test3
  qos: 2
  dup: 0
  retain: 1
  len: 29
  index: 0
  total: 29
Publish acknowledged.
  packetId: 2
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test1
  qos: 0
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test2
  qos: 1
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test3
  qos: 2
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish acknowledged.
  packetId: 3
```

---

### 2. FullyFeatured_QNEthernet on TEENSY 4.1 using StaticIP

Following is the debug terminal when running example [FullyFeatured_QNEthernet](https://github.com/khoih-prog/AsyncMQTT_Generic/tree/main/examples/QNEthernet/FullyFeatured_QNEthernet) on **Teensy 4.1 using QNEthernet Library** to demonstrate the operation of Async MQTT request, relied on [Teensy41_AsyncTCP Library](https://github.com/khoih-prog/Teensy41_AsyncTCP)

```
Starting FullyFeatured_QNEthernet on TEENSY 4.1
AsyncMQTT_Generic v1.7.0 for Teensy 4.1 QNEthernet
Initialize Ethernet using static IP => Connected! IP address:192.168.2.222
Connecting to MQTT...
Connected to MQTT broker: broker.emqx.io, port: 1883
PubTopic: async-mqtt/Teensy41_QNEthernet_Pub
************************************************
Session present: 0
Subscribing at QoS 2, packetId: 1
Publishing at QoS 0
Publishing at QoS 1, packetId: 2
Publishing at QoS 2, packetId: 3
************************************************
Subscribe acknowledged.
  packetId: 1
  qos: 2
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test3
  qos: 2
  dup: 0
  retain: 1
  len: 29
  index: 0
  total: 29
Publish acknowledged.
  packetId: 2
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test1
  qos: 0
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test2
  qos: 1
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish received.
  topic: async-mqtt/Teensy41_QNEthernet_Pub
  message: Teensy41_QNEthernet_Pub Test3
  qos: 2
  dup: 0
  retain: 0
  len: 29
  index: 0
  total: 29
Publish acknowledged.
  packetId: 3
```


---
---

### Debug

Debug is enabled by default on Serial.

You can also change the debugging level `_TEENSY41_ASYNC_TCP_LOGLEVEL_` from 0 to 4 in the sketch.
```cpp
#define _TEENSY41_ASYNC_TCP_LOGLEVEL_     1
```

---

### Troubleshooting

If you get compilation errors, more often than not, you may need to install a newer version of the core for Arduino boards.

Sometimes, the library will only work if you update the board core to the latest version because I am using newly added functions.


---
---

### Issues

Submit issues to: [Teensy41_AsyncTCP issues](https://github.com/khoih-prog/Teensy41_AsyncTCP/issues)

---

## TO DO

1. Search for bug and improvement.
2. `Teensy41_AsyncTCP_SSL` library for **Teensy 4.1 using QNEthernet Library**
3. Permit both TLS/SSL and non-TLS/SSL

---

## DONE

1. Add support to **Teensy 4.1 using QNEthernet Library**
2. Add Table of Contents
3. Add debug feature
4. Use `h-only` style library
5. Add example [multiFileProject](examples/multiFileProject) to demo for multiple-file project to avoid `multiple-definitions` linker error
6. Fix issue with slow browsers or network
7. Remove hard-code if possible
8. Improve debug messages by adding functions to display `error/state messages` instead of `cryptic error/state number`

---
---

### Contributions and Thanks

Many thanks for everyone for bug reporting, new feature suggesting, testing and contributing to the development of this library.

### Contributions and Thanks

1. Based on and modified from [Hristo Gochkov's AsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP). Many thanks to [Hristo Gochkov](https://github.com/me-no-dev) for great [ESPAsyncTCP Library](https://github.com/me-no-dev/ESPAsyncTCP) and [AsyncTCP Library](https://github.com/me-no-dev/AsyncTCP)

<table>
  <tr>
    <td align="center"><a href="https://github.com/me-no-dev"><img src="https://github.com/me-no-dev.png" width="100px;" alt="me-no-dev"/><br /><sub><b>⭐️⭐️ Hristo Gochkov</b></sub></a><br /></td>
  </tr> 
</table>

---

## Contributing

If you want to contribute to this project:

- Report bugs and errors
- Ask for enhancements
- Create issues and pull requests
- Tell other people about this library

---
---

### License

- The library is licensed under [LGPLv3](https://github.com/khoih-prog/Teensy41_AsyncTCP/blob/main/LICENSE)

---

## Copyright

- Copyright 2016- Hristo Gochkov
- Copyright 2022- Khoi Hoang


