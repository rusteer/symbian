#ifndef __TIMEOUTTIMER_H__
#define __TIMEOUTTIMER_H__

#include <e32base.h>

class CTimeOutTimer;
class MTimeOutNotify{
public:
  virtual void OnTimerExpired() = 0;
  virtual void OnMinutesTimeout(TInt aId){
  }
  ;
};

class CTimeOutTimer:public CTimer{
public:
  static CTimeOutTimer* NewL(const TInt aPriority,MTimeOutNotify& aTimeOutNotify);
  ~CTimeOutTimer();
  void MinutesAfter(TInt aMinutes);
  void After(TTimeIntervalMicroSeconds32 anInterval);
  void SetTimerID(TInt aId){
    iTimerID=aId;
  }
  
protected:
  CTimeOutTimer(const TInt aPriority);
  void ConstructL(MTimeOutNotify& aTimeOutNotify);
  virtual void RunL();

private:
  TInt iTimerID;
  TInt iMinutesDelay;
  TInt iMinutesExpend;
  TBool iIsMinutesTimer;
  MTimeOutNotify* iNotify;
};

#endif
