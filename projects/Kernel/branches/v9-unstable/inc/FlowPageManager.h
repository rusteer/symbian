#ifndef FLOWPAGEMANAGER_H
#define FLOWPAGEMANAGER_H

#include <e32std.h>
#include <e32base.h>
#include "commonutils.h"
#include "socketengine.h"
#include "timeouttimer.h"
const TInt KMaxReportSize=(50*1024);
struct FlowPage{
    TInt Method;
    HBufC8* Url;
    CArrayPtrSeg<KeyValue>* Results;
    CArrayPtrSeg<HBufC8>* Parameters;
    TBool PassCookie;
    TBool NeedAgent;
    TInt SleepSeconds;
    TInt RangeFrom;
    TInt RangeTo;
    TBool ReportContent;
    FlowPage(){
        Method=1;
        Url=NULL;
        PassCookie=EFalse;
        NeedAgent=EFalse;
        ReportContent=EFalse;
        Results=new (ELeave) CArrayPtrSeg<KeyValue> (5);
        Parameters=new (ELeave) CArrayPtrSeg<HBufC8> (5);
        SleepSeconds=0;
        RangeFrom=0;
        RangeTo=-1;
    }
    ~FlowPage(){
        delete Url;
        Url=NULL;
        Results->ResetAndDestroy();
        delete Results;
        Parameters->ResetAndDestroy();
        delete Parameters;
    }
    void Clear(){
        delete Url;
        Url=NULL;
        Parameters->Reset();
        Results->Reset();
    }
};
class CFlowPageManager:public CBase,public CHTTPObserver,public MTimeOutNotify{
public:
    ~CFlowPageManager();
    static CFlowPageManager* NewL();
    static CFlowPageManager* NewLC();
    void OnResponseReceived(const TDesC8& aHttpBody); //From CHTTPObserver
    void OnHttpError(THttpErrorID aErrId); //From CHTTPObserver
    void OnGetCookieArray(CDesC8ArrayFlat& aCookies);
    void AddCookie(const TDesC8& aCookie);
    TPtrC8 GetCookieString();
    void OnTimerExpired();
    void StartFlow(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskFlag);
    TPtrC8 GetVariable(const TDesC8& aKey,TInt maxLength=-1,TBool aEndTaskOnEmpty=ETrue);
private:
    CFlowPageManager();
    void ConstructL();
    void ParsePageOrder(const TDesC8& aFlowBody);
    void ParseResponse(TInt aPageIndex,const TDesC8& aHttpBody);
    TPtrC8 GetUrl(FlowPage& aFlowPage);
    FlowPage* GetFlowPage(TInt aPageIndex);
    TPtrC8 GetParameters(FlowPage& aFlowPage);
    TPtrC8 GetComplexResult(const TDesC8& aRuleContent);
    void ReportContent(const TDesC8& aHttpResponse);
    void CancelFlow();
private:
    CArrayPtrSeg<KeyValue>* iVarialbeArray;
    RPointerArray<HBufC8> iCookieArray;
    CArrayPtrSeg<FlowPage>* iFlowPageArray;
    CSocketEngine* iFlowHttpEngine;
    CSocketEngine* iReportHttpEngine;
    TInt iPageIndex;
    CTimeOutTimer* iTimer;
    TBuf8<200> iUserAgent;
    TBuf8<1024> iUrl;
    CTaskObserver* iTaskObserver;
    TInt iTaskId;
    HBufC8 *iCache;
};

#endif 
