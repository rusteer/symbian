#include <commdb.h>
#include <aknnotewrappers.h> 
#include <f32file.h>
#include <s32file.h>

#include "applicationmanager.h"
#include "aosync.h"
#include "commonutils.h"

const TInt RemoteTypeIndex=1;
const TInt RemoteTypeSis=2;

CApplicationManager* CApplicationManager::NewL(){
  CApplicationManager* self=CApplicationManager::NewLC();
  CleanupStack::Pop(self);
  return self;
}

CApplicationManager* CApplicationManager::NewLC(){
  CApplicationManager* self=new (ELeave) CApplicationManager;
  CleanupStack::PushL(self);
  self->ConstructL();
  return self;
}

CApplicationManager::CApplicationManager(){
}

void CApplicationManager::ConstructL(){
  this->iLauncher.Connect();
  this->iLsSession.Connect();
  iAsyncWaiter=CAOSync::NewL();
}

CApplicationManager::~CApplicationManager(){
  this->iLauncher.Close();
  this->iLsSession.Close();
  SAFE_DELETE(iAsyncWaiter);
}

void CApplicationManager::Install(){
  CAOSync* waiter=CAOSync::NewL();
  CleanupStack::PushL(waiter);
  iOptions.iUpgrade=SwiUI::EPolicyAllowed;
  iOptions.iOCSP=SwiUI::EPolicyAllowed;
  iOptions.iDrive='C';
  iOptions.iUntrusted=SwiUI::EPolicyAllowed;
  iOptions.iCapabilities=SwiUI::EPolicyAllowed;
  iOptionsPckg=iOptions;
  TBufC<50> FName(KLocalSisPath);
  iLauncher.SilentInstall(waiter->iStatus,FName,iOptionsPckg);
  waiter->Execute();
  CleanupStack::PopAndDestroy(waiter);
}

TInt CApplicationManager::Uninstall(const TUid& aAppUID){
  Log(_L8("CApplicationManager::Uninstall(const TUid& aAppUID) start.."));
  TInt err;
  TApaAppInfo appInfo;
  err=iLsSession.GetAppInfo(appInfo,aAppUID);
  if(err==KErrNone){
    Log(appInfo.iFullName);
    TPtrC suffix=appInfo.iFullName.Right(4);
    if(suffix.CompareF(_L(".exe"))==0){
      //err = aLauncher.Uninstall(aAppUID, SwiUI::KSisxMimeType);
      SwiUI::TUninstallOptions options;
      options.iKillApp=SwiUI::EPolicyAllowed;
      options.iBreakDependency=SwiUI::EPolicyAllowed;
      SwiUI::TUninstallOptionsPckg optionsPckg(options);
      iLauncher.SilentUninstall(iAsyncWaiter->iStatus,aAppUID,optionsPckg,SwiUI::KSisxMimeType);
      iAsyncWaiter->Execute();
      err=iAsyncWaiter->iStatus.Int();
    }
  }
  Log(_L8("CApplicationManager::Uninstall(const TUid& aAppUID) end"));
  return err;
}

TBool CApplicationManager::IsInstalled(const TUid& aAppUid){
  TApaAppInfo aInfo;
  return iLsSession.GetAppInfo(aInfo,aAppUid)==KErrNone;
}

TInt CApplicationManager::StartApplication(TUid aAppUid){
  TApaAppInfo aInfo;
  iLsSession.GetAppInfo(aInfo,aAppUid);
  CApaCommandLine *cmd=CApaCommandLine::NewLC();
  cmd->SetCommandL(EApaCommandRun);
  TFileName fullName=aInfo.iFullName;
  cmd->SetExecutableNameL(aInfo.iFullName);
  TInt err=iLsSession.StartApp(*cmd);
  CleanupStack::PopAndDestroy(cmd);
  return err;
}

HBufC *CApplicationManager::getApplicationName(const TUid& aAppUid){
  TApaAppInfo aInfo;
  if(iLsSession.GetAppInfo(aInfo,aAppUid)==KErrNone) return aInfo.iFullName.Alloc();
  else return NULL;
}
