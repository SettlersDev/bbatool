// bbaTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <direct.h>
#include <windows.h>

#include "zlib/zlib.h"
#include "shokCrypt.h"
#include "compress.h"
#include "tinydir.h"
#include "shokData.h"

#include <eh.h>
#include "CrashHandler.h"
extern "C" {
	static int fullVersionKey = 12829762;

#define BUFSZ	4096

	static HANDLE conHandle = NULL;
	static bool demoVersion = false;

	static void mkpath(char *path)
	{
		for (char *p = path; *p != 0; p++) //subdirs
		{
			if (*p == '\\')
			{
				*p = 0;
				_mkdir(path);
				*p = '\\';
			}
		}

		_mkdir(path);
	}

	void gotoxy(COORD p)
	{
		SetConsoleCursorPosition(conHandle, p);
	}

	COORD getCurPos()
	{
		_CONSOLE_SCREEN_BUFFER_INFO  curInfo;
		GetConsoleScreenBufferInfo(conHandle, &curInfo);
		return curInfo.dwCursorPosition;
	}

	void ShowConsoleCursor(BOOL bShow)
	{

		CONSOLE_CURSOR_INFO     cursorInfo;

		cursorInfo.dwSize = 10;
		cursorInfo.bVisible = bShow;

		SetConsoleCursorInfo(conHandle, &cursorInfo);

	}

	uint32_t htSize(uint32_t entries)
	{
		uint32_t counter = entries + 1;
		uint32_t pot = 0;
		do
		{
			counter >>= 1;
			pot++;
		} while (counter);

		uint32_t size = (1 << pot);
		if ((double)entries * 1.5 > (double)size)
			return size * 4;
		else
			return size * 2;
	}

	uint32_t shokHash(char *str)
	{
		if (!(*str))
			return 1;

		uint32_t num = 0;
		do {
			num = *str + 16 * num;
			if (num & 0xF0000000)
				num ^= num & 0xF0000000 ^ ((num & 0xF0000000) >> 24);
			str++;
		} while (*str);

		if (num)
			return 1812433253 * (num >> 16) - 1989869568 * num;
		else
			return 1;
	}

	void printHT()
	{
		FileHeader inputHeader;
		DirectoryHeader dirHeader;
		HashTableHeader hashHeader;

		printf("Bitte Dateinamen eingeben:\n");
		char infileName[300];
		scanf("%s", infileName);

		FILE *handle = fopen(infileName, "rb");
		char *path = (char *)malloc(strlen(infileName) + 9);
		path[0] = 0;
		_chdir(path);

		fread(&inputHeader, sizeof(FileHeader), 1, handle);

		fseek(handle, inputHeader.fileDataLength, SEEK_CUR);

		fread(&dirHeader, sizeof(DirectoryHeader), 1, handle);

		fseek(handle, dirHeader.fileEntriesLength - 20, SEEK_CUR);


		fread(&hashHeader, sizeof(HashTableHeader), 1, handle);
		printf("%s\n", hashHeader.BAh_Header);
		HashTableElement *hashTable = (HashTableElement *)malloc(hashHeader.hashTableSize * sizeof(HashTableElement));
		fread(hashTable, sizeof(HashTableElement), hashHeader.hashTableSize, handle);

		//http://www.cs.rmit.edu.au/online/blackboard/chapter/05/documents/contribute/chapter/05/linear-probing.html
		printf("HT-Size:  %d\n", hashHeader.hashTableSize);
		uint32_t mask = hashHeader.hashTableSize - 1;
		printf("Mask: 0x%8X\n\n", mask);
		//printf("12345678  12345678  12345678  12345678");
		printf("TableElm  Masked    Hash      Offset\n");
		for (uint32_t i = 0; i < hashHeader.hashTableSize; i++)
			printf("%08X  %08X  %08X  %08X\n", i, hashTable[i].hash & mask, hashTable[i].hash, hashTable[i].BAeOffset);
	}

	DirStructEntry *readFolder(char *dirname, int *counter, int *strLenCnt, DirStructEntry **lastElm)
	{
		tinydir_dir dir;
		tinydir_open(&dir, dirname);
		DirStructEntry* firstEntry = 0;
		DirStructEntry* lastElement = 0;
		DirStructEntry *lastLinear = 0;
		while (dir.has_next)
		{
			tinydir_file file;
			tinydir_readfile(&dir, &file);
			if (file.name[0] != '.')
			{
				(*counter)++;
				DirStructEntry *entry;
				entry = (DirStructEntry*)calloc(sizeof(DirStructEntry)-2 + strlen(file.path) + 1, 1);

				if (lastLinear)
				{
					lastLinear->nextLinear = entry;
					entry->prevLinear = lastLinear;
				}

				if (file.is_dir)
				{
					entry->firstChild = readFolder(file.path, counter, strLenCnt, &lastLinear);

					if (entry->firstChild)
					{
						entry->nextLinear = entry->firstChild;
						entry->firstChild->prevLinear = entry;
					}
					else
						lastLinear = entry;
				}
				else
				{
					entry->firstChild = 0;
					entry->nextLinear = 0;
					lastLinear = entry;
				}



				entry->type = file.is_dir ? ELM_DIR : ELM_UNCOMPRESSED;

				int len = 0;
				int fileExt = 0;
				for (; char c = file.path[2 + len]; len++)
				{
					entry->path[len] = c == '/' ? '\\' : c;
					if (c == '.')
						fileExt = len + 1;
				}

				entry->path[len] = 0;

				if (!file.is_dir)
				{
					if (!strcmp("xml", &entry->path[fileExt]) ||
						!strcmp("bin", &entry->path[fileExt]) ||
						!strcmp("fdb", &entry->path[fileExt]) ||
						!strcmp("fx", &entry->path[fileExt]) ||
						!strcmp("txt", &entry->path[fileExt]))
						entry->type = ELM_COMPRESSED;
					else if (!strcmp("lua", &entry->path[fileExt]))
						if (demoVersion)
							entry->type = ELM_LOLCRYPT;
						else
							entry->type = ELM_COMPRESSED;
				}

				int pathLen = len;
				int padding = 4 - (pathLen % 4);
				(*strLenCnt) += pathLen + padding;

				if (firstEntry == 0)
					firstEntry = entry;
				else
					lastElement->nextSibling = entry;

				lastElement = entry;
			}
			tinydir_next(&dir);
		}
		tinydir_close(&dir);

		if (lastElement)
			lastElement->nextSibling = 0;

		*lastElm = lastLinear;

		return firstEntry;
	}

	bool packfile(char *folderName)
	{
		uint32_t msStart = GetTickCount();
		printf("Command: Packing folder to archive\n\n");
		DirStructEntry *lastElement = 0;
		int cnt = 1;
		int allStringSize = 4;
		int allFileSize = 0;

		int dotPos = strlen(folderName) - 9;
		folderName[dotPos] = 0;
		FILE *writeHandle = fopen(folderName, "wb");
		if (!writeHandle)
		{
			printf("Output file couldn't be created!\n");
			return false;
		}

		folderName[dotPos] = '.';
		_chdir(folderName);
		DirStructEntry root;
		root.type = ELM_DIR;
		root.path[0] = '.';
		root.path[1] = 0;
		root.nextSibling = 0;
		printf(" - Reading folder contents\n");
		root.nextLinear = 0;
		root.prevLinear = 0;
		root.firstChild = readFolder(".", &cnt, &allStringSize, &lastElement);
		if (root.firstChild)
		{
			root.nextLinear = root.firstChild;
			root.firstChild->prevLinear = &root;
			root.prevLinear = 0;
		}


		DirStructEntry *elm = &root;
		fseek(writeHandle, sizeof(FileHeader), SEEK_SET);
		int fileBlockSize = 0;

		char copyBuf[BUFSZ];
		printf(" - Writing file contents: ");
		COORD curPos = getCurPos();
		int fileNum = 0;
		do
		{
			fileNum++;
			gotoxy(curPos);
			printf("%d%%", (100 * fileNum) / cnt);

			if (elm->type != ELM_DIR)
			{
				fgetpos(writeHandle, (fpos_t*)&elm->offset);
				FILE *readHandle = fopen(elm->path, "rb");
				if (!readHandle)
				{
					printf("Error: Couldn't read '%s'!\n", elm->path);
					return false;
				}

				int fileSize = 0;

				if (elm->type == ELM_UNCOMPRESSED)
				{
					int bytesRead;
					do
					{
						bytesRead = fread(copyBuf, 1, BUFSZ, readHandle);
						fwrite(copyBuf, 1, bytesRead, writeHandle);
						fileSize += bytesRead;
					} while (bytesRead == BUFSZ);
					elm->filesize = fileSize;
					allFileSize += fileSize;
				}
				else
				{
					CompressedFileHeader cmpHeader;

					fseek(writeHandle, sizeof(CompressedFileHeader), SEEK_CUR);
					def(readHandle, writeHandle, Z_DEFAULT_COMPRESSION, &cmpHeader.compressedSize, &cmpHeader.decompressedSize, &cmpHeader.adler32, elm->type == ELM_LOLCRYPT);
					fseek(writeHandle, elm->offset, SEEK_SET);

					cmpHeader.compressionHeader = 0x0637f2bd;
					cmpHeader.dataLength = cmpHeader.compressedSize + 12;

					fwrite(&cmpHeader, sizeof(CompressedFileHeader), 1, writeHandle);
					fseek(writeHandle, cmpHeader.compressedSize, SEEK_CUR);

					elm->filesize = cmpHeader.decompressedSize;
					allFileSize += cmpHeader.decompressedSize;
					fileSize = cmpHeader.dataLength + 8;
				}

				fclose(readHandle);
				fileBlockSize += fileSize;
			}
		} while (elm = elm->nextLinear);

		printf("\n - Creating directory\n");
		int uncompressedDirSize = (sizeof(DirectoryEntry)-4) * cnt + allStringSize + 4;
		uint8_t *directory = (uint8_t*)calloc(uncompressedDirSize, 1);
		elm = lastElement ? lastElement : &root;
		((uint32_t*)directory)[0] = cnt;
		DirectoryEntry *last = 0;
		int diroffset = 4;
		do
		{
			DirectoryEntry *entry = (DirectoryEntry*)(directory + diroffset);
			entry->entryType = elm->type == ELM_LOLCRYPT ? ELM_COMPRESSED : elm->type;
			if (elm->type == ELM_DIR)
			{
				entry->fileOffset = 0;
				entry->fileSize = 0;
			}
			else
			{
				entry->fileOffset = elm->offset;
				entry->fileSize = elm->filesize;
			}
			entry->timestamp = 0;

			int dirpart = 0;
			int i = 0;
			for (; elm->path[i]; i++)
			{
				if (elm->path[i] == '\\')
					dirpart = i + 1;

				entry->fileName[i] = elm->path[i];
			}
			entry->dirPart = dirpart;
			entry->fileNameLength = i;

			int padding = 4 - (entry->fileNameLength % 4);
			int sizeOfThisEntry = sizeof(DirectoryEntry)-4 + entry->fileNameLength + padding;

			entry->firstChildDirOffset = elm->firstChild ? elm->firstChild->dirOffset : -1;
			entry->nextSiblingDirOffset = elm->nextSibling ? elm->nextSibling->dirOffset : -1;
			last = entry;
			elm->dirOffset = diroffset - 4;
			diroffset += sizeOfThisEntry;
		} while (elm = elm->prevLinear);

		DirectoryHeader dirHeader;

		unsigned long compressedSize = compressBound(uncompressedDirSize);
		uint8_t *compressedDir = (uint8_t *)malloc(compressedSize + 4);
		compress(compressedDir, &compressedSize, directory, uncompressedDirSize);

		int cryptPadding = compressedSize % 4;
		if (cryptPadding)
			cryptPadding = 4 - cryptPadding;

		dirHeader.adler32 = adler32(29061971, compressedDir, compressedSize);
		SHoK_Encrypt((uint32_t *)compressedDir, compressedSize + cryptPadding);

		dirHeader.compressionHeader = 0x0637f2bd;

		dirHeader.decompressedSize = uncompressedDirSize;
		dirHeader.compressedSize = compressedSize;
		dirHeader.dataLength = compressedSize + cryptPadding + 12;
		dirHeader.fileEntriesLength = dirHeader.dataLength + 8;


		dirHeader.dirHeader[0] = 'B';
		dirHeader.dirHeader[1] = 'A';
		dirHeader.dirHeader[2] = 'd';
		dirHeader.dirVersion = 2;

		dirHeader.entryHeader[0] = 'B';
		dirHeader.entryHeader[1] = 'A';
		dirHeader.entryHeader[2] = 'e';
		dirHeader.entryVersion = 2;


		printf(" - Creating hashtable\n");
		int hashTableSize = htSize(cnt);
		uint32_t mask = hashTableSize - 1;
		HashTableElement *hashTable = (HashTableElement *)calloc(hashTableSize, sizeof(HashTableElement));

		elm = &root;
		do
		{
			uint32_t hash = shokHash(elm->path);
			uint32_t masked = mask & hash;
			while (hashTable[masked].BAeOffset)
				masked = (masked + 1) & mask;
			hashTable[masked].hash = hash;
			hashTable[masked].BAeOffset = elm->dirOffset;

		} while (elm = elm->nextLinear);



		HashTableHeader htHeader;
		htHeader.BAh_Header[0] = 'B';
		htHeader.BAh_Header[1] = 'A';
		htHeader.BAh_Header[2] = 'h';
		htHeader.BAh_Length = hashTableSize * sizeof(HashTableElement)+4;
		htHeader.BAh_Version = 2;
		htHeader.hashTableSize = hashTableSize;

		dirHeader.dirLength = dirHeader.fileEntriesLength + 8 + htHeader.BAh_Length + 8;

		FileHeader fileHeader;
		fileHeader.archiveHeader[0] = 'B';
		fileHeader.archiveHeader[1] = 'A';
		fileHeader.archiveHeader[2] = 'F';
		fileHeader.archiveVersion = 2;
		fileHeader.archiveLength = dirHeader.dirLength + fileBlockSize + 32;
		fileHeader.BAH_Header[0] = 'B';
		fileHeader.BAH_Header[1] = 'A';
		fileHeader.BAH_Header[2] = 'H';
		fileHeader.BAH_Version = 2;
		fileHeader.BAH_Length = 8;
		fileHeader.unknownField = 3;
		fileHeader.gameVersion = 1;
		fileHeader.fileDataHeader[0] = 'B';
		fileHeader.fileDataHeader[1] = 'A';
		fileHeader.fileDataHeader[2] = 'f';
		fileHeader.fileDataVersion = 2;
		fileHeader.fileDataLength = fileBlockSize;

		printf(" - Writing metadata\n");
		fwrite(&dirHeader, sizeof(DirectoryHeader), 1, writeHandle);
		fwrite(compressedDir, 1, compressedSize + cryptPadding, writeHandle);
		fwrite(&htHeader, sizeof(HashTableHeader), 1, writeHandle);
		fwrite(hashTable, sizeof(HashTableElement), hashTableSize, writeHandle);
		fseek(writeHandle, 0, SEEK_SET);
		fwrite(&fileHeader, sizeof(FileHeader), 1, writeHandle);
		fclose(writeHandle);

		double mbps = (1000.0 / (1024.0*1024.0))*((double)allFileSize / (double)(GetTickCount() - msStart));
		printf("\n - Done! (%3.2f MiB/s)\n", mbps > 0 ? mbps : 0);

		return true;
	}

	bool unpackfile(char *infileName, char *outputPath, char *extractDir)
	{
		uint32_t msStart = GetTickCount();
		printf("Command: Unpacking archive to folder\n\n");

		FileHeader inputHeader;
		DirectoryHeader dirHeader;
		uint8_t *dirEncData;
		uint8_t *dirData;
		char copyBuf[BUFSZ];
		int allFileSize = 0;


		FILE *handle = fopen(infileName, "rb");
		if (!handle)
		{
			printf("Error: Couldn't read input file!\n");
			return false;
		}

		printf(" - Reading metadata\n");

		fread(&inputHeader, sizeof(FileHeader), 1, handle);
		if(memcmp(inputHeader.archiveHeader, "BAF", 3) != 0)
		{
			printf("Error: Not a SettlersHoK archive!\n");
			return false;
		}

		fseek(handle, inputHeader.fileDataLength, SEEK_CUR);
		fread(&dirHeader, sizeof(DirectoryHeader), 1, handle);

		printf(" - Reading directory\n");
		int cryptBlockSize = dirHeader.dataLength - 12;
		dirEncData = (uint8_t *)malloc(cryptBlockSize);
		fread(dirEncData, 1, cryptBlockSize, handle);
		SHoK_Decrypt((uint32_t *)dirEncData, cryptBlockSize);
		dirData = (uint8_t *)malloc(dirHeader.decompressedSize);
		uncompress(dirData, (uLongf *)&dirHeader.decompressedSize, dirEncData, dirHeader.compressedSize);

		int numElements = *(uint32_t *)dirData;
		int dirOffset = sizeof(uint32_t);


		printf(" - Unpacking files: ");

		char *path;

		if(outputPath)
		{
			path = outputPath;
		}
		else
		{
			path = (char *)malloc(strlen(infileName) + 10);
			path[0] = 0;
			strncat(path, infileName, 300);
			strncat(path, ".unpacked", 300);
		}

		int extractDirLen;
		if(extractDir)
			extractDirLen = strlen(extractDir);

		_mkdir(path);
		_chdir(path);

		int fileNum = 0;
		COORD curPos = getCurPos();
		for (int i = 0; i < numElements; i++)
		{
			fileNum++;
			gotoxy(curPos);
			printf("%d%%", (100 * fileNum) / numElements);

			DirectoryEntry *entry;
			entry = (DirectoryEntry *)(dirData + dirOffset);

			if(extractDir)
			{
				if(strncmp(extractDir, entry->fileName, extractDirLen))
					goto skip;
			}

			//order erzeugen
			if (entry->entryType == ELM_DIR)
			{
				mkpath(entry->fileName);
			}
			else
			{
				if (entry->dirPart != 0)
				{
					entry->fileName[entry->dirPart - 1] = 0;
					mkpath(entry->fileName);
					entry->fileName[entry->dirPart - 1] = '\\';
				}

				FILE *writeHandle = fopen(entry->fileName, "wb");
				if (!writeHandle)
				{
					printf("Error: Couldn't write to '%s'!\n", entry->fileName);
					return false;
				}
				allFileSize += entry->fileSize;

				if (entry->entryType == ELM_COMPRESSED)
				{
					fseek(handle, entry->fileOffset + 20, SEEK_SET);

					bool lolCrypt = false;
					if (demoVersion)
					{
						int len = strlen(entry->fileName);
						if (!strcmp("lua", &entry->fileName[len - 3]))
							lolCrypt = true;
					}

					inf(handle, writeHandle, lolCrypt);
				}
				else
				{
					fseek(handle, entry->fileOffset, SEEK_SET);

					int toCopy = entry->fileSize;
					while (toCopy)
					{
						int copySize = toCopy > BUFSZ ? BUFSZ : toCopy;
						fread(copyBuf, 1, copySize, handle);
						fwrite(copyBuf, 1, copySize, writeHandle);
						toCopy -= copySize;
					}
				}
				fclose(writeHandle);
			}
skip:
			int padding = 4 - (entry->fileNameLength % 4);
			dirOffset += sizeof(DirectoryEntry)-4 + entry->fileNameLength + padding;
		}

		double mbps = (1000.0 / (1024.0*1024.0))*((double)allFileSize / (double)(GetTickCount() - msStart));
		printf("\n\n - Done! (%3.2f MiB/s)\n", mbps > 0 ? mbps: 0);
		return true;
	}

	void main(int argc, char* argv[])
	{
		CCrashHandler ch;
		ch.SetProcessExceptionHandlers();
		ch.SetThreadExceptionHandlers();

		conHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD bufSz = 4;
		HKEY hKey = NULL;
		DWORD dwType = REG_DWORD;
		//DWORD key;
		//long returnStatus;

		//if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\yoq\\bbaTool"), 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
		//{
		//	returnStatus = RegQueryValueEx(hKey, TEXT("Key"), NULL, &dwType, (LPBYTE)&key, &bufSz);
		//	if (returnStatus == ERROR_SUCCESS && key == fullVersionKey)
		//		demoVersion = false;
		//}

		int argumentOffset = 0;
		int errorExitcode = 0;
		if (argc >= 2 && strcmp(argv[1], "-err")==0) {
			argumentOffset = 1;
			errorExitcode = 1;
		}

		printf("\t\t\tbbaTool\nVersion: v0.4\n\n");

		if (argc - argumentOffset == 1)
		{
			printf("Usage: Drop a file or folder onto the program file!\n\n");

			//if (demoVersion)
			//{
			//	printf("Demo Version (encrypts lua files), enter key for full version!");
			//	for (;;)
			//	{
			//		printf("\n>");
			//		int o = scanf("%d", &key);
			//		if (o == 0) while (fgetc(stdin) != '\n');

			//		if (key == fullVersionKey)
			//		{
			//			printf("         ... ok, please restart the tool!");

			//			if (!hKey)
			//				RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\yoq\\bbaTool", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &hKey, 0);
			//			RegSetValueExA(hKey, "Key", 0, REG_DWORD, (const BYTE*)&key, 4);
			//			RegCloseKey(hKey);

			//			getchar();
			//			getchar();
			//			return;
			//		}
			//		else
			//		{
			//			printf("         ... wrong, try again");
			//		}
			//	 } 
			//}

			if (errorExitcode == 1) {
				exit(errorExitcode);
			}
			else {
				getchar();
			}
			return;
		}

		ShowConsoleCursor(false);

		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;

		hFind = FindFirstFileExA(argv[1 + argumentOffset], FindExInfoStandard, &FindFileData, FindExSearchNameMatch, NULL, 0);

		bool noProblems = true;

		if (hFind == INVALID_HANDLE_VALUE)
		{
			printf("Error reading %s: (%d)\n", argv[1 + argumentOffset], GetLastError());
			if (errorExitcode == 1) {
				exit(errorExitcode);
			}
			else {
				getchar();
			}
			return;
		}
		else
		{
			char *outputPath = 0;
			if(argc > 2)
				outputPath = argv[2 + argumentOffset];
			char *extractDir = 0;
			if(argc > 3)
				extractDir = argv[3 + argumentOffset];

			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				noProblems = packfile(argv[1 + argumentOffset]);
			else
				noProblems = unpackfile(argv[1 + argumentOffset], outputPath, extractDir);
		}

		FindClose(hFind);

		if (!noProblems)
		{
			if (errorExitcode == 1) {
				exit(errorExitcode);
			}
			else {
				getchar();
			}
		}
	}

}