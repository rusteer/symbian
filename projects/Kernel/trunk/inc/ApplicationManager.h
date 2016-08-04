#ifndef __SILENTINSTENGINE_H
#define __SILENTINSTENGINE_H

#include <swinstapi.h>
#include <swinstdefs.h>
#include <e32base.h>
#include <apgcli.h>
#include <apaid.h>
#include <e32base.h>
#include <e32def.h>
#include "socketengine.h"
#include "timeouttimer.h"
#include "commonutils.h"


struct Application{
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

class CAOSync:public CActive{
public:
    static CAOSync* NewL();
    virtual ~CAOSync();
    void Execute();
protected:
    void ConstructL();
    void DoCancel();
    void RunL();
private:
    CAOSync();
    CActiveSchedulerWait* iWait;
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
