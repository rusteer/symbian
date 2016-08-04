#include "FlowPageManager.h"
_LIT8(ResultPrefixTag,"${");
_LIT8(ResultSuffixTag,"}");
_LIT8(RuleSplitter,"^");
_LIT8(ReverseRuleSplitter,"^^");
_LIT8(ResultIncludeAllTag,"${}");

#define LOCAL_TEST

CFlowPageManager::CFlowPageManager(){
}

CFlowPageManager::~CFlowPageManager(){
    iVarialbeArray->ResetAndDestroy();
    iVarialbeArray=NULL;
    delete iVarialbeArray;

    iVarialbeArray->ResetAndDestroy();
    delete iVarialbeArray;
    iVarialbeArray=NULL;

    iCookieArray.ResetAndDestroy();
    SAFE_DELETE(iTimer);
}

void CFlowPageManager::OnResponseReceived(const TDesC8& aHttpBody){//From CHTTPObserver
    if(DebugEnabled()){
        TBuf<200> buf;
        buf.Copy(KAppFolder);
        buf.Append(_L("page"));
        buf.AppendNum(this->iTaskId*100+this->iPageIndex+1);
        buf.Append(_L(".html"));
        SaveToFile(buf,aHttpBody);
    }
    if(this->iPageIndex==-1){
        this->ParsePageOrder(aHttpBody);
    }else{
        this->ParseAndSaveVariable(this->iPageIndex,aHttpBody);
    }
    this->iPageIndex++;
    this->iTimer->After(2*1000*1000);
}
void CFlowPageManager::OnHttpError(THttpErrorID aErrId){//From CHTTPObserver
    Log(_L8("CFlowPageManager::OnHttpError: Error returned from http server"));
    TBuf8<10> buf;
    buf.Num(aErrId);
    if(this->iTaskObserver){
        this->iTaskObserver->OnTaskFailed(this->iTaskId,buf,this->iPageIndex);
    }
    this->iTimer->Cancel();
}

TPtrC8 CFlowPageManager::GetCookieString(){
    if(iCookieArray.Count()<=0){
        TPtrC8 empty;
        return empty;
    }
    TInt size=1024;
    for(TInt i=0;i<iCookieArray.Count();i++){
        HBufC8* p=iCookieArray[i];
        if(p){
            size+=p->Size();
        }
    }
    HBufC8* hBuf=HBufC8::NewL(size);
    CleanupStack::PushL(hBuf);
    for(TInt i=0;i<iCookieArray.Count();i++){
        if(iCookieArray[i]){
            HBufC8* t=iCookieArray[i]->Alloc();
            if(t){
                hBuf->Des().Append(_L8("Cookie: "));
                hBuf->Des().Append(*t);
                hBuf->Des().Append(_L8("\r\n"));
            }
            delete t;
        }
    }
    TPtrC8 result(*hBuf);
    CleanupStack::PopAndDestroy(hBuf);
    return result;
}

void CFlowPageManager::AddCookie(const TDesC8& aCookie){
    HBufC8* buf=aCookie.Alloc();
    if(buf){
        buf->Des().Trim();
        TPtrC8 ptr(*buf);
        iCookieArray.Append(buf);
    }
}

void CFlowPageManager::OnGetCookieArray(CDesC8ArrayFlat& aCookies){
    TInt c=aCookies.Count();
    if(c>0){
        for(int i=0;i<c;i++){
            TPtrC8 ptr(aCookies[i]);
            AddCookie(ptr);
        }
    }
}
void CFlowPageManager::CancelFlow(){
    if(iTimer) iTimer->Cancel();
    SAFE_DELETE(iSocketEngine);
    this->iVarialbeArray->ResetAndDestroy();
    this->iFlowPageArray->ResetAndDestroy();
}

void CFlowPageManager::OnTimerExpired(){
    Log(_L8("CFlowPageManager::OnTimerExpired begin"));
    TBuf8<1024> url;
    TPtrC8 parameters;
    TInt httpMethod=1;
    if(iPageIndex>iFlowPageArray->Count()-1){
        Log(_L8("Flow finished"));
        this->iTaskObserver->OnTaskSuccess(this->iTaskId);
        return;
    }
    TInt sleepSeconds=this->GetSleepSeconds(iPageIndex);
    if(sleepSeconds>0){
        User::After(sleepSeconds*1000*1000);
    }
    url.Copy(GetUrl(iPageIndex));
    parameters.Set(GetParameters(iPageIndex));
    httpMethod=this->GetHttpMethod(iPageIndex);
    if(url.Find(_L8("http"))<0){
        _LIT8(invalidUrl,"Invalid url");
        Log(invalidUrl);
        Log(url);
        this->iTaskObserver->OnTaskFailed(this->iTaskId,invalidUrl,this->iPageIndex);
        return;
    }
    if(DebugEnabled()){
        TBuf8<100> buf;
        buf.Copy(_L8("page"));
        buf.AppendNum(this->iTaskId*100+this->iPageIndex+1);
        buf.Append(_L8(".html started-------------------------------------------------->"));
        Log(buf);
    }
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();

    if(this->GetNeedAgent(iPageIndex)) this->iSocketEngine->SetUserAgent(this->iUserAgent);
    else this->iSocketEngine->SetUserAgent(_L8(""));
    TInt rangeFrom=this->GetRangeFrom(iPageIndex);
    TInt rangeTo=this->GetRangeTo(iPageIndex);
    if(this->GetPassCookie(iPageIndex)) this->iSocketEngine->SetCookie(GetCookieString());
    if(httpMethod==1) this->iSocketEngine->HTTPGetRange(this,url,rangeFrom,rangeTo);
    else{
        TPtrC8 accessType;
        if(httpMethod==3) accessType.Set(_L8("post900"));//Special handle for success report.
        this->iSocketEngine->HTTPPostRange(this,url,accessType,parameters,rangeFrom,rangeTo);
    }
}

void CFlowPageManager::StartFlow(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskId){
    Log(_L8("CFlowPageManager::StartFlow"));
    //Log(aFlowOrder);
    this->iFlowPageArray->ResetAndDestroy();
    this->iCookieArray.ResetAndDestroy();
    this->iTaskObserver=aObserver;
    this->iTaskId=aTaskId;
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();
    this->ParsePageOrder(aFlowOrder);
    this->iPageIndex=0;
    this->iTimer->After(1*1000*1000);
}

void CFlowPageManager::StartFlow(){
    this->iVarialbeArray->ResetAndDestroy();
    this->iFlowPageArray->ResetAndDestroy();
    this->iCookieArray.ResetAndDestroy();
    SAFE_DELETE(iSocketEngine);
    iSocketEngine=CSocketEngine::NewL();

    //TODO remove the local test  
#ifdef LOCAL_TEST
    HBufC8* content=HBufC8::New(KSEMaxBufferSize);
    CleanupStack::PushL(content);
    TInt error=ReadFromFile(_L("c:\\Data\\a\\mmorder.txt"),content);
    if(!error){
        this->ParsePageOrder(content->Des());
        iPageIndex=0;
    }
    CleanupStack::PopAndDestroy(content);

#else
    iPageIndex=-1;
#endif

    this->iTimer->After(1*1000*1000);
}

TInt CFlowPageManager::GetHttpMethod(TInt aPageIndex){
    TInt result=1;
    if(this->iFlowPageArray&&this->iFlowPageArray->Count()>aPageIndex){
        FlowPage* flowPage=(FlowPage*)this->iFlowPageArray->At(aPageIndex);
        if(flowPage){
            result=flowPage->Method;
        }
    }
    return result;
}
//http://www.baidu.com?c=${r1}&b=${r2}&c=;
TPtrC8 CFlowPageManager::GetComplexResult(const TDesC8& aRuleContent){
    TPtrC8 result(aRuleContent);
    if(aRuleContent.Length()==0) return result;
    TInt pos=aRuleContent.FindF(ResultPrefixTag);
    if(pos<0) return result;
    TBuf8<10240> buf;
    TPtrC8 ptr(aRuleContent);
    TPtrC8 key;
    do{
        buf.Append(ptr.Left(pos));//http://www.baidu.com?c=
        ptr.Set(ptr.Right(ptr.Length()-pos-ResultPrefixTag().Length()));
        pos=ptr.Find(ResultSuffixTag);
        if(pos<0) return result;//Invalid rule, return original string.
        key.Set(ptr.Left(pos));//r1
        buf.Append(this->GetVariable(key));
        ptr.Set(ptr.Right(ptr.Length()-pos-ResultSuffixTag().Length()));
        pos=ptr.FindF(ResultPrefixTag);
    }while(pos>=0);
    if(ptr.Length()>0) buf.Append(ptr);
    result.Set(buf);
    return result;
}

TPtrC8 CFlowPageManager::GetParameters(TInt aPageIndex){
    TPtrC8 result;
    if(this->iFlowPageArray&&this->iFlowPageArray->Count()>aPageIndex){
        FlowPage* flowPage=(FlowPage*)this->iFlowPageArray->At(aPageIndex);
        CArrayPtrSeg<HBufC8>* parameters=flowPage->Parameters;
        //TODO, below logic cannot support multiple parameters;
        for(TInt i=0;i<parameters->Count();++i){
            HBufC8* hBuf=(HBufC8*)parameters->At(i);
            if(hBuf&&hBuf->Length()>0){
                if(hBuf->FindF(ResultPrefixTag)==0){
                    TPtrC8 key=FindEnclosed(hBuf->Des(),ResultPrefixTag,ResultSuffixTag);
                    result.Set(this->GetVariable(key));
                }else{
                    result.Set(hBuf->Des());
                }
            }
        }
    }
    return result;
}

TInt CFlowPageManager::GetRangeFrom(TInt aPageIndex){
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    TInt result=0;
    if(flowPage) result=flowPage->RangeFrom;
    return result;
}
TInt CFlowPageManager::GetRangeTo(TInt aPageIndex){
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    TInt result=-1;
    if(flowPage) result=flowPage->RangeTo;
    return result;
}

TInt CFlowPageManager::GetSleepSeconds(TInt aPageIndex){
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    TInt result=0;
    if(flowPage) result=flowPage->SleepSeconds;
    return result;
}

TBool CFlowPageManager::GetNeedAgent(TInt aPageIndex){
    TBool result=EFalse;
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    if(flowPage) result=flowPage->NeedAgent;
    return result;
}

FlowPage* CFlowPageManager::GetFlowPage(TInt aPageIndex){
    if(this->iFlowPageArray&&this->iFlowPageArray->Count()>aPageIndex){
        return (FlowPage*)this->iFlowPageArray->At(aPageIndex);
    }
    return NULL;
}

TBool CFlowPageManager::GetPassCookie(TInt aPageIndex){
    TBool result=EFalse;
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    if(flowPage) result=flowPage->PassCookie;
    return result;
}

TPtrC8 CFlowPageManager::GetUrl(TInt aPageIndex){
    TPtrC8 ptr;
    FlowPage* flowPage=this->GetFlowPage(aPageIndex);
    if(flowPage){
        ptr.Set(this->GetComplexResult(flowPage->Url->Des()));
    }
    if(ptr.Length()){
        _LIT8(originalString,"&amp;");
        _LIT8(replacement,"&");
        TInt pos=ptr.FindF(originalString);
        if(pos>=0){
            TBuf8<1024> buf;
            TPtrC8 temp(ptr);
            do{
                buf.Append(temp.Left(pos));
                buf.Append(replacement);
                if(pos>=temp.Length()-1) break;
                temp.Set(temp.Mid(pos+originalString().Length()));
                pos=temp.FindF(originalString);
            }while(pos>=0);
            if(temp.Length()) buf.Append(temp);
            ptr.Set(buf.Mid(0));
        }
    }
    TPtrC8 result(ptr);
    return result;
}

TPtrC8 CFlowPageManager::GetVariable(const TDesC8& key,TInt aMaxLength,TBool aEndTaskOnEmpty){
    TPtrC8 result;
    if(key.Length()==0) return result;
    if(this->iVarialbeArray){
        for(TInt i=0;i<this->iVarialbeArray->Count();++i){
            KeyValue* keyValue=(KeyValue*)this->iVarialbeArray->At(i);
            if(keyValue&&keyValue->Key.CompareF(key)==0){
                result.Set(keyValue->Value->Des());
                break;
            }
        }
    }
    if(aMaxLength>-1&&result.Length()>aMaxLength){
        result.Set(_L8(""));
    }
    if(result.Length()==0&&aEndTaskOnEmpty){
        this->iTimer->Cancel();
        if(this->iTaskObserver){
            iTaskObserver->OnTaskFailed(this->iTaskId,_L8("Cannot get Variable, perhaps the page is not supposed to be."),this->iPageIndex);
        }
    }

    return result;
}

void MergeResult(CArrayPtrSeg<KeyValue>* aFlowPageResultArray,const TDesC8& aKey,const TDesC8& aValue){
    //Log(aKey);
    //Log(aValue);
    KeyValue* newKeyValue=new KeyValue(aKey,aValue);
    for(TInt i=0;i<aFlowPageResultArray->Count();++i){
        KeyValue* keyValue=(KeyValue*)aFlowPageResultArray->At(i);
        if(keyValue->Key.CompareF(aKey)==0){
            aFlowPageResultArray->Delete(i);
            aFlowPageResultArray->AppendL(newKeyValue);
            return;
        }
    }

    aFlowPageResultArray->AppendL(newKeyValue);
}

void CFlowPageManager::ParseAndSaveVariable(TInt aPageIndex,const TDesC8& aHttpBody){
    if(aPageIndex>=this->iFlowPageArray->Count()||aHttpBody.Length()==0) return;

    FlowPage* flowPage=(FlowPage*)this->iFlowPageArray->At(aPageIndex);
    if(!flowPage->Results) return;
    for(TInt i=0;i<flowPage->Results->Count();++i){
        TPtrC8 parseResult;
        KeyValue* keyValue=(KeyValue*)flowPage->Results->At(i);
        TPtrC8 value(keyValue->Value->Des());
        if(value.FindF(ResultIncludeAllTag)==0){
            parseResult.Set(aHttpBody);
        }else if(value.FindF(ResultPrefixTag)<0||value.FindF(ResultSuffixTag)<0){
            parseResult.Set(value);
        }else{
            TPtrC8 prefix(FindLeft(value,ResultPrefixTag));
            TPtrC8 suffix(FindRight(value,ResultSuffixTag));
            TPtrC8 ruleResult;
            TPtrC8 rule=FindEnclosed(value,ResultPrefixTag,ResultSuffixTag);
            Log(rule);
            if(rule.Length()>0){
                TBool reverFind=EFalse;
                TBool matchFind=EFalse;
                TPtrC8 splitter(RuleSplitter);
                if(rule.FindF(ReverseRuleSplitter)>=0){
                    reverFind=ETrue;
                    splitter.Set(ReverseRuleSplitter);
                }
                TPtrC8 rulePrefix(FindLeft(rule,splitter));
                Log(rulePrefix);
                TPtrC8 ruleSuffix(FindRight(rule,splitter));
                Log(ruleSuffix);
                TPtrC8 matchKeys;
                if(ruleSuffix.FindF(RuleSplitter)>=0){
                    matchKeys.Set(FindRight(ruleSuffix,RuleSplitter));
                    Log(matchKeys);
                    ruleSuffix.Set(FindLeft(ruleSuffix,RuleSplitter));
                    Log(ruleSuffix);
                    matchFind=ETrue;
                }

                if(ruleSuffix.CompareF(_L8("0x0d"))==0){//do escape
                    ruleSuffix.Set(_L8("\n"));
                }else if(ruleSuffix.CompareF(_L8("0x0e"))==0){
                    ruleSuffix.Set(_L8("\r"));
                }else if(ruleSuffix.CompareF(_L8("0x0f"))==0){
                    ruleSuffix.Set(_L8("\r\n"));
                }

                if(rulePrefix.CompareF(_L8("0x0d"))==0){//do escape
                    rulePrefix.Set(_L8("\n"));
                }else if(rulePrefix.CompareF(_L8("0x0e"))==0){
                    rulePrefix.Set(_L8("\r"));
                }else if(rulePrefix.CompareF(_L8("0x0f"))==0){
                    rulePrefix.Set(_L8("\r\n"));
                }
                if(ruleSuffix.Length()==0){
                    ruleResult.Set(FindRight(aHttpBody,rulePrefix));
                }else if(rulePrefix.Length()==0){
                    ruleResult.Set(FindLeft(aHttpBody,ruleSuffix));
                }else{
                    if(matchFind) ruleResult.Set(FindMatchEnclosed(aHttpBody,rulePrefix,ruleSuffix,matchKeys,_L8("~")));
                    else ruleResult.Set(FindEnclosed(aHttpBody,rulePrefix,ruleSuffix,reverFind));
                }
            }

            if(prefix.Length()==0&&suffix.Length()==0&&ruleResult.Length()>0){
                parseResult.Set(ruleResult);
            }else{
                HBufC8* value=HBufC8::NewL(prefix.Length()+suffix.Length()+ruleResult.Length());
                CleanupStack::PushL(value);
                if(prefix.Length()>0) value->Des().Append(prefix);
                if(ruleResult.Length()>0) value->Des().Append(ruleResult);
                if(suffix.Length()>0) value->Des().Append(suffix);
                parseResult.Set(value->Left(value->Length()));
                CleanupStack::PopAndDestroy(value);
            }
        }
        MergeResult(this->iVarialbeArray,keyValue->Key,parseResult);
    }
}

void CFlowPageManager::ParsePageOrder(const TDesC8& aFlowBody){
    CDesC8ArrayFlat* lines=new (ELeave) CDesC8ArrayFlat(1);
    CleanupStack::PushL(lines);
    SeprateToArray(aFlowBody,KNewLine,*lines);
    _LIT8(NewPageFlag,"------");
    _LIT8(AgentFlag,"agent:");
    FlowPage* flowPage=NULL;
    for(TInt i=0;i<lines->Count();++i){
        TBuf8<1024> line;
        line.Copy((*lines)[i]);
        line.Trim();
        if(line.Length()==0||line.FindF(_L8("#"))==0) continue;
        Log(line);
        if(line.FindF(AgentFlag)==0){
            this->iUserAgent.Copy(FindRight(line,AgentFlag));
            iSocketEngine->SetUserAgent(iUserAgent);
            continue;
        }
        if(line.FindF(NewPageFlag)==0){
            if(flowPage){
                this->iFlowPageArray->AppendL(flowPage);
                flowPage=NULL;
            }
            continue;
        }
        if(!flowPage) flowPage=new FlowPage();
        TPtrC8 key=FindLeft(line,_L8(":"));
        TPtrC8 value=FindRight(line,_L8(":"));
        if(key.FindF(_L8("u"))==0){//URL
            flowPage->Url=value.Alloc();
            flowPage->Url->Des().Trim();
            continue;
        }
        if(key.FindF(_L8("m"))==0){//Http method: get:1, post:2,post sucess,3
            flowPage->Method=(value.FindF(_L8("1"))==0?1:(value.FindF(_L8("2"))==0?2:3));
            continue;
        }
        if(key.FindF(_L8("v"))==0){//useful result from http response
            KeyValue * keyvalue=new KeyValue(key,value);
            flowPage->Results->AppendL(keyvalue);
            continue;
        }
        if(key.FindF(_L8("p"))==0){//http request parameters
            HBufC8* hbuf=value.Alloc();
            hbuf->Des().Trim();
            flowPage->Parameters->AppendL(hbuf);
            continue;
        }
        if(key.FindF(_L8("c"))==0){//If Cookie enabled
            flowPage->PassCookie=(value.FindF(_L8("y"))==0);
            continue;
        }
        if(key.FindF(_L8("a"))==0){//If agent needed
            flowPage->NeedAgent=(value.FindF(_L8("y"))==0);
            continue;
        }
        if(key.FindF(_L8("s"))==0){//sleep before next stage.
            TBuf8<10> buf;
            buf.Copy(value);
            flowPage->SleepSeconds=TDesC82TInt(buf);
            continue;
        }

        if(key.FindF(_L8("rf"))==0){//http request bytes range from
            TBuf8<10> buf;
            buf.Copy(value);
            flowPage->RangeFrom=TDesC82TInt(buf);
            continue;
        }
        if(key.FindF(_L8("rt"))==0){//http request bytes range to
            TBuf8<10> buf;
            buf.Copy(value);
            flowPage->RangeTo=TDesC82TInt(buf);
            continue;
        }
    }
    CleanupStack::PopAndDestroy(lines);
}

CFlowPageManager* CFlowPageManager::NewLC(){
    CFlowPageManager* self=new (ELeave) CFlowPageManager();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CFlowPageManager* CFlowPageManager::NewL(){
    CFlowPageManager* self=CFlowPageManager::NewLC();
    CleanupStack::Pop(); // self;
    return self;
}

void CFlowPageManager::ConstructL(){
    this->iVarialbeArray=new (ELeave) CArrayPtrSeg<KeyValue> (5);
    this->iFlowPageArray=new (ELeave) CArrayPtrSeg<FlowPage> (5);
    iTimer=CTimeOutTimer::NewL(0,*this);
}
