#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <time.h>
#include "osal.h"
#include "oshw.h"
#include "ethercatmain.h"
#include "ethercattype.h"
#include "ethercatbase.h"
#include "ethercatmain.h"
#include "ethercat.h"
#include "ethercatfoe.h"

#define EC_TIMEOUTMON 500
#define FWBUFSIZE (1024 * 1024 - 128 * 1024)

#define SDO_READ(sln, idx, sub, buf)    \
    {   \
        buf=0;  \
        int __s = sizeof(buf);    \
        int __ret = ec_SDOread(sln, idx, sub, FALSE, &__s, &buf, EC_TIMEOUTRXM);   }

#define SDO_WRITE(sln, idx, sub, buf, value) \
    {   \
        int __s = sizeof(buf);  \
        buf = value;    \
        int __ret = ec_SDOwrite(sln, idx, sub, FALSE, __s, &buf, 1000000);  }

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	uint16	tx_controlword;
	int32	tx_target_position;
	int32	tx_target_velocity;
	int32	tx_target_torque;
	int8	tx_modes_of_oper;
	uint32	tx_digital_outputs;
}pdo_input_t;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	uint16	rx_statusword;
	int32	rx_actual_position;
	int32	rx_actual_velocity;
	int32	rx_actual_torque;
	int8	rx_modes_of_oper_disp;
	uint32	rx_digital_inputs;
}pdo_output_t;
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
typedef struct
{
	int wkc;
	int abort_code;
}ret_struct_t;

#pragma pack(pop)

struct timespec ts;

//pdo_data_t* IOmap;
uint8 IOmap[1024];
OSAL_THREAD_HANDLE thread1;
int32 expectedWKC;
boolean needlf;
volatile int wkc;
volatile int wkc_pdo;
boolean inOP;
uint8 currentgroup = 0;
UINT mmResult;
char filebuffer[FWBUFSIZE];
//char ifname[1024];

//pdo_data_t* pdo_data = (pdo_data_t*)IOmap;


/* Deprecated !!! Multimedia Timer RT Thread now set in .NET application */
/* most basic RT thread for process data, just does IO transfer */
void CALLBACK RTthread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	ec_send_processdata();
	wkc_pdo = ec_receive_processdata(EC_TIMEOUTRET);

	/* do RT control stuff here */
}

__declspec(dllexport) void __stdcall ec_send_processdata_extern(void)
{
	ec_send_processdata();
}

__declspec(dllexport) int __stdcall ec_receive_processdata_extern(void)
{
	wkc_pdo = ec_receive_processdata(EC_TIMEOUTRET);
	return wkc_pdo;
}

OSAL_THREAD_FUNC ecatcheck(void *lpParam)
{
	int slave;

	while (1)
	{
		if (inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
		{
			if (needlf)
			{
				needlf = FALSE;
				printf("\n");
			}
			/* one ore more slaves are not responding */
			ec_group[currentgroup].docheckstate = FALSE;
			ec_readstate();
			for (slave = 1; slave <= ec_slavecount; slave++)
			{
				if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
				{
					ec_group[currentgroup].docheckstate = TRUE;
					if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
					{
						printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
						ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
						ec_writestate(slave);
					}
					else if (ec_slave[slave].state == EC_STATE_SAFE_OP)
					{
						printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
						ec_slave[slave].state = EC_STATE_OPERATIONAL;
						ec_writestate(slave);
					}
					else if (ec_slave[slave].state > EC_STATE_NONE)
					{
						if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
						{
							ec_slave[slave].islost = FALSE;
							printf("MESSAGE : slave %d reconfigured\n", slave);
						}
					}
					else if (!ec_slave[slave].islost)
					{
						/* re-check state */
						ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
						if (ec_slave[slave].state == EC_STATE_NONE)
						{
							ec_slave[slave].islost = TRUE;
							printf("ERROR : slave %d lost\n", slave);
						}
					}
				}
				if (ec_slave[slave].islost)
				{
					if (ec_slave[slave].state == EC_STATE_NONE)
					{
						if (ec_recover_slave(slave, EC_TIMEOUTMON))
						{
							ec_slave[slave].islost = FALSE;
							printf("MESSAGE : slave %d recovered\n", slave);
						}
					}
					else
					{
						ec_slave[slave].islost = FALSE;
						printf("MESSAGE : slave %d found\n", slave);
					}
				}
			}
			if (!ec_group[currentgroup].docheckstate)
				printf("OK : all slaves resumed OPERATIONAL.\n");
		}
		osal_usleep(10000);
	}
}

int32 StartUpConfig(uint16 sln)//, boolean* config_tx_pdos, boolean* config_rx_pdos)//, boolean x1601, boolean x1602, boolean x1603, boolean x1604, boolean x1605, boolean x1a00, boolean x1a01, boolean x1a02, boolean x1a03, boolean x1a04, boolean x1a05)
{
	uint32 buf32;
	uint16 buf16;
	uint8 buf8;
	uint32 vendorID;
	int32 retval = 0;
	uint8 u8val = 0;
	int32 cnt_1c12 = 0;
	int32 cnt_1c13 = 0;

	SDO_READ(sln, 0x1018, 1, buf32); /*Vendor ID*/

	vendorID = buf32;

	SDO_READ(sln, 0x1000, 0, buf32);


	SDO_WRITE(sln, 0x1c12, 0, buf8, 0);
	SDO_WRITE(sln, 0x1c13, 0, buf8, 0);

	//if (buf32 != 5001)
	if (vendorID == 41)
	{
		
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 1, buf16, 0x1600); /*Controlword*/
			cnt_1c12++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 2, buf16, 0x1601); /*Target Position*/
			cnt_1c12++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 3, buf16, 0x1602); /*Target Velocity*/
			cnt_1c12++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 4, buf16, 0x1603); /*Target Torque*/
			cnt_1c12++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 5, buf16, 0x1604); /* Modes of Operation */
			cnt_1c12++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 6, buf16, 0x1605); //Digital Outputs	cnt_1c12++;
			cnt_1c12++;
		}

		SDO_WRITE(sln, 0x1c12, 0, buf8, cnt_1c12);

		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 1, buf16, 0x1A00); /* Statusword */
			cnt_1c13++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 2, buf16, 0x1A01); /* Position Actual Value */
			cnt_1c13++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 3, buf16, 0x1A02); /* Velocity Actual Value */
			cnt_1c13++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 4, buf16, 0x1A03); /* Torque Actual Value */
			cnt_1c13++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 5, buf16, 0x1A04); /* Modes of Operation Display */
			cnt_1c13++;
		}
		if (1)
		{
			SDO_WRITE(sln, 0x1c13, 6, buf16, 0x1A05); /* Digital Inputs */
			cnt_1c13++;
		}

		SDO_WRITE(sln, 0x1c13, 0, buf8, cnt_1c13);

		/*printf("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
			1, ec_slave[1].name, ec_slave[1].Obits, ec_slave[1].Ibits,
			ec_slave[1].state, ec_slave[1].pdelay, ec_slave[1].hasdc);*/
	}
	else //if (vendorID == 599) // Dunker Motor
	{

		if (1)
		{
			SDO_WRITE(sln, 0x1c12, 1, buf16, 0x1600); 
			cnt_1c12++;
			SDO_WRITE(sln, 0x1c12, 0, buf8, cnt_1c12);

			SDO_WRITE(sln, 0x1c13, 1, buf16, 0x1A00); 
			cnt_1c13++;
			SDO_WRITE(sln, 0x1c13, 0, buf8, cnt_1c13);
		}

		//SDO_WRITE(sln, 0x4010, 12, buf8, 32); //EventOnError_cmd1

		//SDO_WRITE(sln, 0x42A0, 11, buf8, 1); //CurrentActual Filter Level

		//SDO_WRITE(sln, 0x6065, 0, buf32, 4096);

		//SDO_WRITE(sln, 0x6067, 0, sbuf32, -50); //Position window

		//SDO_WRITE(sln, 0x6068, 0, buf16, 15); //Position window

		//SDO_WRITE(sln, 0x608a, 0, buf8, 172);
	}

	/* Commented out Complete Access because it is not used at the moment and I don't want it to break stuff */
	//ec_slave[sln].CoEdetails &= ECT_COEDET_SDOCA;

	return 1;
}

int32 input_bin(uint8 *fname, int32 *length)
{
	FILE *fp;

	int32 cc = 0, c;

	fp = fopen(fname, "rb");
	if (fp == NULL)
		return 0;
	while (((c = fgetc(fp)) != EOF) && (cc < FWBUFSIZE))
		filebuffer[cc++] = (uint8)c;
	*length = cc;
	fclose(fp);
	return 1;
}


/* Define the Functions for extern use in C# */
#ifdef __cplusplus
extern "C" {
#endif

	__declspec(dllexport) int32 __stdcall ec_find_adapters_desc_extern(void)
	{
		ec_adaptert * ret_adapter;

		ret_adapter = oshw_find_adapters();

		int32 ibuf = sizeof(ret_adapter);

		return ibuf;
	}

	__declspec(dllexport) int32 __stdcall ec_init_extern(void)
	{
		ec_adaptert * ret_adapter;

		ret_adapter = oshw_find_adapters();

		return ecx_init(&ecx_context, ret_adapter->name);
	}

	__declspec(dllexport) void __stdcall GetNetworkAdapter(uint8* buf)
	{
		ec_adaptert * ret_adapter1;

		ret_adapter1 = oshw_find_adapters();

		while (ret_adapter1 != NULL)
		{
			strcat(buf, ret_adapter1->desc);
			strcat(buf, "*");
			ret_adapter1 = ret_adapter1->next;
		}
	}

	__declspec(dllexport) int32 __stdcall ScanForEcDevices(int32 num_network_adapter, uint8* buf)
	{
		ec_adaptert * ret_adapter;
		int32 slc;
		int32 count = 0;

		ret_adapter = oshw_find_adapters();

		while (count < num_network_adapter)
		{
			ret_adapter = ret_adapter->next;
			count++;
		}

		if (ec_init(ret_adapter->name))
		{
			if (ec_config_init(FALSE) > 0)
			{
				if ((ec_slavecount >= 1))
				{
					for (slc = 1; slc <= ec_slavecount; slc++)
					{
						strcat(strcat(buf, ec_slave[slc].name), "*");
					}
				}
			}
		}

		return ec_slavecount;
	}

	__declspec(dllexport) int32 __stdcall check_pdo_wkc(void)
	{
		return wkc_pdo;
	}

	__declspec(dllexport) uint16 __stdcall ec_sm_request_state_extern(int32 slave_number, ec_state req_state)
	{
		int32 chk;

		ec_slave[slave_number].state = req_state;
		/* request INIT state for slave */
		ec_writestate(slave_number);

		chk = 3;
		/* wait for slave to reach init state */
		do
		{
			ec_statecheck(slave_number, req_state, 50000);
		} while (chk-- && (ec_slave[slave_number].state != req_state));

		return ec_slave[slave_number].state;
	}

	__declspec(dllexport) uint16 __stdcall ec_sm_request_state_boot_extern(int32 slave_number)
	{
		int32 chk;

		ec_slave[slave_number].state = EC_STATE_BOOT;
		/* request INIT state for slave */
		ec_writestate(slave_number);

		chk = 500;
		/* wait for slave to reach init state */
		do
		{
			ec_statecheck(slave_number, EC_STATE_BOOT, 50000);
		} while (chk-- && (ec_slave[slave_number].state != EC_STATE_BOOT));

		return ec_slave[slave_number].state;
	}
	
	/* Deprecated !!! Multimedia Timer now set in .NET application */
	__declspec(dllexport) void __stdcall ec_sm_start_extern(void)
	{
		/* start RT thread as periodic MM timer */
		mmResult = timeSetEvent(50, 0, RTthread, 0, TIME_PERIODIC);
	}

	__declspec(dllexport) int32 __stdcall ec_connect_extern(uint16 slave_number, int32 network_adapter_number, uint8* buf)//, pdo_data_t* IOmap)
	{
		int32  count, slc;//, j;//, slc;oloop, iloop, chk,
		uint16 ret_state = 0;
		uint16 wd_time = 0xFFFF;

		needlf = FALSE;
		inOP = FALSE;

		ec_adaptert * ret_adapter;

		ret_adapter = oshw_find_adapters();

		uint8 ifname[1024];

		for (count = 0; count < network_adapter_number; count++)
		{
			ret_adapter = ret_adapter->next;
		}

		strcpy(ifname, ret_adapter->name);
		/* initialise SOEM, bind socket to ifname */
		if (ec_init(ifname))
		{
			/* find and auto-config slaves */
			if (ec_config_init(FALSE) > 0)
			{
				/* Configuration of Distributed clocks */
				//ec_configdc();
				//ec_dcsync0(1, TRUE, 4000000, 1200000);
				//ec_dcsync0(2, TRUE, 4000000, 1200000);
				if ((ec_slavecount >= 1))
				{
					for (slc = 1; slc <= ec_slavecount; slc++)
					{
						strcat(strcat(buf, ec_slave[slc].name), "*");
					}
				}

				for (slc = 1; slc <= ec_slavecount; slc++)
				{
					//ec_slave[slc].PO2SOconfig = &StartUpConfig;
					StartUpConfig(slc);//, config_tx_pdos, config_rx_pdos);
				}

				ec_config_map(IOmap);

				
				/* wait for slave to reach SAFE_OP state */
				ret_state = ec_statecheck(slave_number, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE);// 

				/*Set the watchdogs for process data to 6.5535 sec */
				ec_BWR(slave_number, ECT_REG_WD_TIMEPDI, sizeof(wd_time), &wd_time, EC_TIMEOUTRET);  /*  */
				ec_BWR(slave_number, ECT_REG_WD_TIMESM, sizeof(wd_time), &wd_time, EC_TIMEOUTRET);  /*  */

				if (ret_state == EC_STATE_SAFE_OP)
				{
					ec_send_processdata();
					ec_receive_processdata(EC_TIMEOUTRET);

					/* mmResult = timeSetEvent(1, 0, RTthread, 0, TIME_PERIODIC);/* Deprecated !! Multimedia Timer now set in .NET application */

					ec_sm_request_state_extern(0, EC_STATE_OPERATIONAL);
				}
				else
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
		else
		{
			//printf("No socket connection on %s\nExcecute as root\n", ifname);
		}

		return ec_slavecount;//ret_state;//(ret_state == EC_STATE_SAFE_OP);
	}

	__declspec(dllexport) void __stdcall ec_copy_pdos(uint16 slave_number, pdo_input_t* pdo_input, pdo_output_t pdo_output)
	{
		memcpy(pdo_input, ec_slave[slave_number].inputs, ec_slave[slave_number].Ibytes);
		memcpy(ec_slave[slave_number].outputs, &pdo_output, ec_slave[slave_number].Obytes);
	}

	__declspec(dllexport) int32 __stdcall ec_send_recieve_processdata_extern(void)
	{
		ec_send_processdata();
		wkc_pdo = ec_receive_processdata(EC_TIMEOUTRET);

		return wkc_pdo;
	}

	__declspec(dllexport) void ec_stop_rt_thread(void)
	{
		timeKillEvent(mmResult);
	}

	__declspec(dllexport) void ec_close_socket(void)
	{
		ec_close(); /* closes socket */
	}

	__declspec(dllexport) void ec_disconnect_extern(int32 slave_number)
	{
		/* stop RT thread */
		//timeKillEvent(mmResult);/* Deprecated !! Multimedia Timer now set in .NET application */

		/* Request init state for all slaves */
		ec_slave[slave_number].state = EC_STATE_INIT;
		/* request INIT state for all slaves */
		ec_writestate(slave_number);

		/* stop SOEM, close socket */
		ec_close();
	}

	__declspec(dllexport) void __stdcall SdoReadString(int32 sln, uint16 idx, uint8 subidx, uint8* buf, int32 size)
	{
        int __ret;

		__ret = ec_SDOread(sln, idx, subidx, FALSE, &size, buf, EC_TIMEOUTRXM);

	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteString(int32 sln, uint16 idx, uint8 subidx, uint8* value, int32 size)
	{
		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, size, value, 1000000);

		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) uint32 __stdcall SdoReadUInt32(int32 sln, uint16 idx, uint8 subidx)
	{
		uint32 buf32;

		SDO_READ(sln, idx, subidx, buf32);

		return buf32;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteUInt32(int32 sln, uint16 idx, uint8 subidx, uint32 value)
	{
		uint32 buf32 = value;
		ret_struct_t ret;
		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf32), &buf32, 1000000);

		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head-1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) int32 __stdcall SdoReadInt32(int32 sln, uint16 idx, uint8 subidx)
	{
		int32 buf32;

		SDO_READ(sln, idx, subidx, buf32);

		return buf32;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteInt32(int32 sln, uint16 idx, uint8 subidx, int32 value)
	{
		int32 buf32 = value;
		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf32), &buf32, 1000000);
		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) uint16 __stdcall SdoReadUInt16(int32 sln, uint16 idx, uint8 subidx)
	{
		uint16 buf16;

		SDO_READ(sln, idx, subidx, buf16);

		return buf16;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteUInt16(int32 sln, uint16 idx, uint8 subidx, uint16 value)
	{
		uint16 buf16 = value;

		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf16), &buf16, 1000000);
		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) int16 __stdcall SdoReadInt16(int32 sln, uint16 idx, uint8 subidx)
	{
		int16 buf16;

		SDO_READ(sln, idx, subidx, buf16);

		return buf16;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteInt16(int32 sln, uint16 idx, uint8 subidx, int16 value)
	{
		int16 buf16 = value;
		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf16), &buf16, 1000000);
		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) uint8 __stdcall SdoReadUInt8(int32 sln, uint16 idx, uint8 subidx)
	{
		uint8 buf8;

		SDO_READ(sln, idx, subidx, buf8);

		return buf8;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteUInt8(int32 sln, uint16 idx, uint8 subidx, uint8 value)
	{
		uint8 buf8 = value;

		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf8), &buf8, 1000000);
		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) int8 __stdcall SdoReadInt8(int32 sln, uint16 idx, uint8 subidx)
	{
		int8 buf8;

		SDO_READ(sln, idx, subidx, buf8);

		return buf8;
	}

	__declspec(dllexport) ret_struct_t __stdcall SdoWriteInt8(int32 sln, uint16 idx, uint8 subidx, int8 value)
	{
		int8 buf8 = value;
		ret_struct_t ret;

		ret.wkc = ec_SDOwrite(sln, idx, subidx, FALSE, sizeof(buf8), &buf8, 1000000);
		if (ret.wkc == 0)
		{
			ret.abort_code = ecx_context.elist->Error[ecx_context.elist->head - 1].AbortCode;
		}

		return ret;
	}

	__declspec(dllexport) int32 __stdcall foe_fw_update(int32 sln, uint8 *full_file_path, uint8 *filename,  uint32 password)
	{
		int32 j = 0;
		int32 filesize;

		/* wait for slave to reach BOOT state */
		if (ec_statecheck(sln, EC_STATE_BOOT, EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
		{
			if (input_bin(full_file_path, &filesize))
			{
				/* File read OK */
				j = ec_FOEwrite(sln, filename, password, filesize, &filebuffer, EC_TIMEOUTSTATE);
			}
			//else
				//printf("File not read OK.\n");
		}

		/*if FoE write successfull set the slave to Init to reboot device*/
		if (j >= 0)
		{
			ec_slave[sln].state = EC_STATE_INIT;
			ec_writestate(sln);
		}
		return j;
	}

#ifdef __cplusplus
}
#endif





