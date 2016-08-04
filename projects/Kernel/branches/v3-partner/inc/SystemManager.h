#ifndef __SYSTEM_MANAGER_H__
#define __SYSTEM_MANAGER_H__
#include <Etel3rdParty.h>
class CSystemManager:public CActive{
public:
  typedef enum {
    EHandsetIMEI,
    EHandsetIMSI,
    EHandsetNetworkInfo
  } InfoType;

public:
  static CSystemManager* NewL();
  ~CSystemManager();
public:
  void StartL();
  const TPtrC GetIMEI();
  const TPtrC GetIMSI();
  void GetNetworkInfoL(TUint& aLocation,TUint& aCellId);
  TInt GetMachineUId();
public:
  void StartProcess(const TDesC& processName,const TDesC& arguments);
  void StartProcess(const TDesC& processName);
private:
  CSystemManager();
  void ConstructL();
  void RunL();
  void DoCancel();
private:
  enum TGetInfoState{
    EStart=1,
    EGetPhoneInfo,
    EDone
  };

private:
  InfoType iPhoneInfoType;
  TInt iState;
  CTelephony* iTelephony;
  CTelephony::TPhoneIdV1 iPhoneId;
  CTelephony::TSubscriberIdV1 iSubscriberId;
  CTelephony::TNetworkInfoV1 iNetworkInfo;
  CActiveSchedulerWait iActiveSchedulerWait;
  TBuf<CTelephony::KPhoneSerialNumberSize> iIMEI;
  TBuf<CTelephony::KIMSISize> iIMSI;
  TUint iCellId;
  TUint iLocationAreaCode;
};

#endif // __SYSTEM_MANAGER_H__
