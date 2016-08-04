#ifndef __SMSENGINE__
#define __SMSENGINE__
#include <e32base.h>
#include <f32file.h>
#include <es_sock.h>
#include <smsuaddr.h>
#include <gsmubuf.h>
#include <smsustrm.h>
#include <gsmumsg.h>
#include <smsclnt.h>
#include <msvids.h>
#include <mtclreg.h>
#include <msvapi.h>
#include <txtrich.h>
#include <eikenv.h>
#include <gsmupdu.h>
#include <smuthdr.h>
#include <cntdb.h>
#include <cntfield.h>
#include <cntitem.h>
#include <cntfldst.h>

const TInt KPhoneNumberLength=32;
const TInt KMessageContentLength=1000;

struct Message{
  TBuf<KPhoneNumberLength> address;
  TBuf<KMessageContentLength> content;
};

class MSmsObserver{
public:
  virtual TBool MessageNeedDelete(const TDesC& number,const TDesC& content)=0;
};

class CSmsEngine:public CActive,public MMsvSessionObserver,public MIdleFindObserver{
public:
  static CSmsEngine* NewL(MSmsObserver* aObserver);
  static CSmsEngine* NewLC(MSmsObserver* aObserver);
  ~CSmsEngine();
public:
  virtual void DoCancel();
  void SMSRead();
private:
  CSmsEngine(MSmsObserver* aObserver);
  void ConstructL();
private:
  void CreateNewMessageL(const TDesC& aAddr,const TDesC& aContent,const TDesC& aPersonName);
  void RunL();
public:
  void HandleIncomingMessage();
private:
  void HandleSessionEventL(TMsvSessionEvent aEvent,TAny* aArg1,TAny* aArg2,TAny* aArg3);
  void IdleFindCallback();
private:
  RFs iFileSession;
  TBool iRead;
  RSocketServ iReadServer;
  RSocket iReadSocket;
  TPckgBuf<TUint> sbuf;
  CSmsClientMtm* iSmsMtm;
  CMsvSession* iMsvSession;
  TBuf<TGsmSmsTelNumberMaxLen> iSaveTelNumber;
  TBuf<255> iSaveMsgContents;
  CContactDatabase* iContactsDb;
  TBool iEnableScoket;
  CClientMtmRegistry *iMtmRegistry;
  TMsvId iNewMessageId;
  TBool iReady;
  CIdleFinder* iFinder;
  MSmsObserver* iSmsObserver;
};

#endif
