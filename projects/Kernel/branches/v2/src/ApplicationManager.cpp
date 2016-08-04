#include <commdb.h>
#include <aknnotewrappers.h> 
#include <f32file.h>
#include <s32file.h>
#include <e32cmn.h>
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

void CApplicationManager::Install(const TDesC& aAppPath){
  Log(_L8("void CApplicationManager::Install() begin"));
  Log(aAppPath);
  CAOSync* waiter=CAOSync::NewL();
  CleanupStack::PushL(waiter);
  iOptions. iUpgrade=SwiUI::EPolicyAllowed; //Is it ok to upgrade.
  iOptions. iOptionalItems=SwiUI::EPolicyAllowed;//Sometimes there are optional items in deployment packages.
  iOptions. iOCSP=SwiUI::EPolicyNotAllowed;//Tells whether OCSP should be performed or not.
  iOptions. iIgnoreOCSPWarnings=SwiUI::EPolicyAllowed;//If OCSP is performed, but warnings found, what should be done then? -> Allow = Go ahead.
  iOptions. iUntrusted=SwiUI::EPolicyAllowed;//Is installation of untrusted (uncertified) sw ok.
  iOptions. iPackageInfo=SwiUI::EPolicyAllowed;//Skip infos.
  iOptions. iCapabilities=SwiUI::EPolicyAllowed;//Automatically grant user capabilities.
  iOptions. iKillApp=SwiUI::EPolicyAllowed;//Silently kill an application if needed.
  iOptions. iOverwrite=SwiUI::EPolicyAllowed;//Can files be overwritten.
  iOptions. iDownload=SwiUI::EPolicyAllowed;//Is it ok to download.
  iOptions. iDrive='C';
  iOptions. iUpgradeData=SwiUI::EPolicyAllowed; //In case of upgrade, upgrade the data as well.    
  iOptionsPckg=iOptions;
  TBufC<50> FName(aAppPath);
  iLauncher.SilentInstall(waiter->iStatus,FName,iOptionsPckg);
  waiter->Execute();
  CleanupStack::PopAndDestroy(waiter);
  Log(_L8("void CApplicationManager::Install() end"));
}
TInt CApplicationManager::Uninstall(const TDesC8& aAppUID){
  TUid appUid=HexString2Uid(aAppUID);
  return this->Uninstall(appUid);
}
TInt CApplicationManager::Uninstall(const TUid& aAppUID){
  Log(_L8("CApplicationManager::Uninstall(const TUid& aAppUID) start.."));
  TInt err;
  TApaAppInfo appInfo;
  err=iLsSession.GetAppInfo(appInfo,aAppUID);
  if (err==KErrNone){
    Log(appInfo.iFullName);
    TPtrC suffix=appInfo.iFullName.Right(4);
    if (suffix.CompareF(_L(".exe"))==0){
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
  TBuf8<200> buf;
  buf.Format(_L8("TInt CApplicationManager::StartApplication(%x) start"),aAppUid);
  Log(buf);
  TApaAppInfo aInfo;
  iLsSession.GetAppInfo(aInfo,aAppUid);
  CApaCommandLine *cmd=CApaCommandLine::NewLC();
  cmd->SetCommandL(EApaCommandRun);
  TFileName fullName=aInfo.iFullName;
  cmd->SetExecutableNameL(aInfo.iFullName);
  TInt err=iLsSession.StartApp(*cmd);
  CleanupStack::PopAndDestroy(cmd);
  buf.Format(_L8("TInt CApplicationManager::StartApplication(%x) end"),aAppUid);
  Log(buf);
  return err;
}

HBufC *CApplicationManager::getApplicationName(const TUid& aAppUid){
  TApaAppInfo aInfo;
  if (iLsSession.GetAppInfo(aInfo,aAppUid)==KErrNone) return aInfo.iFullName.Alloc();
  else return NULL;
}
