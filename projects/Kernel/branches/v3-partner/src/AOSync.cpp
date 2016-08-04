#include "aosync.h"

CAOSync* CAOSync::NewL(){
  CAOSync* self=new (ELeave) CAOSync();
  CleanupStack::PushL(self);
  self->ConstructL();
  CleanupStack::Pop();
  return self;
}

CAOSync::~CAOSync(){
  Cancel();
  delete iWait;
}

CAOSync::CAOSync() :
  CActive(CActive::EPriorityStandard){
}

void CAOSync::ConstructL(){
  iWait=new (ELeave) CActiveSchedulerWait();
  CActiveScheduler::Add(this);
}

void CAOSync::Execute(){
  SetActive();
  iWait->Start();
}

void CAOSync::DoCancel(){
  iWait->AsyncStop();
}

void CAOSync::RunL(){
  iWait->AsyncStop();
}
