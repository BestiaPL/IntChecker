#ifndef Utils_h__
#define Utils_h__

class FarScreenSave
{
private:
	HANDLE hScreen;

public:
	FarScreenSave();
	~FarScreenSave();
};

bool CheckEsc();

bool IsAbsPath(const wchar_t* path);
void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize);
void IncludeTrailingPathDelim(wstring &pathStr);

int64_t GetFileSize_i64(const wchar_t* path);
int64_t GetFileSize_i64(HANDLE hFile);
bool IsFile(const wchar_t* path);

void TrimRight(char* str);

int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive);

bool CopyTextToClipboard(std::wstring &data);
bool CopyTextToClipboard(std::vector<std::wstring> &data);

#endif // Utils_h__