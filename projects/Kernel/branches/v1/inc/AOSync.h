#ifndef __AOSYNC_H
#define __AOSYNC_H

#include <e32base.h>
#include <e32def.h>

class CAOSync:public CActive{
public:
  static CAOSync* NewL();
  virtual ~CAOSync();
  void Execute();
protected:
  void ConstructL();
  void DoCancel();
  void RunL();
private:
  CAOSync();
  CActiveSchedulerWait* iWait;
};

#endif
