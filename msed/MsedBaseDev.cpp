/* C:B**************************************************************************
This software is Copyright 2014 Michael Romeo <r0m30@r0m30.com>

This file is part of msed.

msed is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

msed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with msed.  If not, see <http://www.gnu.org/licenses/>.

 * C:E********************************************************************** */
/** Base device class.
 * An OS port must create a subclass of this class
 * and implement the sendcmd class specific to the
 * IO requirements of that OS
 */
#include "os.h"
#include <stdio.h>
#include <iostream>
#include<iomanip>
#include "MsedBaseDev.h"
#include "MsedEndianFixup.h"
#include "MsedStructures.h"
#include "MsedCommand.h"
#include "MsedResponse.h"
#include "MsedHexDump.h"

using namespace std;

/** Device Class (Base) represents a single disk device.
 *  This is the functionality that is common to all OS's
 */
MsedBaseDev::MsedBaseDev()
{
}

MsedBaseDev::~MsedBaseDev()
{
}

uint8_t MsedBaseDev::isOpal2()
{
    LOG(D4) << "Entering MsedBaseDev::isOpal2()";
    return disk_info.OPAL20;
}

uint8_t MsedBaseDev::isOpal1()
{
    LOG(D4) << "Entering MsedBaseDev::isOpal1()";
    return disk_info.OPAL10;
}

uint8_t MsedBaseDev::isEprise()
{
    LOG(D4) << "Entering MsedBaseDev::isEprise";
    return disk_info.Enterprise;
}
uint8_t MsedBaseDev::isANYSSC()
{
	LOG(D4) << "Entering MsedBaseDev::isANYSSC";
	return disk_info.ANY_OPAL_SSC;
}
uint8_t MsedBaseDev::isPresent()
{
    LOG(D4) << "Entering MsedBaseDev::isPresent()";
    return isOpen;
}

void MsedBaseDev::getFirmwareRev(uint8_t bytes[8])
{
    memcpy(&bytes[0], &disk_info.firmwareRev[0], 8);
}

void MsedBaseDev::getModelNum(uint8_t bytes[40])
{
    memcpy(&bytes[0], &disk_info.modelNum[0], 40);
}

void MsedBaseDev::getSerialNum(uint8_t bytes[20])
{
    memcpy(&bytes[0], &disk_info.serialNum[0], 20);
}

uint16_t MsedBaseDev::comID()
{
    LOG(D4) << "Entering MsedBaseDev::comID()";
    if (disk_info.OPAL20)
        return disk_info.OPAL20_basecomID;
    else if (disk_info.OPAL10)
        return disk_info.OPAL10_basecomID;
    else if (disk_info.Enterprise)
        return disk_info.Enterprise_basecomID;
    else
        return 0x0000;
}

uint8_t MsedBaseDev::exec(MsedCommand * cmd, MsedResponse &response, uint8_t protocol)
{
    uint8_t rc = 0;
    OPALHeader * hdr = (OPALHeader *) cmd->getCmdBuffer();
    LOG(D3) << endl << "Dumping command buffer";
    IFLOG(D3) MsedHexDump(cmd->getCmdBuffer(), SWAP32(hdr->cp.length) + sizeof (OPALComPacket));
    rc = sendCmd(IF_SEND, protocol, comID(), cmd->getCmdBuffer(), IO_BUFFER_LENGTH);
    if (0 != rc) {
        LOG(E) << "Command failed on send " << (uint16_t) rc;
        return rc;
    }
    hdr = (OPALHeader *) cmd->getRespBuffer();
    do {
        //LOG(I) << "read loop";
        osmsSleep(25);
        memset(cmd->getRespBuffer(), 0, IO_BUFFER_LENGTH);
        rc = sendCmd(IF_RECV, protocol, comID(), cmd->getRespBuffer(), IO_BUFFER_LENGTH);

    }
    while ((0 != hdr->cp.outstandingData) && (0 == hdr->cp.minTransfer));
    LOG(D3) << std::endl << "Dumping reply buffer";
    IFLOG(D3) MsedHexDump(cmd->getRespBuffer(), SWAP32(hdr->cp.length) + sizeof (OPALComPacket));
    if (0 != rc) {
        LOG(E) << "Command failed on recv" << (uint16_t) rc;
        return rc;
    }
    response.init(cmd->getRespBuffer());
    return 0;
}

void MsedBaseDev::discovery0()
{
    LOG(D4) << "Entering MsedBaseDev::discovery0()";
    void * d0Response = NULL;
    uint8_t * epos, *cpos;
    Discovery0Header * hdr;
    Discovery0Features * body;
    d0Response = ALIGNED_ALLOC(4096, IO_BUFFER_LENGTH);
    if (NULL == d0Response) return;
    memset(d0Response, 0, IO_BUFFER_LENGTH);
    if (sendCmd(IF_RECV, 0x01, 0x0001, d0Response, IO_BUFFER_LENGTH)) {
        ALIGNED_FREE(d0Response);
        return;
    }

    epos = cpos = (uint8_t *) d0Response;
    hdr = (Discovery0Header *) d0Response;
    LOG(D3) << "Dumping D0Response";
    IFLOG(D3) MsedHexDump(hdr, SWAP32(hdr->length));
    epos = epos + SWAP32(hdr->length);
    cpos = cpos + 48; // TODO: check header version

    do {
        body = (Discovery0Features *) cpos;
        switch (SWAP16(body->TPer.featureCode)) { /* could use of the structures here is a common field */
        case FC_TPER: /* TPer */
            disk_info.TPer = 1;
            disk_info.TPer_ACKNACK = body->TPer.acknack;
            disk_info.TPer_async = body->TPer.async;
            disk_info.TPer_bufferMgt = body->TPer.bufferManagement;
            disk_info.TPer_comIDMgt = body->TPer.comIDManagement;
            disk_info.TPer_streaming = body->TPer.streaming;
            disk_info.TPer_sync = body->TPer.sync;
            break;
        case FC_LOCKING: /* Locking*/
            disk_info.Locking = 1;
            disk_info.Locking_locked = body->locking.locked;
            disk_info.Locking_lockingEnabled = body->locking.lockingEnabled;
            disk_info.Locking_lockingSupported = body->locking.lockingSupported;
            disk_info.Locking_MBRDone = body->locking.MBRDone;
            disk_info.Locking_MBREnabled = body->locking.MBREnabled;
            disk_info.Locking_mediaEncrypt = body->locking.mediaEncryption;
            break;
        case FC_GEOMETRY: /* Geometry Features */
            disk_info.Geometry = 1;
            disk_info.Geometry_align = body->geometry.align;
            disk_info.Geometry_alignmentGranularity = SWAP64(body->geometry.alignmentGranularity);
            disk_info.Geometry_logicalBlockSize = SWAP32(body->geometry.logicalBlockSize);
            disk_info.Geometry_lowestAlignedLBA = SWAP64(body->geometry.lowestAlighedLBA);
            break;
        case FC_ENTERPRISE: /* Enterprise SSC */
            disk_info.Enterprise = 1;
			disk_info.ANY_OPAL_SSC = 1;
            disk_info.Enterprise_rangeCrossing = body->enterpriseSSC.rangeCrossing;
            disk_info.Enterprise_basecomID = SWAP16(body->enterpriseSSC.baseComID);
            disk_info.Enterprise_numcomID = SWAP16(body->enterpriseSSC.numberComIDs);
            break;
        case FC_OPALV100: /* Opal V1 */
            disk_info.OPAL10 = 1;
			disk_info.ANY_OPAL_SSC = 1;
            disk_info.OPAL10_basecomID = SWAP16(body->opalv100.baseComID);
            disk_info.OPAL10_numcomIDs = SWAP16(body->opalv100.numberComIDs);
            break;
        case FC_SINGLEUSER: /* Single User Mode */
            disk_info.SingleUser = 1;
            disk_info.SingleUser_all = body->singleUserMode.all;
            disk_info.SingleUser_any = body->singleUserMode.any;
            disk_info.SingleUser_policy = body->singleUserMode.policy;
            disk_info.SingleUser_lockingObjects = SWAP32(body->singleUserMode.numberLockingObjects);
            break;
        case FC_DATASTORE: /* Datastore Tables */
            disk_info.DataStore = 1;
            disk_info.DataStore_maxTables = SWAP16(body->datastore.maxTables);
            disk_info.DataStore_maxTableSize = SWAP32(body->datastore.maxSizeTables);
            disk_info.DataStore_alignment = SWAP32(body->datastore.tableSizeAlignment);
            break;
        case FC_OPALV200: /* OPAL V200 */
            disk_info.OPAL20 = 1;
			disk_info.ANY_OPAL_SSC = 1;
            disk_info.OPAL20_basecomID = SWAP16(body->opalv200.baseCommID);
            disk_info.OPAL20_initialPIN = body->opalv200.initialPIN;
            disk_info.OPAL20_revertedPIN = body->opalv200.revertedPIN;
            disk_info.OPAL20_numcomIDs = SWAP16(body->opalv200.numCommIDs);
            disk_info.OPAL20_numAdmins = SWAP16(body->opalv200.numlockingAdminAuth);
            disk_info.OPAL20_numUsers = SWAP16(body->opalv200.numlockingUserAuth);
            disk_info.OPAL20_rangeCrossing = body->opalv200.rangeCrossing;
            break;
        default:
            disk_info.Unknown += 1;
            LOG(D) << "Unknown Feature in Discovery 0 response " << std::hex << SWAP16(body->TPer.featureCode) << std::dec;
            /* should do something here */
            break;
        }
        cpos = cpos + (body->TPer.length + 4);
    }
    while (cpos < epos);
    ALIGNED_FREE(d0Response);
}

/** Print out the Discovery 0 results */
void MsedBaseDev::puke()
{
    LOG(D4) << "Entering MsedBaseDev::puke()";
    /* IDENTIFY */
    cout << endl << dev << (disk_info.devType ? " OTHER " : " ATA ");
    for (int i = 0; i < sizeof (disk_info.modelNum); i++) {
        cout << disk_info.modelNum[i];
    }
    cout << " ";
    for (int i = 0; i < sizeof (disk_info.firmwareRev); i++) {
        if (0x20 == disk_info.firmwareRev[i]) break;
        cout << disk_info.firmwareRev[i];
    }
    cout << " ";
    for (int i = 0; i < sizeof (disk_info.serialNum); i++) {
        cout << disk_info.serialNum[i];
    }
    cout << endl;
    /* TPer */
    if (disk_info.TPer) {
        cout << "TPer function (" << HEXON(4) << FC_TPER << HEXOFF << ")" << std::endl;
        cout << "    ACKNAK = " << (disk_info.TPer_ACKNACK ? "Y, " : "N, ")
                << "ASYNC = " << (disk_info.TPer_async ? "Y, " : "N. ")
                << "BufferManagement = " << (disk_info.TPer_bufferMgt ? "Y, " : "N, ")
                << "comIDManagement  = " << (disk_info.TPer_comIDMgt ? "Y, " : "N, ")
                << "Streaming = " << (disk_info.TPer_streaming ? "Y, " : "N, ")
                << "SYNC = " << (disk_info.TPer_sync ? "Y" : "N")
                << std::endl;
    }
    if (disk_info.Locking) {

        cout << "Locking function (" << HEXON(4) << FC_LOCKING << HEXOFF << ")" << std::endl;
        cout << "    Locked = " << (disk_info.Locking_locked ? "Y, " : "N, ")
                << "LockingEnabled = " << (disk_info.Locking_lockingEnabled ? "Y, " : "N, ")
                << "LockingSupported = " << (disk_info.Locking_lockingSupported ? "Y, " : "N, ");
        cout << "MBRDone = " << (disk_info.Locking_MBRDone ? "Y, " : "N, ")
                << "MBREnabled = " << (disk_info.Locking_MBREnabled ? "Y, " : "N, ")
                << "MediaEncrypt = " << (disk_info.Locking_mediaEncrypt ? "Y" : "N")
                << std::endl;
    }
    if (disk_info.Geometry) {

        cout << "Geometry function (" << HEXON(4) << FC_GEOMETRY << HEXOFF << ")" << std::endl;
        cout << "    Align = " << (disk_info.Geometry_align ? "Y, " : "N, ")
                << "Alignment Granularity = " << disk_info.Geometry_alignmentGranularity
                << " (" << // display bytes
                (disk_info.Geometry_alignmentGranularity *
                disk_info.Geometry_logicalBlockSize)
                << ")"
                << ", Logical Block size = " << disk_info.Geometry_logicalBlockSize
                << ", Lowest Aligned LBA = " << disk_info.Geometry_lowestAlignedLBA
                << std::endl;
    }
    if (disk_info.Enterprise) {
        cout << "Enterprise function (" << HEXON(4) << FC_ENTERPRISE << HEXOFF << ")" << std::endl;
        cout << "    Range crossing = " << (disk_info.Enterprise_rangeCrossing ? "Y, " : "N, ")
                << "Base comID = " << disk_info.Enterprise_basecomID
                << ", comIDs = " << disk_info.Enterprise_numcomID
                << std::endl;
    }
    if (disk_info.OPAL10) {
        cout << "Opal V1.0 function (" << HEXON(4) << FC_OPALV100 << HEXOFF << ")" << std::endl;
        cout << "Base comID = " << disk_info.OPAL10_basecomID
                << ", comIDs = " << disk_info.OPAL10_numcomIDs
                << std::endl;
    }
    if (disk_info.SingleUser) {
        cout << "SingleUser function (" << HEXON(4) << FC_SINGLEUSER << HEXOFF << ")" << std::endl;
        cout << "    ALL = " << (disk_info.SingleUser_all ? "Y, " : "N, ")
                << "ANY = " << (disk_info.SingleUser_any ? "Y, " : "N, ")
                << "Policy = " << (disk_info.SingleUser_policy ? "Y, " : "N, ")
                << "Locking Objects = " << (disk_info.SingleUser_lockingObjects)
                << std::endl;
    }
    if (disk_info.DataStore) {
        cout << "DataStore function (" << HEXON(4) << FC_DATASTORE << HEXOFF << ")" << std::endl;
        cout << "    Max Tables = " << disk_info.DataStore_maxTables
                << ", Max Size Tables = " << disk_info.DataStore_maxTableSize
                << ", Table size alignment = " << disk_info.DataStore_alignment
                << std::endl;
    }

    if (disk_info.OPAL20) {
        cout << "OPAL 2.0 function (" << HEXON(4) << FC_OPALV200 << ")" << HEXOFF << std::endl;
        cout << "    Base comID = " << HEXON(4) << disk_info.OPAL20_basecomID << HEXOFF;
        cout << ", Initial PIN = " << HEXON(2) << disk_info.OPAL20_initialPIN << HEXOFF;
        cout << ", Reverted PIN = " << HEXON(2) << disk_info.OPAL20_revertedPIN << HEXOFF;
        cout << ", comIDs = " << disk_info.OPAL20_numcomIDs;
        cout << std::endl;
        cout << "    Locking Admins = " << disk_info.OPAL20_numAdmins;
        cout << ", Locking Users = " << disk_info.OPAL20_numUsers;
        cout << ", Range Crossing = " << (disk_info.OPAL20_rangeCrossing ? "Y" : "N");
        cout << std::endl;
    }
    if (disk_info.Unknown)
        cout << "**** " << (uint16_t) disk_info.Unknown << " **** Unknown function codes IGNORED " << std::endl;
}