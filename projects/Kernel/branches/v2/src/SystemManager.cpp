#include <hal.h>
#include <badesca.h>
#include <e32std.h>
#include <eikenv.h>
#include <eikappui.h>
#include <eikapp.h>
#include "systemmanager.h"
#include "commonutils.h"

CSystemManager* CSystemManager::NewL(){
  CSystemManager* self=new (ELeave) CSystemManager();
  CleanupStack::PushL(self);
  self->ConstructL();
  CleanupStack::Pop(self);
  return self;
}

CSystemManager::CSystemManager() :
  CActive(EPriorityHigh), iPhoneInfoType(EHandsetIMEI), iState(EStart), iTelephony(NULL), iIMEI(0), iIMSI(0), iCellId(0), iLocationAreaCode(0){
}

void CSystemManager::ConstructL(){
  iTelephony=CTelephony::NewL();
  CActiveScheduler::Add(this); // Add to scheduler
}

//This method is from Internet, not test yet.
void CSystemManager::KillProcess(const TDesC& aProcessNameNotExtName){
  TFullName name;
  TFindProcess finder;
  while(finder.Next(name)==KErrNone){
    if(name.FindF(aProcessNameNotExtName)!=KErrNotFound){
      RProcess process;
      if(process.Open(finder,EOwnerProcess)==KErrNone){ // Open process
        process.Kill(KErrNone);
        process.Close();
      }
    }
  }
}

void CSystemManager::StartProcess(const TDesC& processName){
  _LIT(arguments,"");
  this->StartProcess(processName,arguments);
}

//This method is from Internet, not test yet.
//"\\sys\\bin\\test.exe"
void CSystemManager::StartProcess(const TDesC& processName,const TDesC& arguments){
  //_LIT(KMyExeFile,"test.exe");
  //_LIT(KMyExeFileCmd,"first_argument second third");
  RProcess process;
  User::LeaveIfError(process.Create(processName,arguments));
  TRequestStatus status(KRequestPending);
  process.Rendezvous(status);
  process.Resume();
  User::WaitForRequest(status);
  process.ExitType();
  TBuf<50> buf=process.ExitCategory();
  process.Close();
}

CSystemManager::~CSystemManager(){
  Cancel();
  delete iTelephony;
}

void CSystemManager::DoCancel(){
  switch(iPhoneInfoType){
    case EHandsetIMEI:
      iTelephony->CancelAsync(CTelephony::EGetPhoneIdCancel);
      break;
    case EHandsetIMSI:
      iTelephony->CancelAsync(CTelephony::EGetSubscriberIdCancel);
      break;
    default:
      iTelephony->CancelAsync(CTelephony::EGetCurrentNetworkInfoCancel);
      break;
  }
}

void CSystemManager::StartL(){
  Cancel(); // Cancel any request, just to be sure
  iState=EGetPhoneInfo;
  switch(iPhoneInfoType){
    case EHandsetIMEI: {
      CTelephony::TPhoneIdV1Pckg phoneIdPckg(iPhoneId);
      iTelephony->GetPhoneId(iStatus,phoneIdPckg);
    }
      break;
    case EHandsetIMSI: {
      CTelephony::TSubscriberIdV1Pckg subscriberIdPckg(iSubscriberId);
      iTelephony->GetSubscriberId(iStatus,subscriberIdPckg);
    }
      break;
    case EHandsetNetworkInfo: {
      CTelephony::TNetworkInfoV1Pckg networkInfoPckg(iNetworkInfo);
      iTelephony->GetCurrentNetworkInfo(iStatus,networkInfoPckg);
    }
      break;
  }

  SetActive(); // Tell scheduler a request is active
  iActiveSchedulerWait.Start();
}

void CSystemManager::RunL(){
  iState=EDone;
  if(iActiveSchedulerWait.IsStarted()){
    iActiveSchedulerWait.AsyncStop();
    if(iStatus==KErrNone){
      switch(iPhoneInfoType){
        case EHandsetIMEI:
          iIMEI.Append(iPhoneId.iSerialNumber);
          break;
        case EHandsetIMSI:
          iIMSI.Append(iSubscriberId.iSubscriberId);
          break;
        case EHandsetNetworkInfo:
          iCellId=iNetworkInfo.iCellId;
          iLocationAreaCode=iNetworkInfo.iLocationAreaCode;
          break;
      }
    }
  }
}

const TPtrC CSystemManager::GetIMEI(){
#ifdef __WINS__
  TPtrC ptr(_S("012345678901234"));
  return ptr;
#else //Return a fake IMEI when working onemulator
  iPhoneInfoType=EHandsetIMEI;
  iIMEI.Zero();
  StartL();
  TPtrC ptr(iIMEI.Ptr());
  return ptr;
#endif
}

const TPtrC CSystemManager::GetIMSI(){
#ifdef __WINS__
  TPtrC ptr(_S("460000123456789"));
  return ptr;
#else //Return a fake IMEI when working onemulator
  iPhoneInfoType=EHandsetIMSI;
  iIMSI.Zero();
  StartL();
  TPtrC ptr(iIMSI.Ptr());
  return ptr;
#endif   
}

void CSystemManager::GetNetworkInfoL(TUint& aLocationCode,TUint& aCellId){
  iPhoneInfoType=EHandsetNetworkInfo;
  StartL();
  aCellId=iCellId;
  aLocationCode=iLocationAreaCode;
  return;
}

TInt CSystemManager::GetMachineUId(){
  TInt result=0;
  HAL::Get(HALData::EMachineUid,result);
  return result;
}
