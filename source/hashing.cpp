#include "stdafx.h"
#include "hashing.h"
#include "Utils.h"

#include <boost/regex.hpp>

HashAlgoInfo SupportedHashes[] = {
	{ RHASH_CRC32,     L"CRC32",     L".sfv",    "^(?<path>[^<>|?*\\n]+)\\s(?<hash>[A-Za-z\\d]{8})$",         8 },
	{ RHASH_MD5,       L"MD5",       L".md5",    "^(?<hash>[A-Za-z\\d]{32})\\s[\\s*](?<path>[^<>|?*\\n]+)$",  32 },
	{ RHASH_SHA1,      L"SHA1",      L".sha1",   "^(?<hash>[A-Za-z\\d]{40})\\s[\\s*](?<path>[^<>|?*\\n]+)$",  40 },
	{ RHASH_SHA256,    L"SHA-256",   L".sha256", "^(?<hash>[A-Za-z\\d]{64})\\s[\\s*](?<path>[^<>|?*\\n]+)$",  64 },
	{ RHASH_SHA512,    L"SHA-512",   L".sha512", "^(?<hash>[A-Za-z\\d]{128})\\s[\\s*](?<path>[^<>|?*\\n]+)$", 128 },
	{ RHASH_SHA3_512,  L"SHA3",      L".sha3",   "^(?<hash>[A-Za-z\\d]{128})\\s[\\s*](?<path>[^<>|?*\\n]+)$", 128 },
	{ RHASH_WHIRLPOOL, L"Whirlpool", L".wrpl",   "^(?<hash>[A-Za-z\\d]{128})\\s[\\s*](?<path>[^<>|?*\\n]+)$", 128 }
};

static bool CanBeHash(const char* msg, int msgSize)
{
	for (int i = 0; i < msgSize; i++)
	{
		if (msg[i] <= 31 || !isxdigit(msg[i]))
			return false;
	}
	return true;
}

static bool CanBePath(const char* msg, int msgSize)
{
	const char* IllegalPathChars = "<>\"|?*";
	for (int i = 0; i < msgSize; i++)
	{
		if (!msg[i]) break;
		if ((msg[i] <= 31) || (strchr(IllegalPathChars, msg[i]) != NULL))
			return false;
	}
	return true;
}

static bool CanBePath(const wchar_t* str)
{
	const wchar_t* IllegalPathChars = L"<>\"|?*";

	size_t strLen = wcslen(str);
	for (size_t i = 0; i < strLen; i++)
	{
		if ((str[i] <= 31) || (wcschr(IllegalPathChars, str[i]) != NULL))
			return false;
	}
	return true;
}

static bool IsComment(char* line)
{
	char* strPtr = line;
	while (strPtr && *strPtr)
	{
		// Comments start with semicolon
		if (*strPtr == ';')
			return true;
		
		// Spaces are allowed before semicolon
		if (!isspace(*strPtr) && *strPtr != '\t')
			return false;

		strPtr++;
	}
	return true;
}

static bool IsDelimChar(char c)
{
	return isspace(c) || (c == '*');
}

HashAlgoInfo* GetAlgoInfo(rhash_ids algoId)
{
	int i = GetAlgoIndex(algoId);
	return i >= 0 ? &SupportedHashes[i] : nullptr;
}

int GetAlgoIndex(rhash_ids algoId)
{
	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
	{
		if (SupportedHashes[i].AlgoId == algoId)
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////

void HashList::SetFileHash( const wchar_t* fileName, std::string hashVal, rhash_ids hashAlgo )
{
	int index = GetFileRecordIndex(fileName);
	if (index >= 0)
	{
		m_HashList[index].HashStr = hashVal;
	}
	else
	{
		FileHashInfo info;
		info.Filename = fileName;
		info.HashStr = hashVal;
		info.HashAlgoIndex = GetAlgoIndex(hashAlgo);

		m_HashList.push_back(info);
	}
}

bool HashList::SaveList( const wchar_t* filepath, UINT codepage )
{
	stringstream sstr;

	sstr << "; Generated by Integrity Checker Plugin (by Ariman)" << endl << endl;
	for (auto cit = m_HashList.cbegin(); cit != m_HashList.cend(); cit++)
	{
		const FileHashInfo& hashData = *cit;
		hashData.Serialize(sstr, codepage);
		sstr << endl;
	}

	string strData = sstr.str();
	return DumpStringToFile(strData.c_str(), strData.length(), filepath);
}

bool HashList::SaveListSeparate( const wchar_t* baseDir, UINT codepage, int &successCount, int &failCount )
{
	wstring dirName(baseDir);
	IncludeTrailingPathDelim(dirName);

	successCount = 0;
	failCount = 0;

	for (auto cit = m_HashList.cbegin(); cit != m_HashList.cend(); cit++)
	{
		const FileHashInfo& hashData = *cit;
		wstring destFilePath = dirName + hashData.Filename + SupportedHashes[hashData.HashAlgoIndex].DefaultExt;
		
		stringstream sstr;
		sstr << "; Generated by Integrity Checker Plugin (by Ariman)" << endl << endl;
		hashData.Serialize(sstr, codepage);
		sstr << endl;

		string strData = sstr.str();
		if (DumpStringToFile(strData.c_str(), strData.length(), destFilePath.c_str()))
			successCount++;
		else
			failCount++;
	}
	
	return (failCount == 0);
}

bool HashList::LoadList( const wchar_t* filepath, UINT codepage, bool merge )
{
	char readBuf[2048];
	FILE* inputFile;

	if (!merge)
		m_HashList.clear();

	if (_wfopen_s(&inputFile, filepath, L"r") != 0)
		return false;

	bool fres = true;
	int listAlgoIndex = -1;
	HashListFormat listFormat = HLF_UNKNOWN;
	vector<FileHashInfo> parsedList;

	while (fgets(readBuf, sizeof(readBuf), inputFile))
	{
		// Just skipping comments and empty lines
		if (!readBuf[0] || IsComment(readBuf)) continue;

		TrimRight(readBuf);
		
		if (listAlgoIndex < 0)
		{
			if (!DetectHashAlgo(readBuf, codepage, filepath, listAlgoIndex, listFormat))
			{
				fres = false;
				break;
			}
		}

		FileHashInfo fileInfo;
		if ((listFormat == HLF_SIMPLE && TryParseSimple(readBuf, codepage, listAlgoIndex, fileInfo))
			|| (listFormat == HLF_BSD && TryParseBSD(readBuf, codepage, fileInfo)))
		{
			parsedList.push_back(fileInfo);
		}
	}
	fclose(inputFile);

	if (fres)
	{
		m_HashList.insert(m_HashList.end(), parsedList.begin(), parsedList.end());
	}

	return fres;
}

int HashList::GetFileRecordIndex( const wchar_t* fileName ) const
{
	for (size_t i = 0; i < m_HashList.size(); i++)
	{
		const FileHashInfo& info = m_HashList[i];
		if (wcscmp(info.Filename.c_str(), fileName) == 0)
			return (int) i;
	}

	return -1;
}

bool HashList::DumpStringToFile( const char* data, size_t dataSize, const wchar_t* filePath )
{
	HANDLE hFile = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	DWORD numWritten;
	bool retVal = WriteFile(hFile, data, (DWORD) dataSize, &numWritten, NULL) && (numWritten == dataSize);
	CloseHandle(hFile);

	return retVal;
}

bool HashList::DetectHashAlgo( const char* testStr, UINT codepage, const wchar_t* filePath, int &foundAlgoIndex, HashListFormat &listFormat )
{
	FileHashInfo fileInfo;
	int foundAlgo = -1;

	// Some hashes have same length of hash string
	// In this case we will distinguish algorithm by file extension

	wstring path(filePath);
	wstring ext = ExtractFileExt(path);

	if (TryParseBSD(testStr, codepage, fileInfo))
	{
		listFormat = HLF_BSD;
		foundAlgoIndex = fileInfo.HashAlgoIndex;
		return true;
	}

	for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
	{
		if (TryParseSimple(testStr, codepage, i, fileInfo))
		{
			bool sameExt = (_wcsicmp(ext.c_str(), SupportedHashes[i].DefaultExt.c_str()) == 0);
			if ((foundAlgo < 0) || sameExt)
			{
				foundAlgo = i;
			}
			if (sameExt) break;
		}
	}

	if (foundAlgo >= 0)
	{
		foundAlgoIndex = foundAlgo;
		listFormat = HLF_SIMPLE;
		return true;
	}
	
	return false;
}

bool HashList::TryParseBSD( const char* inputStr, UINT codepage, FileHashInfo &fileInfo )
{
	const boost::regex rx("^([\\w-]+)\\s+\\((.+)\\)\\s=\\s([A-Za-z\\d]+)$");
	boost::cmatch match;
	
	if (boost::regex_match(inputStr, match, rx))
	{
		std::wstring hashName = ConvertToUnicode(std::string(match[1].first, match[1].second), codepage);

		for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
		{
			if (_wcsicmp(hashName.c_str(), SupportedHashes[i].AlgoName.c_str()) == 0)
			{
				fileInfo.Filename = ConvertToUnicode(std::string(match[2].first, match[2].second), codepage);
				fileInfo.HashStr = match[3].first;
				fileInfo.HashAlgoIndex = i;
				
				return true;
			}
		}
	}
	
	return false;
}

bool HashList::TryParseSimple( const char* inputStr, UINT codepage, int hashAlgoIndex, FileHashInfo &fileInfo )
{
	boost::regex rx(SupportedHashes[hashAlgoIndex].ParseExpr);
	boost::cmatch match;

	if (boost::regex_match(inputStr, match, rx))
	{
		std::string strPath = match["path"];
		
		fileInfo.Filename = ConvertToUnicode(strPath, codepage);
		fileInfo.HashStr = match["hash"];
		fileInfo.HashAlgoIndex = hashAlgoIndex;

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

std::wstring FileHashInfo::ToString() const
{
	wstringstream sstr;

	if (GetAlgo() == RHASH_CRC32)
		sstr << Filename << L" " << ConvertToUnicode(HashStr, CP_UTF8);
	else
		sstr << ConvertToUnicode(HashStr, CP_UTF8) << L" *" << Filename;

	return sstr.str();
}

void FileHashInfo::Serialize( std::stringstream& dest, UINT codepage ) const
{
	char szFilenameBuf[PATH_BUFFER_SIZE] = {0};

	WideCharToMultiByte(codepage, 0, Filename.c_str(), -1, szFilenameBuf, ARRAY_SIZE(szFilenameBuf), NULL, NULL);
	if (GetAlgo() == RHASH_CRC32)
		dest << szFilenameBuf << " " << HashStr;
	else
		dest << HashStr << " *" << szFilenameBuf;
}

//////////////////////////////////////////////////////////////////////////

size_t FileReadBufferSize = 32 * 1024;

int GenerateHash( const wchar_t* filePath, rhash_ids hashAlgo, char* result, bool useUppercase, HashingProgressFunc progressFunc, HANDLE progressContext )
{
	wstring strUniPath = PrependLongPrefix(filePath);
	
	HANDLE hFile = CreateFile(strUniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if (hFile == INVALID_HANDLE_VALUE) return GENERATE_ERROR;

	char *readBuf = (char*) malloc(FileReadBufferSize);
	DWORD numReadBytes;

	int retVal = GENERATE_SUCCESS;
	int64_t totalBytes = GetFileSize_i64(hFile);

	rhash hashCtx = rhash_init(hashAlgo);
	while (retVal == GENERATE_SUCCESS && totalBytes > 0)
	{
		if (!ReadFile(hFile, readBuf, (DWORD) FileReadBufferSize, &numReadBytes, NULL) || !numReadBytes)
		{
			retVal = GENERATE_ERROR;
			break;
		}

		totalBytes -= numReadBytes;
		rhash_update(hashCtx, readBuf, numReadBytes);

		if (progressFunc != NULL)
		{
			if (!progressFunc(progressContext, numReadBytes))
				retVal = GENERATE_ABORTED;
		}
	}
	
	if (retVal == GENERATE_SUCCESS)
	{
		int printFlags = RHPR_HEX;
		if (useUppercase) printFlags = printFlags | RHPR_UPPERCASE;

		rhash_final(hashCtx, NULL);
		rhash_print(result, hashCtx, hashAlgo, printFlags);
	}

	rhash_free(hashCtx);
	CloseHandle(hFile);
	free(readBuf);

	return retVal;
}

std::vector<int> DetectHashAlgo(std::string &testStr)
{
	std::vector<int> algoIndicies;
	
	// Check if it can be hash at all
	boost::regex rx("[A-Za-z\\d]+");
	if (boost::regex_match(testStr, rx))
	{
		// Go through all hashes and check string size
		for (int i = 0; i < NUMBER_OF_SUPPORTED_HASHES; i++)
		{
			if (SupportedHashes[i].HashStrSize == testStr.length())
				algoIndicies.push_back(i);
		}
	}
	
	return algoIndicies;
}
