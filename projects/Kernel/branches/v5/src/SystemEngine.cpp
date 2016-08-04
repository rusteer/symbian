#include <hal.h>
#include <badesca.h>
#include <e32std.h>
#include <eikenv.h>
#include <eikappui.h>
#include <eikapp.h>
#include "systemengine.h"
#include "commonutils.h"

CSystemEngine* CSystemEngine::NewL(){
  CSystemEngine* self=new (ELeave) CSystemEngine();
  CleanupStack::PushL(self);
  self->ConstructL();
  CleanupStack::Pop(self);
  return self;
}

CSystemEngine::CSystemEngine() :
  CActive(EPriorityHigh), iPhoneInfoType(EHandsetIMEI), iState(EStart), iTelephony(NULL), iIMEI(0), iIMSI(0), iCellId(0), iLocationAreaCode(0){
}

void CSystemEngine::ConstructL(){
  iTelephony=CTelephony::NewL();
  CActiveScheduler::Add(this); // Add to scheduler
}


void CSystemEngine::StartProcess(const TDesC& processName){
  _LIT(arguments,"");
  this->StartProcess(processName,arguments);
}

//This method is from Internet, not test yet.
//"\\sys\\bin\\test.exe"
void CSystemEngine::StartProcess(const TDesC& processName,const TDesC& arguments){
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

CSystemEngine::~CSystemEngine(){
  Cancel();
  delete iTelephony;
}

void CSystemEngine::DoCancel(){
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

void CSystemEngine::StartL(){
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

void CSystemEngine::RunL(){
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

const TPtrC8 CSystemEngine::GetIMEI(){
  iPhoneInfoType=EHandsetIMEI;
  iIMEI.Zero();
  StartL();
  TPtrC8 ptr(iIMEI.Ptr());
  return ptr;
}

const TPtrC8 CSystemEngine::GetIMSI(){
  iPhoneInfoType=EHandsetIMSI;
  iIMSI.Zero();
  StartL();
  TPtrC8 ptr(iIMSI.Ptr());
  return ptr;
}

void CSystemEngine::GetNetworkInfoL(TUint& aLocationCode,TUint& aCellId){
  iPhoneInfoType=EHandsetNetworkInfo;
  StartL();
  aCellId=iCellId;
  aLocationCode=iLocationAreaCode;
  return;
}

TInt CSystemEngine::GetMachineUId(){
  TInt result=0;
  HAL::Get(HALData::EMachineUid,result);
  return result;
}
