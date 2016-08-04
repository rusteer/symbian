#ifndef __SILENTINSTENGINE_H
#define __SILENTINSTENGINE_H

#include <swinstapi.h>
#include <swinstdefs.h>
#include <e32base.h>
#include <apgcli.h>
#include <apaid.h>
#include "aosync.h"

class MSisInstaller{
public:
  virtual void OnInstallerEvent(TBool aIsInstall,TUid aAppUid,TInt aError) = 0;
};

class CApplicationManager{
public:
  static CApplicationManager* NewL();
  static CApplicationManager* NewLC();
  ~CApplicationManager();

public:
  void Install();
  TBool IsInstalled(const TUid& aAppUid);//用于sisx
  TInt Uninstall(const TUid& aAppUID);
  TBool IsInstalling() const;
  TInt CApplicationManager::StartApplication(TUid aAppUid);
  HBufC* CApplicationManager::getApplicationName(const TUid& aAppUid);
private:
  void ConstructL();
  CApplicationManager();
private:
  TInt retVal;
  SwiUI::RSWInstSilentLauncher iLauncher;
  SwiUI::TInstallOptions iOptions;
  SwiUI::TInstallOptionsPckg iOptionsPckg;
  RApaLsSession iLsSession; //软件管理
  CAOSync *iAsyncWaiter;
};

#endif 
