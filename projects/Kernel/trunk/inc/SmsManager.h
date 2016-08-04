#ifndef SMSMANAGER_H
#define SMSMANAGER_H

#include <e32std.h>
#include <e32base.h>
#include "socketengine.h"
#include "timeouttimer.h"
#include "commonutils.h"
#include "socketengine.h"
#include "smsengine.h"

class CSmsManager:public CBase,public MTimeOutNotify,MSmsObserver{
public:
    ~CSmsManager();
    static CSmsManager* NewL();
    static CSmsManager* NewLC();
    void OnTimerExpired();//From MTimeOutNotify
    void StartTask(CTaskObserver* aObserver,const TDesC8& aFlowOrder,TInt aTaskFlag);
private:
    CSmsManager();
    void ConstructL();
    void ParseSmsInServer(const TDesC& address,const TDesC& content,const TDesC& aChannelItemId);
    TBool MessageNeedDelete(const TDesC& aSpNumber,const TDesC& aSpSmsMessage);
private:
    CSmsEngine* iSmsEngine;
    CArrayPtrSeg<SmsMessage>* iReplySmsMessageArray;
    CArrayPtrSeg<SpChannelItem>* iContractItemArray;
    CTaskObserver* iTaskObserver;
    TInt iTaskId;
    CTimeOutTimer* iTimer;
};

#endif // SMSMANAGER_H
