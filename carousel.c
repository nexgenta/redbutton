/*
 * carousel.c
 */

/*
 * Copyright (C) 2005, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "carousel.h"
#include "table.h"
#include "dsmcc.h"
#include "biop.h"
#include "utils.h"

void
load_carousel(struct carousel *car)
{
	unsigned char *table;
	bool done;

	/* no modules yet */
	car->nmodules = 0;
	car->modules = NULL;

	/* see what the next DSMCC table is */
	done = false;
	do
	{
		struct dsmccMessageHeader *dsmcc;
		if((table = read_dsmcc_tables(car)) == NULL)
			fatal("Unable to read PID");
		dsmcc = (struct dsmccMessageHeader *) &table[8];
		if(dsmcc->protocolDiscriminator == DSMCC_PROTOCOL
		&& dsmcc->dsmccType == DSMCC_TYPE_DOWNLOAD)
		{
			if(ntohs(dsmcc->messageId) == DSMCC_MSGID_DII)
				process_dii(car, (struct DownloadInfoIndication *) dsmccMessage(dsmcc), ntohl(dsmcc->transactionId));
			else if(ntohs(dsmcc->messageId) == DSMCC_MSGID_DSI)
				process_dsi(car, (struct DownloadServerInitiate *) dsmccMessage(dsmcc));
			else if(ntohs(dsmcc->messageId) == DSMCC_MSGID_DDB)
				process_ddb(car, (struct DownloadDataBlock *) dsmccMessage(dsmcc), ntohl(dsmcc->transactionId), DDB_blockDataLength(dsmcc));
			else
				printf("Unknown DSMCC messageId: 0x%x\n", ntohs(dsmcc->messageId));
		}
	}
	while(!done);

	return;
}

void
process_dii(struct carousel *car, struct DownloadInfoIndication *dii, uint32_t transactionId)
{
	unsigned int nmodules;
	unsigned int i;

	printf("DownloadInfoIndication\n");
//	printf("transactionId: %u\n", transactionId);
//	printf("downloadId: %u\n", ntohl(dii->downloadId));

	nmodules = DII_numberOfModules(dii);
//	printf("numberOfModules: %u\n", nmodules);

	for(i=0; i<nmodules; i++)
	{
		struct DIIModule *mod;
		mod = DII_module(dii, i);
//		printf("Module %u\n", i);
//		printf(" moduleId: %u\n", ntohs(mod->moduleId));
//		printf(" moduleVersion: %u\n", mod->moduleVersion);
//		printf(" moduleSize: %u\n", ntohl(mod->moduleSize));
		if(find_module(car, ntohs(mod->moduleId), mod->moduleVersion, ntohl(dii->downloadId)) == NULL)
			add_module(car, dii, mod);
	}

//	printf("\n");

	return;
}

void
process_dsi(struct carousel *car, struct DownloadServerInitiate *dsi)
{
	uint16_t elementary_pid;

	printf("DownloadServerInitiate\n");

	/*
	 * BBC1 (for example) just broadcasts a DSI
	 * the DSI points to a carousel on the BBCi elementary_pid
	 * so, to access the carousel for BBC1 we have to read from the BBCi PID
	 * but, when we read the BBCi PID we will download the BBCi DSI
	 * we don't want the BBCi DSI to overwrite our original BBC1 DSI
	 * so only download the first DSI we find (ie before we read from to BBCi)
	 */
	if(car->got_dsi)
		return;

	car->got_dsi = true;

	elementary_pid = process_biop_service_gateway_info(car->service_id, &car->assoc, DSI_privateDataByte(dsi), ntohs(dsi->privateDataLength));

	/* make sure we are downloading data from the PID the DSI refers to */
	add_dsmcc_pid(car, elementary_pid);

//	printf("\n");

	return;
}

void
process_ddb(struct carousel *car, struct DownloadDataBlock *ddb, uint32_t downloadId, uint32_t blockLength)
{
	unsigned char *block;
	struct module *mod;

	printf("DownloadDataBlock\n");
//	printf("downloadId: %u\n", downloadId);

//	printf("moduleId: %u\n", ntohs(ddb->moduleId));
//	printf("moduleVersion: %u\n", ddb->moduleVersion);
//	printf("blockNumber: %u\n", ntohs(ddb->blockNumber));

//	printf("blockLength: %u\n", blockLength);
	block = DDB_blockDataByte(ddb);
//	hexdump(block, blockLength);

	if((mod = find_module(car, ntohs(ddb->moduleId), ddb->moduleVersion, downloadId)) != NULL)
		download_block(car, mod, ntohs(ddb->blockNumber), block, blockLength);

//	printf("\n");

	return;
}

