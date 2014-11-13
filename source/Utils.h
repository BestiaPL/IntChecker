#ifndef Utils_h__
#define Utils_h__

bool CheckEsc();

bool IsAbsPath(const wchar_t* path);
void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize);
void IncludeTrailingPathDelim(wstring &pathStr);
std::wstring ExtractFileName(const std::wstring& fullPath);
std::wstring ExtractFileExt(const std::wstring& path);

int64_t GetFileSize_i64(const wchar_t* path);
int64_t GetFileSize_i64(HANDLE hFile);
bool IsFile(const wchar_t* path);

void TrimRight(char* str);

int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive);

bool CopyTextToClipboard(std::wstring &data);
bool CopyTextToClipboard(std::vector<std::wstring> &data);

std::wstring FormatString(const std::wstring fmt, ...);

#endif // Utils_h__