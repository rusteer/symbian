#include "timeouttimer.h"

CTimeOutTimer::CTimeOutTimer(const TInt aPriority) :
  CTimer(aPriority){
}

CTimeOutTimer::~CTimeOutTimer(){
  Cancel();
}

CTimeOutTimer* CTimeOutTimer::NewL(const TInt aPriority,MTimeOutNotify& aTimeOutNotify){
  CTimeOutTimer *p=new (ELeave) CTimeOutTimer(aPriority);
  CleanupStack::PushL(p);
  p->ConstructL(aTimeOutNotify);
  CleanupStack::Pop();
  return p;
}

void CTimeOutTimer::ConstructL(MTimeOutNotify& aTimeOutNotify){
  iNotify=&aTimeOutNotify;
  CTimer::ConstructL();
  CActiveScheduler::Add(this);
}

void CTimeOutTimer::RunL(){
  if(iIsMinutesTimer){
    iMinutesExpend++;
    if(iMinutesExpend>=iMinutesDelay){
      iNotify->OnMinutesTimeout(iTimerID);
    }else CTimer::After(60*1000000);
  }else iNotify->TimerExpired();
}

void CTimeOutTimer::After(TTimeIntervalMicroSeconds32 anInterval){
  Cancel();
  iIsMinutesTimer=EFalse;
  CTimer::After(anInterval);
}

void CTimeOutTimer::MinutesAfter(TInt aMinutes){
  if(aMinutes>0){
    Cancel();
    iMinutesDelay=aMinutes;
    iMinutesExpend=0;
    iIsMinutesTimer=ETrue;
    CTimer::After(60*1000000);
  }
}

