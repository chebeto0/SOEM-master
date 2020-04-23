/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage: firm_update ifname1 slave fname
 * ifname is NIC interface, f.e. eth0
 * slave = slave number in EtherCAT order 1..n
 * fname = binary file to store in slave
 * CAUTION! Using the wrong file can result in a bricked slave!
 *
 * This is a slave firmware update test.
 *
 * (c)Arthur Ketels 2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/time.h>
//#include <unistd.h>
#include "oshw.h"
#include "ethercat.h"

#define FWBUFSIZE (8 * 1024 * 1024)

char IOmap[4096];
uint8 ob;
uint16 ow;
uint32 data;
char filename[256];
char filebuffer[FWBUFSIZE]; // 8MB buffer
int filesize;
int j;
uint16 argslave;

int input_bin(char *fname, int *length)
{
    FILE *fp;

	int cc = 0, c;

    fp = fopen(fname, "rb");
    if(fp == NULL)
        return 0;
	while (((c = fgetc(fp)) != EOF) && (cc < FWBUFSIZE))
		filebuffer[cc++] = (uint8)c;
	*length = cc;
	fclose(fp);
	return 1;
}

int StartUpConfig(uint16 sln)
{
	//int retval;
	//uint8 u8val;
	////uint16 u16val;

	//retval = 0;

	//u8val = 0;

	//uint32 buf32;
	//int32 sbuf32;
	//uint16 buf16;
	//uint8 buf8;

	//READ(sln, 0x1000, 0, buf32);

	//if (buf32 != 5001)
	//{
	//	WRITE(sln, 0x1c12, 0, buf8, 0);
	//	WRITE(sln, 0x1c13, 0, buf8, 0);

	//	WRITE(sln, 0x1c12, 1, buf16, 0x1600);
	//	WRITE(sln, 0x1c12, 0, buf8, 1);
	//	WRITE(sln, 0x1c13, 1, buf16, 0x1a00);
	//	WRITE(sln, 0x1c13, 0, buf8, 1);

	//	READ(sln, 0x1c12, 0, buf32);
	//	READ(sln, 0x1c13, 0, buf32);

	//	READ(sln, 0x1c12, 1, buf32);
	//	READ(sln, 0x1c13, 1, buf32);

	//	READ(sln, 0x1c12, 0, buf32);
	//	READ(sln, 0x1c13, 0, buf32);

	//	READ(sln, 0x1c12, 1, buf32);
	//	READ(sln, 0x1c13, 1, buf32);

	//	//WRITE(sln, 0x1c12, 0, buf8, 1);
	//	//WRITE(sln, 0x1c13, 0, buf8, 1);

	//	/*printf("\nSlave:%d\n Name:%s\n Output size: %dbits\n Input size: %dbits\n State: %d\n Delay: %d[ns]\n Has DC: %d\n",
	//	1, ec_slave[1].name, ec_slave[1].Obits, ec_slave[1].Ibits,
	//	ec_slave[1].state, ec_slave[1].pdelay, ec_slave[1].hasdc);*/
	//}
	ec_slave[sln].CoEdetails &= ECT_COEDET_SDOCA;

	//uint32 vendorID;

	//READ(sln, 0x1018, 1, buf32);

	//vendorID = buf32;

	//if (vendorID == 599) // Dunker Motor
	//{
	//	WRITE(sln, 0x4010, 12, buf8, 32); //EventOnError_cmd1

	//	WRITE(sln, 0x42A0, 11, buf8, 1); //CurrentActual Filter Level

	//	WRITE(sln, 0x6065, 0, buf32, 4096);

	//	WRITE(sln, 0x6067, 0, sbuf32, -50); //Position window

	//	WRITE(sln, 0x6068, 0, buf16, 15); //Position window

	//	WRITE(sln, 0x608a, 0, buf8, 172);
	//}

	return 1;
}

void boottest(char *ifname, uint16 slave, char *filename)
{
	printf("Starting firmware update example\n");

	/* initialise SOEM, bind socket to ifname */
	if (ec_init(ifname))
	{
		printf("ec_init on %s succeeded.\n",ifname);
		/* find and auto-config slaves */


	    if ( ec_config_init(FALSE) > 0 )
		{
			printf("%d slaves found and configured.\n",ec_slavecount);


			ec_slave[slave].PO2SOconfig = &StartUpConfig;

			StartUpConfig(slave);
			ec_config_map(&IOmap);
			ec_configdc();

			ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);// !!!!!!!!!!!!!!!!!!!!!!!!!

			printf("Request init state for slave %d\n", slave);
			ec_slave[slave].state = EC_STATE_INIT;
			ec_writestate(slave);

			/* wait for slave to reach INIT state */
			ec_statecheck(slave, EC_STATE_INIT,  EC_TIMEOUTSTATE * 4);
			printf("Slave %d state to INIT.\n", slave);

			///* read BOOT mailbox data, master -> slave */
			//data = ec_readeeprom(slave, ECT_SII_BOOTRXMBX, EC_TIMEOUTEEP);
			//ec_slave[slave].SM[0].StartAddr = (uint16)LO_WORD(data);
   //         		ec_slave[slave].SM[0].SMlength = (uint16)HI_WORD(data);
			///* store boot write mailbox address */
			//ec_slave[slave].mbx_wo = (uint16)LO_WORD(data);
			///* store boot write mailbox size */
			//ec_slave[slave].mbx_l = (uint16)HI_WORD(data);

			///* read BOOT mailbox data, slave -> master */
			//data = ec_readeeprom(slave, ECT_SII_BOOTTXMBX, EC_TIMEOUTEEP);
			//ec_slave[slave].SM[1].StartAddr = (uint16)LO_WORD(data);
   //                     ec_slave[slave].SM[1].SMlength = (uint16)HI_WORD(data);
			///* store boot read mailbox address */
			//ec_slave[slave].mbx_ro = (uint16)LO_WORD(data);
			///* store boot read mailbox size */
			//ec_slave[slave].mbx_rl = (uint16)HI_WORD(data);

			//printf(" SM0 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[0].StartAddr, ec_slave[slave].SM[0].SMlength,
			//    (int)ec_slave[slave].SM[0].SMflags);
			//printf(" SM1 A:%4.4x L:%4d F:%8.8x\n", ec_slave[slave].SM[1].StartAddr, ec_slave[slave].SM[1].SMlength,
			//    (int)ec_slave[slave].SM[1].SMflags);
			///* program SM0 mailbox in for slave */
			//ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM0, sizeof(ec_smt), &ec_slave[slave].SM[0], EC_TIMEOUTRET);
			///* program SM1 mailbox out for slave */
			//ec_FPWR (ec_slave[slave].configadr, ECT_REG_SM1, sizeof(ec_smt), &ec_slave[slave].SM[1], EC_TIMEOUTRET);

			printf("Request BOOT state for slave %d\n", slave);
			ec_slave[slave].state = EC_STATE_BOOT;
			ec_writestate(slave);

			/* wait for slave to reach BOOT state */
			if (ec_statecheck(slave, EC_STATE_BOOT,  EC_TIMEOUTSTATE * 10) == EC_STATE_BOOT)
			{
				printf("Slave %d state to BOOT.\n", slave);

				if (input_bin(filename, &filesize))
				{
					printf("File read OK, %d bytes.\n",filesize);
					printf("FoE write....");
					j = ec_FOEwrite(slave, filename, 0xBEEFBEEF, filesize , &filebuffer, EC_TIMEOUTSTATE);
					printf("result %d.\n",j);
					printf("Request init state for slave %d\n", slave);
					ec_slave[slave].state = EC_STATE_INIT;
					ec_writestate(slave);
				}
				else
				{
					printf("File not read OK.\n");
					printf(" %d bytes.\n", filesize);
				}
				   
			}

		}
		else
		{
			printf("No slaves found!\n");
		}
		printf("End firmware update example, close socket\n");
		/* stop SOEM, close socket */
		ec_close();
	}
	else
	{
		printf("No socket connection on %s\nExcecute as root\n",ifname);
	}
}

int main(int argc, char *argv[])
{
	printf("SOEM (Simple Open EtherCAT Master)\nFirmware update example\n");

	char *filename = "C:\\Users\\garcia\\Desktop\\firmware_crc.bin";
	//char *filename = "C:\\Workspaces\\DAVE - 4.3 - 64Bit\\WS_2016_09_02\\ETHCAT_FWUPDATE_SSC_APPLICATION_XMC48\\Debug\\firmware_crc.bin";
	//char *filename = "C:\\Users\\garcia\\Desktop\\firmware_crc_test.bin";

	ec_adaptert * ret_adapter;

	ret_adapter = oshw_find_adapters();

	char ifname[1024];

	strcpy(ifname, ret_adapter->name);

	//if (argc > 3)
	//{
		//argslave = atoi(argv[2]);
		//boottest(argv[1], argslave, argv[3]);
	boottest(ifname, 1, filename);
	//}
	//else
	//{
		//printf("Usage: firm_update ifname1 slave fname\n");
		//printf("ifname = eth0 for example\n");
		//printf("slave = slave number in EtherCAT order 1..n\n");
		//printf("fname = binary file to store in slave\n");
		//printf("CAUTION! Using the wrong file can result in a bricked slave!\n");
	//}
	
	printf("End program\n");
	system("pause");
	return (0);
}
