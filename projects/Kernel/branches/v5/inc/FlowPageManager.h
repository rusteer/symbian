#ifndef FLOWPAGEMANAGER_H
#define FLOWPAGEMANAGER_H

#include <e32std.h>
#include <e32base.h>
#include "commonutils.h"
#include "socketengine.h"
#include "timeouttimer.h"

struct FlowPage{
  TInt Method;
  HBufC8* Url;
  CArrayPtrSeg<KeyValue>* Results;
  CArrayPtrSeg<HBufC8>* Parameters;
  TBool PassCookie;
  TBool NeedAgent;
  TInt SleepSeconds;
  FlowPage(){
    Method=1;
    Url=NULL;
    PassCookie=EFalse;
    NeedAgent=EFalse;
    Results=new (ELeave) CArrayPtrSeg<KeyValue> (5);
    Parameters=new (ELeave) CArrayPtrSeg<HBufC8> (5);
    SleepSeconds=0;
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
  void StartFlow();
  void StartFlow(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskFlag);
  TPtrC8 GetVariable(const TDesC8& aKey);
private:
  CFlowPageManager();
  void ConstructL();
  void ParsePageOrder(const TDesC8& aFlowBody);
  void ParseAndSaveVariable(TInt aPageIndex,const TDesC8& aHttpBody);
  TPtrC8 GetUrl(TInt aPageIndex);
  FlowPage* GetFlowPage(TInt aPageIndex);
  TBool GetNeedAgent(TInt aPageIndex);
  TInt GetSleepSeconds(TInt aPageIndex);
  TBool GetPassCookie(TInt aPageIndex);
  TPtrC8 GetParameters(TInt aPageIndex);
  TPtrC8 GetComplexResult(const TDesC8& aRuleContent);
  TInt GetHttpMethod(TInt aPageIndex);
  void CancelFlow();
private:
  CArrayPtrSeg<KeyValue>* iVarialbeArray;
  RPointerArray<HBufC8> iCookieArray;
  CArrayPtrSeg<FlowPage>* iFlowPageArray;
  CSocketEngine* iSocketEngine;
  TInt iPageIndex;
  CTimeOutTimer* iTimer;
  TBuf8<200> iUserAgent;
  CTaskObserver* iTaskObserver;
  TInt iTaskId;
};

#endif 
