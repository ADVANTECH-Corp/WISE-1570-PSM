/*
 * Copyright (c) 2017 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//*****************************************************************************
//                 Included files
//*****************************************************************************
#include "mbed.h"
#include "common_functions.h"
#include "UDPSocket.h"
#include "EasyCellularConnection.h"
#include "mbed-trace/mbed_trace.h"
#include "CellularCommon.h"
#include "CellularSMS.h"



//*****************************************************************************
//                 MACROs and needed structures
//*****************************************************************************
//
// Check networking protocol using UDP or TCP
//
#define UDP 0
#define TCP 1

//  
// Power setting
//
#define PSM_PERIODIC_TIME			180	// 180 seconds
#define PSM_ACTIVE_TIME				0 	// 0  second

//
// MCU sleep time interval
//
#define MCU_SLEEP_TIME	(PSM_PERIODIC_TIME * 1000) // ms

//
// SIM pin code goes here
//
#ifndef MBED_CONF_APP_SIM_PIN_CODE
# define MBED_CONF_APP_SIM_PIN_CODE	"1234"
#endif
#ifndef MBED_CONF_APP_APN
# define MBED_CONF_APP_APN			"internet.iot"
#endif
#ifndef MBED_CONF_APP_USERNAME
# define MBED_CONF_APP_USERNAME    	NULL
#endif
#ifndef MBED_CONF_APP_PASSWORD
# define MBED_CONF_APP_PASSWORD   	NULL
#endif

//
// Number of retries
//
#define RETRY_COUNT 3
#define NUM_PACKETS 50
#define PACKET_INTERVAL 2 // seconds

//
// About echo server 
//
#define SEND_DATA_TIMEOUT 15000 // ms
//#define YOUR_ECHO_SERVER_IP	1
#if defined(YOUR_ECHO_SERVER_IP)
#define HOST_NAME "your_host_name";
#define PORT_NUM 9157
#else // ARM echo server
#define HOST_NAME "echo.mbedcloudtesting.com";
#define PORT_NUM 7;
#endif

//
// For debug using
//
#define MAIN_DEBUG 1
#define PRINT_TEXT_LENGTH 512
#define EVENT_BUF_SIZE	128
#ifdef MAIN_DEBUG
#define DMSG(s) do { print_function(s);} while(0)
#else
#define DMSG(...) do{}while(0);
#endif



//*****************************************************************************
//                 Global variables
//*****************************************************************************
// 
// CellularInterface object
//
EasyCellularConnection iface;
CellularPower *power;

//
// Echo server hostname & port (same for TCP and UDP)
//
const char *host_name = HOST_NAME;
const int port = PORT_NUM;

//
// Set input with no using I/Os  
//
DigitalIn gp1(W_DISABLE);
DigitalIn gp2(BACKUP);
DigitalIn gp3(WAKE);
DigitalIn gp4(I2C0_SDA);
DigitalIn gp5(I2C0_SCL);
DigitalIn gp6(I2C1_SDA);
DigitalIn gp7(I2C1_SCL);

//
// The poewr for peripherals
//
DigitalOut g_tPower(CB_PWR_ON);

//
// For debug message displayed
//
Mutex PrintMutex;
char print_text[PRINT_TEXT_LENGTH];

//
// The buffer for test
//
char recv_buf[PRINT_TEXT_LENGTH];



//*****************************************************************************
//            Local functions prototypes
//*****************************************************************************
Thread dot_thread(osPriorityNormal, 512);



//*****************************************************************************
//            Local functions
//*****************************************************************************
void print_function(const char *input_string)
{
    PrintMutex.lock();
    printf("%s", input_string);
    fflush(NULL);
    PrintMutex.unlock();
}

void dot_event()
{
	while (true) {
        wait(4);
        if (!iface.is_connected()) {
            DMSG(".");
        } else {
            break;
        }
    }
}

/**
 * Connects to the Cellular Network
 */
nsapi_error_t do_connect()
{
    nsapi_error_t retcode;
	uint8_t retry_counter = 0;

    while (!iface.is_connected()) 
	{
        retcode = iface.connect();
        if (retcode == NSAPI_ERROR_AUTH_FAILURE) {
        	DMSG("\n\nAuthentication Failure. Exiting application\n");
            return retcode;
        } else if (retcode != NSAPI_ERROR_OK) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "\n\nCouldn't connect: %d, will retry\n", retcode);
            DMSG(print_text);
            retry_counter++;
            continue;
        } else if (retcode != NSAPI_ERROR_OK && retry_counter > RETRY_COUNT) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "\n\nFatal connection failure: %d\n", retcode);
            DMSG(print_text);
            return retcode;
        }

        break;
    }

    DMSG("\n\nConnection Established.\n");
	
    return NSAPI_ERROR_OK;
}


/**
 * Opens a UDP or a TCP socket with the given echo server and performs an echo
 * transaction retrieving current.
 */
nsapi_error_t test_send_recv()
{
    int i;
	int n = 0;
	nsapi_size_or_error_t retcode;
    const char *echo_string = "Hello World";
    unsigned int send_data_size = strlen(echo_string);
	
#if MBED_CONF_APP_SOCK_TYPE == TCP
    TCPSocket sock;
#else
    UDPSocket sock;
#endif

	i = 0;
	UARTSerial *modemUART = iface.get_serial();
	SocketAddress sock_addr;
    while (i < NUM_PACKETS) 
	{ 
		//
		// Turn on the power for peripherals
		//
		g_tPower = 1;

		//
		// Open socket
		//
    	retcode = sock.open(&iface);
    	if (retcode != NSAPI_ERROR_OK) {
#if MBED_CONF_APP_SOCK_TYPE == TCP
            snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.open() fails, code: %d\n", retcode);
            DMSG(print_text);
#else
            snprintf(print_text, PRINT_TEXT_LENGTH, "UDPSocket.open() fails, code: %d\n", retcode);
            DMSG(print_text);
#endif
        	continue;
    	}

		snprintf(print_text, PRINT_TEXT_LENGTH, "Socket.open() success, code: %d\n", retcode);
    	DMSG(print_text);

		//
		// Set socket address & port number
		//
    	retcode = iface.gethostbyname(host_name, &sock_addr);
    	if (retcode != NSAPI_ERROR_OK) {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "Couldn't resolve remote host: %s, code: %d\n", host_name,
               	retcode);
        	DMSG(print_text);
        	continue;
    	}

		snprintf(print_text, PRINT_TEXT_LENGTH, "remote host: %s, code: %d\n", host_name, retcode);
   		DMSG(print_text);

    	sock_addr.set_port(port);
    	sock.set_timeout(SEND_DATA_TIMEOUT);
	
    	//
    	// Connect to server
    	//
#if MBED_CONF_APP_SOCK_TYPE == TCP
    	retcode = sock.connect(sock_addr);
    	if (retcode < 0) {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.connect() fails, code: %d\n", retcode);
        	DMSG(print_text);
        	continue;
    	} else {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "TCP: connected with %s server\n", host_name);
        	DMSG(print_text);
    	}
#endif

		//
		// Send packets to echo server
		//
#if MBED_CONF_APP_SOCK_TYPE == UDP
		retcode = sock.sendto(host_name, port, (void*) echo_string, send_data_size);
    	if (retcode < 0) {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "UDPSocket.sendto() fails, pkt #: %d, code: %d\n", i+1, retcode);
        	DMSG(print_text);

			// Failed to send data and turn off the power
			g_tPower = 0;

			// Wait PACKET_INTERVAL seconds before sending next packet
			wait(PACKET_INTERVAL);

			// Don't exit. Continue with the next packet
			continue;		
    	} else {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "UDP: Sent %d Bytes to %s ater tries %d\n", retcode, host_name, i + 1);
        	DMSG(print_text);
    	}

		//
		// Wait for data received
		//
		memset(recv_buf, 0, sizeof(recv_buf));
    	retcode = sock.recvfrom(&sock_addr, (void*) recv_buf, sizeof(recv_buf));
    	if (retcode > 0) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "UDP: Received %d Bytes \"%s\" from %s tries: %d\n", retcode, recv_buf, host_name, i + 1);
            DMSG(print_text);
    	} else {
            snprintf(print_text, PRINT_TEXT_LENGTH, "UDPSocket.recvfrom() fails after %d tries, code: %d\n", i + 1, retcode);
            DMSG(print_text);
    	}
		
#else // TCP
    	retcode = sock.send((void*) echo_string, send_data_size);
    	if (retcode < 0) {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.send() fails, code: %d\n", retcode);
        	DMSG(print_text);
			
			// Failed to send data and turn off the power
			g_tPower = 0;

			// Wait PACKET_INTERVAL seconds before sending next packet
			wait(PACKET_INTERVAL);

			// Don't exit. Continue with the next packet
			continue;
    	} else {
        	snprintf(print_text, PRINT_TEXT_LENGTH, "TCP: Sent %d Bytes to %s\n", retcode, host_name);
        	DMSG(print_text);
    	}

		//
		// Wait for data received
		//
		memset(recv_buf, 0, sizeof(recv_buf));
    	retcode = sock.recv((void*) recv_buf, send_data_size);
    	if (retcode > 0) {
            snprintf(print_text, PRINT_TEXT_LENGTH, "TCP: Received %d Bytes \"%s\" from %s tries: %d\n", retcode, recv_buf, host_name, i + 1);
            DMSG(print_text);
    	} else {
            snprintf(print_text, PRINT_TEXT_LENGTH, "TCPSocket.recvfrom() fails after %d tries, code: %d\n", i + 1, retcode);
            DMSG(print_text);
    	}

#endif // end if MBED_CONF_APP_SOCK_TYPE

        // 
        // If MCU cannot go into deep-sleep, unlock it; so it can go into deep-sleep during idle
		//
		while (!sleep_manager_can_deep_sleep()) {
        	sleep_manager_unlock_deep_sleep();
			break;
		}

		//
		// Close socket and turn off the power
		//
		DMSG("Close socket!\n\n");
    	sock.close();
		g_tPower = 0;

		//
		// MCU can go into deep sleep
		//
		ThisThread::sleep_for(MCU_SLEEP_TIME);
		modemUART->set_baud(MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE);
        // Delay to restore lpuart
        wait(1);

		//
        // Lock deep-sleep; so to prevent MCU from go into deep-sleep mode
		//
        sleep_manager_lock_deep_sleep();

		i++;

    }

    return 0;
}



//*****************************************************************************
//                 Main function
//*****************************************************************************
int main()
{
	nsapi_error_t retcode;

#if MBED_TICKLESS
	DMSG("MBED_TICKLESS is enabled\n");
#else
	DMSG("MBED_TICKLESS is disabled\n");
#endif

	// Delay for modem ready
	ThisThread::sleep_for(5000);

    // Turn on power for periphrals
	g_tPower = 1;
	
    // initialized trace log
    mbed_trace_init();
    iface.modem_debug_on(MBED_CONF_APP_MODEM_TRACE);

    // Set Pin code for SIM card
    iface.set_sim_pin(MBED_CONF_APP_SIM_PIN_CODE);

    // Set network credentials here, e.g., APN
	iface.set_credentials(MBED_CONF_APP_APN, MBED_CONF_APP_USERNAME, MBED_CONF_APP_PASSWORD);

    // Set PSM
	power = iface.get_device()->open_power(iface.get_serial());
	power->opt_power_save_mode(PSM_PERIODIC_TIME, PSM_ACTIVE_TIME);
	snprintf(print_text, PRINT_TEXT_LENGTH, 
			"\n\rPower setting: PSM:TAU->%d seconds, ActiveTime->%d seconds, MCU Sleep time:%d seconds\n\r", 
			PSM_PERIODIC_TIME, 
			PSM_ACTIVE_TIME, 
			MCU_SLEEP_TIME/1000);
   	DMSG(print_text);
	

    DMSG("\n\nmbed-os-example-cellular\n");
    DMSG("Establishing connection ");
    dot_thread.start(dot_event);

    //
    // Attempt to connect to a cellular network and running test with echo server
    //
    if (do_connect() == NSAPI_ERROR_OK) {
        retcode = test_send_recv();
        if (retcode == NSAPI_ERROR_OK) {
            DMSG("\n\nSuccess. Exiting \n\n");
            return 0;
        }
    }

    DMSG("\n\nFailure. Exiting \n\n");
    return -1;
}
// EOF
