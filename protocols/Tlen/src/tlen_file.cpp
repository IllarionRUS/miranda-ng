/*

Tlen Protocol Plugin for Miranda NG
Copyright (C) 2004-2007  Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "stdafx.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "tlen_list.h"
#include "tlen_p2p_old.h"

static void TlenFileReceiveParse(TLEN_FILE_TRANSFER *ft)
{
	int i;
	char *p;
	TLEN_FILE_PACKET *rpacket = nullptr, *packet;
	if (ft->state == FT_CONNECTING) {
		rpacket = TlenP2PPacketReceive(ft->s);
		if (rpacket != nullptr) {
			p = rpacket->packet;
			if (rpacket->type == TLEN_FILE_PACKET_FILE_LIST) { // list of files (length & name)
				ft->fileCount = (int)(*((DWORD*)p));
				ft->files = (char **)mir_alloc(sizeof(char *) * ft->fileCount);
				ft->filesSize = (long *)mir_alloc(sizeof(long) * ft->fileCount);
				ft->currentFile = 0;
				ft->allFileTotalSize = 0;
				ft->allFileReceivedBytes = 0;
				p += sizeof(DWORD);
				for (i = 0; i < ft->fileCount; i++) {
					ft->filesSize[i] = (long)(*((DWORD*)p));
					ft->allFileTotalSize += ft->filesSize[i];
					p += sizeof(DWORD);
					ft->files[i] = (char *)mir_alloc(256);
					memcpy(ft->files[i], p, 256);
					p += 256;
				}
				
				if ((packet = TlenP2PPacketCreate(3 * sizeof(DWORD))) == nullptr)
					ft->state = FT_ERROR;
				else {
					TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_LIST_ACK);
					TlenP2PPacketSend(ft->s, packet);
					TlenP2PPacketFree(packet);
					ft->state = FT_INITIALIZING;
					ft->proto->debugLogA("Change to FT_INITIALIZING");
				}
			}
			TlenP2PPacketFree(rpacket);
		}
		else ft->state = FT_ERROR;
	}
	else if (ft->state == FT_INITIALIZING) {
		char *fullFileName;
		if ((packet = TlenP2PPacketCreate(3 * sizeof(DWORD))) != nullptr) {
			TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_REQUEST); // file request
			TlenP2PPacketPackDword(packet, ft->currentFile);
			TlenP2PPacketPackDword(packet, 0);
			TlenP2PPacketPackDword(packet, 0);
			TlenP2PPacketSend(ft->s, packet);
			TlenP2PPacketFree(packet);

			fullFileName = (char *)mir_alloc(mir_strlen(ft->szSavePath) + mir_strlen(ft->files[ft->currentFile]) + 2);
			mir_strcpy(fullFileName, ft->szSavePath);
			if (fullFileName[mir_strlen(fullFileName) - 1] != '\\')
				mir_strcat(fullFileName, "\\");
			mir_strcat(fullFileName, ft->files[ft->currentFile]);
			ft->fileId = _open(fullFileName, _O_BINARY | _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
			ft->fileReceivedBytes = 0;
			ft->fileTotalSize = ft->filesSize[ft->currentFile];
			ft->proto->debugLogA("Saving to [%s] [%d]", fullFileName, ft->filesSize[ft->currentFile]);
			mir_free(fullFileName);
			ft->state = FT_RECEIVING;
			ft->proto->debugLogA("Change to FT_RECEIVING");
		}
		else ft->state = FT_ERROR;
	}
	else if (ft->state == FT_RECEIVING) {
		PROTOFILETRANSFERSTATUS pfts;
		memset(&pfts, 0, sizeof(PROTOFILETRANSFERSTATUS));
		pfts.cbSize = sizeof(PROTOFILETRANSFERSTATUS);
		pfts.hContact = ft->hContact;
		pfts.pszFiles = ft->files;
		pfts.totalFiles = ft->fileCount;
		pfts.currentFileNumber = ft->currentFile;
		pfts.totalBytes = ft->allFileTotalSize;
		pfts.szWorkingDir = nullptr;
		pfts.szCurrentFile = ft->files[ft->currentFile];
		pfts.currentFileSize = ft->filesSize[ft->currentFile];
		pfts.currentFileTime = 0;
		ft->proto->debugLogA("Receiving data...");
		while (ft->state == FT_RECEIVING) {
			rpacket = TlenP2PPacketReceive(ft->s);
			if (rpacket != nullptr) {
				p = rpacket->packet;
				if (rpacket->type == TLEN_FILE_PACKET_FILE_DATA) { // file data
					int writeSize;
					writeSize = rpacket->len - 2 * sizeof(DWORD); // skip file offset
					if (_write(ft->fileId, p + 2 * sizeof(DWORD), writeSize) != writeSize) {
						ft->state = FT_ERROR;
					}
					else {
						ft->fileReceivedBytes += writeSize;
						ft->allFileReceivedBytes += writeSize;
						pfts.totalProgress = ft->allFileReceivedBytes;
						pfts.currentFileProgress = ft->fileReceivedBytes;
						ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, (LPARAM)&pfts);
					}
				}
				else if (rpacket->type == TLEN_FILE_PACKET_END_OF_FILE) { // end of file
					_close(ft->fileId);
					ft->proto->debugLogA("Finishing this file...");
					if (ft->currentFile >= ft->fileCount - 1) {
						ft->state = FT_DONE;
					}
					else {
						ft->currentFile++;
						ft->state = FT_INITIALIZING;
						ft->proto->debugLogA("File received, advancing to the next file...");
						ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_NEXTFILE, ft, 0);
					}
				}
				TlenP2PPacketFree(rpacket);
			}
			else ft->state = FT_ERROR;
		}
	}
}

static void TlenFileReceivingConnection(HNETLIBCONN hConnection, DWORD, void * pExtra)
{
	TlenProtocol *proto = (TlenProtocol *)pExtra;
	TLEN_FILE_TRANSFER *ft = TlenP2PEstablishIncomingConnection(proto, hConnection, LIST_FILE, TRUE);
	if (ft != nullptr) {
		HNETLIBCONN slisten = ft->s;
		ft->s = hConnection;
		ft->proto->debugLogA("Set ft->s to %d (saving %d)", hConnection, slisten);
		ft->proto->debugLogA("Entering send loop for this file connection... (ft->s is hConnection)");
		while (ft->state != FT_DONE && ft->state != FT_ERROR)
			TlenFileReceiveParse(ft);

		if (ft->state == FT_DONE)
			ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		ft->proto->debugLogA("Closing connection for this file transfer... (ft->s is now hBind)");
		ft->s = slisten;
		ft->proto->debugLogA("ft->s is restored to %d", ft->s);
		
		if (ft->s != hConnection)
			Netlib_CloseHandle(hConnection);
		if (ft->hFileEvent != nullptr)
			SetEvent(ft->hFileEvent);
	}
	else Netlib_CloseHandle(hConnection);
}

static void __cdecl TlenFileReceiveThread(void *arg)
{
	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)arg;
	ft->proto->debugLogA("Thread started: type=file_receive server='%s' port='%d'", ft->hostName, ft->wPort);
	ft->mode = FT_RECV;

	NETLIBOPENCONNECTION nloc = { sizeof(nloc) };
	nloc.szHost = ft->hostName;
	nloc.wPort = ft->wPort;
	ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
	HNETLIBCONN s = Netlib_OpenConnection(ft->proto->m_hNetlibUser, &nloc);
	if (s != nullptr) {
		ft->s = s;
		ft->proto->debugLogA("Entering file receive loop");
		TlenP2PEstablishOutgoingConnection(ft, TRUE);
		while (ft->state != FT_DONE && ft->state != FT_ERROR)
			TlenFileReceiveParse(ft);

		if (ft->s) {
			Netlib_CloseHandle(s);
			ft->s = nullptr;
		}
	}
	else {
		ft->pfnNewConnectionV2 = TlenFileReceivingConnection;
		ft->proto->debugLogA("Connection failed - receiving as server");
		s = TlenP2PListen(ft);
		if (s != nullptr) {
			ft->s = s;
			HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			ft->hFileEvent = hEvent;
			ft->currentFile = 0;
			ft->state = FT_CONNECTING;
			char *nick = TlenNickFromJID(ft->jid);
			TlenSend(ft->proto, "<f t='%s' i='%s' e='7' a='%s' p='%d'/>", nick, ft->iqId, ft->localName, ft->wLocalPort);
			mir_free(nick);
			ft->proto->debugLogA("Waiting for the file to be received...");
			WaitForSingleObject(hEvent, INFINITE);
			ft->hFileEvent = nullptr;
			CloseHandle(hEvent);
			ft->proto->debugLogA("Finish all files");
			Netlib_CloseHandle(s);
		}
		else {
			ft->state = FT_ERROR;
		}
	}
	TlenListRemove(ft->proto, LIST_FILE, ft->iqId);
	if (ft->state == FT_DONE)
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
	else {
		char *nick = TlenNickFromJID(ft->jid);
		TlenSend(ft->proto, "<f t='%s' i='%s' e='8'/>", nick, ft->iqId);
		mir_free(nick);
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
	}

	ft->proto->debugLogA("Thread ended: type=file_receive server='%s'", ft->hostName);
	TlenP2PFreeFileTransfer(ft);
}



static void TlenFileSendParse(TLEN_FILE_TRANSFER *ft)
{
	int i;
	char *p, *t;
	int currentFile, numRead;
	char *fileBuffer;
	TLEN_FILE_PACKET *packet;

	if (ft->state == FT_CONNECTING) {
		char filename[256];	// Must be 256 (0x100)
		if ((packet = TlenP2PPacketCreate(sizeof(DWORD) + (ft->fileCount*(sizeof(filename) + sizeof(DWORD))))) != nullptr) {
			// Must pause a bit, sending these two packets back to back
			// will break the session because the receiver cannot take it :)
			SleepEx(1000, TRUE);
			TlenP2PPacketSetLen(packet, 0); // Reuse packet
			TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_LIST);
			TlenP2PPacketPackDword(packet, (DWORD)ft->fileCount);
			for (i = 0; i < ft->fileCount; i++) {
				//					struct _stat statbuf;
				//					_stat(ft->files[i], &statbuf);
				//					TlenP2PPacketPackDword(packet, statbuf.st_size);
				TlenP2PPacketPackDword(packet, ft->filesSize[i]);
				memset(filename, 0, sizeof(filename));
				if ((t = strrchr(ft->files[i], '\\')) != nullptr)
					t++;
				else
					t = ft->files[i];
				strncpy_s(filename, t, _TRUNCATE);
				TlenP2PPacketPackBuffer(packet, filename, sizeof(filename));
			}
			TlenP2PPacketSend(ft->s, packet);
			TlenP2PPacketFree(packet);

			ft->allFileReceivedBytes = 0;
			ft->state = FT_INITIALIZING;
			ft->proto->debugLogA("Change to FT_INITIALIZING");
		}
		else ft->state = FT_ERROR;
	}
	else if (ft->state == FT_INITIALIZING) {	// FT_INITIALIZING
		TLEN_FILE_PACKET *rpacket = TlenP2PPacketReceive(ft->s);
		ft->proto->debugLogA("FT_INITIALIZING: recv %d", rpacket);
		if (rpacket == nullptr) {
			ft->state = FT_ERROR;
			return;
		}
		ft->proto->debugLogA("FT_INITIALIZING: recv type %d", rpacket->type);
		p = rpacket->packet;
		// TYPE: TLEN_FILE_PACKET_FILE_LIST_ACK	will be ignored
		// LEN: 0
		/*if (rpacket->type == TLEN_FILE_PACKET_FILE_LIST_ACK) {

		}
		// Then the receiver will request each file
		// TYPE: TLEN_FILE_PACKET_REQUEST
		// LEN:
		// (DWORD) file number
		// (DWORD) 0
		// (DWORD) 0
		else */if (rpacket->type == TLEN_FILE_PACKET_FILE_REQUEST) {
			PROTOFILETRANSFERSTATUS pfts;
			//struct _stat statbuf;

			currentFile = *((DWORD*)p);
			if (currentFile != ft->currentFile) {
				ft->proto->debugLogA("Requested file (#%d) is invalid (must be %d)", currentFile, ft->currentFile);
				ft->state = FT_ERROR;
			}
			else {
				//	_stat(ft->files[currentFile], &statbuf);	// file size in statbuf.st_size
				ft->proto->debugLogA("Sending [%s] [%d]", ft->files[currentFile], ft->filesSize[currentFile]);
				if ((ft->fileId = _open(ft->files[currentFile], _O_BINARY | _O_RDONLY)) < 0) {
					ft->proto->debugLogA("File cannot be opened");
					ft->state = FT_ERROR;
				}
				else {
					memset(&pfts, 0, sizeof(PROTOFILETRANSFERSTATUS));
					pfts.cbSize = sizeof(PROTOFILETRANSFERSTATUS);
					pfts.hContact = ft->hContact;
					pfts.flags = PFTS_SENDING;
					pfts.pszFiles = ft->files;
					pfts.totalFiles = ft->fileCount;
					pfts.currentFileNumber = ft->currentFile;
					pfts.totalBytes = ft->allFileTotalSize;
					pfts.szWorkingDir = nullptr;
					pfts.szCurrentFile = ft->files[ft->currentFile];
					pfts.currentFileSize = ft->filesSize[ft->currentFile]; //statbuf.st_size;
					pfts.currentFileTime = 0;
					ft->fileReceivedBytes = 0;
					if ((packet = TlenP2PPacketCreate(2 * sizeof(DWORD) + 2048)) == nullptr) {
						ft->state = FT_ERROR;
					}
					else {
						TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_FILE_DATA);
						fileBuffer = (char *)mir_alloc(2048);
						ft->proto->debugLogA("Sending file data...");
						while ((numRead = _read(ft->fileId, fileBuffer, 2048)) > 0) {
							TlenP2PPacketSetLen(packet, 0); // Reuse packet
							TlenP2PPacketPackDword(packet, (DWORD)ft->fileReceivedBytes);
							TlenP2PPacketPackDword(packet, 0);
							TlenP2PPacketPackBuffer(packet, fileBuffer, numRead);
							if (TlenP2PPacketSend(ft->s, packet)) {
								ft->fileReceivedBytes += numRead;
								ft->allFileReceivedBytes += numRead;
								pfts.totalProgress = ft->allFileReceivedBytes;
								pfts.currentFileProgress = ft->fileReceivedBytes;
								ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, (LPARAM)&pfts);
							}
							else {
								ft->state = FT_ERROR;
								break;
							}
						}
						mir_free(fileBuffer);
						_close(ft->fileId);
						if (ft->state != FT_ERROR) {
							if (ft->currentFile >= ft->fileCount - 1)
								ft->state = FT_DONE;
							else {
								ft->currentFile++;
								ft->state = FT_INITIALIZING;
								ft->proto->debugLogA("File sent, advancing to the next file...");
								ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_NEXTFILE, ft, 0);
							}
						}
						ft->proto->debugLogA("Finishing this file...");
						TlenP2PPacketSetLen(packet, 0); // Reuse packet
						TlenP2PPacketSetType(packet, TLEN_FILE_PACKET_END_OF_FILE);
						TlenP2PPacketPackDword(packet, currentFile);
						TlenP2PPacketSend(ft->s, packet);
						TlenP2PPacketFree(packet);
					}
				}
			}
			TlenP2PPacketFree(rpacket);
		}
		else {
			TlenP2PPacketFree(rpacket);
			ft->state = FT_ERROR;
		}
	}
}

static void TlenFileSendingConnection(HNETLIBCONN hConnection, DWORD, void * pExtra)
{
	HNETLIBCONN slisten;
	TlenProtocol *proto = (TlenProtocol *)pExtra;

	TLEN_FILE_TRANSFER *ft = TlenP2PEstablishIncomingConnection(proto, hConnection, LIST_FILE, TRUE);
	if (ft != nullptr) {
		slisten = ft->s;
		ft->s = hConnection;
		ft->proto->debugLogA("Set ft->s to %d (saving %d)", hConnection, slisten);

		ft->proto->debugLogA("Entering send loop for this file connection... (ft->s is hConnection)");
		while (ft->state != FT_DONE && ft->state != FT_ERROR) {
			TlenFileSendParse(ft);
		}
		if (ft->state == FT_DONE)
			ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		else
			ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		ft->proto->debugLogA("Closing connection for this file transfer... (ft->s is now hBind)");
		ft->s = slisten;
		ft->proto->debugLogA("ft->s is restored to %d", ft->s);

		if (ft->s != hConnection)
			Netlib_CloseHandle(hConnection);
		if (ft->hFileEvent != nullptr)
			SetEvent(ft->hFileEvent);
	}
	else Netlib_CloseHandle(hConnection);
}

int TlenFileCancelAll(TlenProtocol *proto)
{
	HANDLE hEvent;
	int i = 0;

	while ((i = TlenListFindNext(proto, LIST_FILE, 0)) >= 0) {
		TLEN_LIST_ITEM *item = TlenListGetItemPtrFromIndex(proto, i);
		if (item != nullptr) {
			TLEN_FILE_TRANSFER *ft = item->ft;
			TlenListRemoveByIndex(proto, i);
			if (ft != nullptr) {
				if (ft->s) {
					//ProtoBroadcastAck(m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
					ft->proto->debugLogA("Closing ft->s = %d", ft->s);
					ft->state = FT_ERROR;
					Netlib_CloseHandle(ft->s);
					ft->s = nullptr;
					if (ft->hFileEvent != nullptr) {
						hEvent = ft->hFileEvent;
						ft->hFileEvent = nullptr;
						SetEvent(hEvent);
					}
				}
				else {
					ft->proto->debugLogA("freeing ft struct");
					TlenP2PFreeFileTransfer(ft);
				}
			}
		}
	}
	return 0;
}

static void __cdecl TlenFileSendingThread(void *arg)
{
	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)arg;
	char *nick;

	ft->proto->debugLogA("Thread started: type=tlen_file_send");
	ft->mode = FT_SEND;
	ft->pfnNewConnectionV2 = TlenFileSendingConnection;
	HNETLIBCONN s = TlenP2PListen(ft);
	if (s != nullptr) {
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
		ft->s = s;
		//TlenLog("ft->s = %d", s);
		//TlenLog("fileCount = %d", ft->fileCount);

		HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		ft->hFileEvent = hEvent;
		ft->currentFile = 0;
		ft->state = FT_CONNECTING;

		nick = TlenNickFromJID(ft->jid);
		TlenSend(ft->proto, "<f t='%s' i='%s' e='6' a='%s' p='%d'/>", nick, ft->iqId, ft->localName, ft->wLocalPort);
		mir_free(nick);
		ft->proto->debugLogA("Waiting for the file to be sent...");
		WaitForSingleObject(hEvent, INFINITE);
		ft->hFileEvent = nullptr;
		CloseHandle(hEvent);
		ft->proto->debugLogA("Finish all files");
		Netlib_CloseHandle(s);
		ft->s = nullptr;
		ft->proto->debugLogA("ft->s is NULL");

		if (ft->state == FT_SWITCH) {
			ft->proto->debugLogA("Sending as client...");
			ft->state = FT_CONNECTING;

			NETLIBOPENCONNECTION nloc = { sizeof(nloc) };
			nloc.szHost = ft->hostName;
			nloc.wPort = ft->wPort;
			HNETLIBCONN hConn = Netlib_OpenConnection(ft->proto->m_hNetlibUser, &nloc);
			if (hConn != nullptr) {
				ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_CONNECTING, ft, 0);
				ft->s = hConn;
				TlenP2PEstablishOutgoingConnection(ft, TRUE);
				ft->proto->debugLogA("Entering send loop for this file connection...");
				while (ft->state != FT_DONE && ft->state != FT_ERROR)
					TlenFileSendParse(ft);

				if (ft->state == FT_DONE)
					ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
				else
					ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
				ft->proto->debugLogA("Closing connection for this file transfer... ");
				Netlib_CloseHandle(hConn);
			}
			else ft->state = FT_ERROR;
		}
	}
	else {
		ft->proto->debugLogA("Cannot allocate port to bind for file server thread, thread ended.");
		ft->state = FT_ERROR;
	}
	TlenListRemove(ft->proto, LIST_FILE, ft->iqId);
	
	switch (ft->state) {
	case FT_DONE:
		ft->proto->debugLogA("Finish successfully");
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
		break;
	case FT_DENIED:
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_DENIED, ft, 0);
		break;
	default: // FT_ERROR:
		nick = TlenNickFromJID(ft->jid);
		TlenSend(ft->proto, "<f t='%s' i='%s' e='8'/>", nick, ft->iqId);
		mir_free(nick);
		ft->proto->debugLogA("Finish with errors");
		ProtoBroadcastAck(ft->proto->m_szModuleName, ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, ft, 0);
		break;
	}
	ft->proto->debugLogA("Thread ended: type=file_send");
	TlenP2PFreeFileTransfer(ft);
}


TLEN_FILE_TRANSFER *TlenFileCreateFT(TlenProtocol *proto, const char *jid)
{
	TLEN_FILE_TRANSFER *ft = (TLEN_FILE_TRANSFER *)mir_alloc(sizeof(TLEN_FILE_TRANSFER));
	memset(ft, 0, sizeof(TLEN_FILE_TRANSFER));
	ft->proto = proto;
	ft->jid = mir_strdup(jid);
	return ft;
}


/*
 * File transfer
 */
void TlenProcessF(XmlNode *node, ThreadData *info)
{
	char *p;
	char jid[128], szFilename[MAX_PATH];
	int numFiles;
	TLEN_LIST_ITEM *item;

	//	if (!node->name || mir_strcmp(node->name, "f")) return;
	if (info == nullptr) return;

	char *from = TlenXmlGetAttrValue(node, "f");
	if (from != nullptr) {
		if (strchr(from, '@') == nullptr)
			mir_snprintf(jid, "%s@%s", from, info->server);
		else
			strncpy_s(jid, from, _TRUNCATE);

		char *e = TlenXmlGetAttrValue(node, "e");
		if (e != nullptr) {
			if (!mir_strcmp(e, "1")) {
				// FILE_RECV : e='1' : File transfer request
				TLEN_FILE_TRANSFER *ft = TlenFileCreateFT(info->proto, jid);
				ft->hContact = TlenHContactFromJID(info->proto, jid);

				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr)
					ft->iqId = mir_strdup(p);

				szFilename[0] = '\0';
				if ((p = TlenXmlGetAttrValue(node, "c")) != nullptr) {
					numFiles = atoi(p);
					if (numFiles == 1) {
						if ((p = TlenXmlGetAttrValue(node, "n")) != nullptr) {
							p = TlenTextDecode(p);
							strncpy(szFilename, p, sizeof(szFilename) - 1);
							mir_free(p);
						}
						else mir_strcpy(szFilename, Translate("1 File"));
					}
					else if (numFiles > 1)
						mir_snprintf(szFilename, Translate("%d Files"), numFiles);
				}

				if (szFilename[0] != '\0' && ft->iqId != nullptr) {
					wchar_t* filenameT = mir_utf8decodeW((char*)szFilename);
					PROTORECVFILET pre = { 0 };
					pre.dwFlags = PRFF_UNICODE;
					pre.fileCount = 1;
					pre.timestamp = time(nullptr);
					pre.descr.w = filenameT;
					pre.files.w = &filenameT;
					pre.lParam = (LPARAM)ft;
					ft->proto->debugLogA("sending chainrecv");
					ProtoChainRecvFile(ft->hContact, &pre);
					mir_free(filenameT);
				}
				else {
					// malformed <f/> request, reject
					if (ft->iqId)
						TlenSend(ft->proto, "<f i='%s' e='4' t='%s'/>", ft->iqId, from);
					else
						TlenSend(ft->proto, "<f e='4' t='%s'/>", from);
					TlenP2PFreeFileTransfer(ft);
				}
			}
			else if (!mir_strcmp(e, "3")) {
				// FILE_RECV : e='3' : invalid transfer error
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr) {
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr) {
						if (item->ft != nullptr) {
							ProtoBroadcastAck(info->proto->m_szModuleName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, item->ft, 0);
							info->proto->FileCancel(NULL, item->ft);
						}
						TlenListRemove(info->proto, LIST_FILE, p);
					}
				}
			}
			else if (!mir_strcmp(e, "4")) {
				// FILE_SEND : e='4' : File sending request was denied by the remote client
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr) {
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr) {
						if (!mir_strcmp(item->ft->jid, jid)) {
							ProtoBroadcastAck(info->proto->m_szModuleName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_DENIED, item->ft, 0);
							TlenListRemove(info->proto, LIST_FILE, p);
						}
					}
				}
			}
			else if (!mir_strcmp(e, "5")) {
				// FILE_SEND : e='5' : File sending request was accepted
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr)
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr)
						if (!mir_strcmp(item->ft->jid, jid))
							mir_forkthread(TlenFileSendingThread, item->ft);
			}
			else if (!mir_strcmp(e, "6")) {
				// FILE_RECV : e='6' : IP and port information to connect to get file
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr) {
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr) {
						if ((p = TlenXmlGetAttrValue(node, "a")) != nullptr) {
							item->ft->hostName = mir_strdup(p);
							if ((p = TlenXmlGetAttrValue(node, "p")) != nullptr) {
								item->ft->wPort = atoi(p);
								mir_forkthread(TlenFileReceiveThread, item->ft);
							}
						}
					}
				}
			}
			else if (!mir_strcmp(e, "7")) {
				// FILE_RECV : e='7' : IP and port information to connect to send file
				// in case the conection to the given server was not successful
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr) {
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr) {
						if ((p = TlenXmlGetAttrValue(node, "a")) != nullptr) {
							if (item->ft->hostName != nullptr) mir_free(item->ft->hostName);
							item->ft->hostName = mir_strdup(p);
							if ((p = TlenXmlGetAttrValue(node, "p")) != nullptr) {
								item->ft->wPort = atoi(p);
								item->ft->state = FT_SWITCH;
								SetEvent(item->ft->hFileEvent);
							}
						}
					}
				}
			}
			else if (!mir_strcmp(e, "8")) {
				// FILE_RECV : e='8' : transfer error
				if ((p = TlenXmlGetAttrValue(node, "i")) != nullptr) {
					if ((item = TlenListGetItemPtr(info->proto, LIST_FILE, p)) != nullptr) {
						item->ft->state = FT_ERROR;
						if (item->ft->hFileEvent != nullptr)
							SetEvent(item->ft->hFileEvent);
						else
							ProtoBroadcastAck(info->proto->m_szModuleName, item->ft->hContact, ACKTYPE_FILE, ACKRESULT_FAILED, item->ft, 0);
					}
				}
			}
		}
	}
}
