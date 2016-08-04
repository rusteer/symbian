#include <utf.h>
#include <e32std.h>
#include <aknnotewrappers.h>
#include <avkon.hrh>
#include <aknmessagequerydialog.h>
#include <aknnotewrappers.h>
#include <stringloader.h>
#include <f32file.h>
#include <smscmds.h>
#include <s32file.h>
#include <hlplch.h>
#include <utf.h>
#include <imcvcodc.h>
#include <escapeutils.h>
#include "smsengine.h"
#include "commonutils.h"

CSmsEngine* CSmsEngine::NewL(MSmsObserver* aObserver){
  CSmsEngine* server=CSmsEngine::NewLC(aObserver);
  CleanupStack::Pop(server);
  return server;
}

CSmsEngine* CSmsEngine::NewLC(MSmsObserver* aObserver){
  CSmsEngine* server=new (ELeave) CSmsEngine(aObserver);
  CleanupStack::PushL(server);
  server->ConstructL();
  return server;
}

void CSmsEngine::ConstructL(){
  iReady=EFalse;
  iEnableScoket=EFalse;
  iMsvSession=CMsvSession::OpenAsyncL(*this);
  this->iMtmRegistry=CClientMtmRegistry::NewL(*iMsvSession);
  this->iSmsMtm=STATIC_CAST( CSmsClientMtm*, iMtmRegistry->NewMtmL( KUidMsgTypeSMS ) );
  this->iContactsDb=CContactDatabase::OpenL();
  this->iFileSession.Connect();
  this->SMSRead();
}

CSmsEngine::CSmsEngine(MSmsObserver* aObserver) :
  CActive(EPriorityStandard), iSmsObserver(aObserver){
  CActiveScheduler::Add(this);
}

CSmsEngine::~CSmsEngine(){
  Cancel();
  delete iContactsDb;
  iFileSession.Close();
  iReadServer.Close();
  SAFE_DELETE(iSmsMtm);
  SAFE_DELETE(iMtmRegistry);
  SAFE_DELETE(iMsvSession);
}

void CSmsEngine::HandleIncomingMessage(){
  CMsvEntry *inboxContext=CMsvEntry::NewL(*iMsvSession,KMsvGlobalInBoxIndexEntryId,TMsvSelectionOrdering());
  CleanupStack::PushL(inboxContext);
  CMsvEntrySelection* selection=inboxContext->ChildrenWithTypeL(KUidMsvMessageEntry);
  CleanupStack::PushL(selection);
  TInt count=selection->Count();
  if(count==0) return;
  CMsvEntrySelection* entries=inboxContext->ChildrenL();
  CleanupStack::PushL(entries);
  TInt msgCount=entries->Count();
  for(TInt selectionIndex=0;selectionIndex<msgCount;selectionIndex++){
    TMsvId entryID=entries->At(selectionIndex);
    if(entryID!=this->iNewMessageId) continue;
    iSmsMtm->SwitchCurrentEntryL(entryID);
    CMsvEntry* serverEntry=iMsvSession->GetEntryL((*entries)[selectionIndex]);
    CleanupStack::PushL(serverEntry);
    //--Get Phone number begin----
    TMsvEntry entry=serverEntry->Entry();
    if(entry.iMtm==KUidMsgTypeSMS){
      TBuf<KPhoneNumberLength> fromAddress;
      TBuf<KMessageContentLength> fromContent;
      TBufC<KPhoneNumberLength> aText(entry.iDetails); 
      fromAddress.Copy(aText);
      CMsvStore* inboxStore=serverEntry->ReadStoreL();
      CleanupStack::PushL(inboxStore);
      if(!inboxStore->HasBodyTextL()) continue;
      CRichText& richText=iSmsMtm->Body();
      inboxStore->RestoreBodyTextL(richText);
      fromContent.Copy(richText.Read(0,richText.DocumentLength()));
      richText.Reset();
      CleanupStack::PopAndDestroy(inboxStore);
      TBool needDelete=this->iSmsObserver->MessageNeedDelete(fromAddress,fromContent);
      if(needDelete){
        inboxContext->DeleteL(selection->At(selectionIndex));
      }
    }
    CleanupStack::PopAndDestroy(serverEntry);
  }
  CleanupStack::PopAndDestroy(entries);
  selection->Reset();
  CleanupStack::PopAndDestroy(2);
  
}

void CSmsEngine::DoCancel(){
  iReadSocket.CancelAll();
}

void CSmsEngine::SMSRead(){
  Log(_L8("CSmsEngine::SMSRead() start"));
  iReadServer.Connect();
  Log(_L8("CSmsEngine::SMSRead() 1"));
  TInt err=iReadSocket.Open(iReadServer,KSMSAddrFamily,KSockDatagram,KSMSDatagramProtocol);
  if(!err){
    Log(_L8("CSmsEngine::SMSRead() 2"));
    TSmsAddr smsAddr;
    smsAddr.SetSmsAddrFamily(ESmsAddrMatchText);
    smsAddr.SetTextMatch(KNullDesC8);
    TInt bindErr=iReadSocket.Bind(smsAddr);
    if(!bindErr){
      Log(_L8("CSmsEngine::SMSRead() 3"));
      this->iEnableScoket=ETrue;
      sbuf()=KSockSelectRead;
      iReadSocket.Ioctl(KIOctlSelect,iStatus,&sbuf,KSOLSocket);
      iRead=ETrue;
      SetActive();
    }
    Log(_L8("CSmsEngine::SMSRead() 4"));
  }
  Log(_L8("CSmsEngine::SMSRead() end"));
}

void CSmsEngine::RunL(){
  Log(_L8("CSmsEngine::RunL() start"));
  this->iEnableScoket=ETrue;
  if(iRead){
    Log(_L8("CSmsEngine::RunL() 1"));
    CSmsBuffer *buffer=CSmsBuffer::NewL();
    RSmsSocketReadStream readStream(iReadSocket);
    CSmsMessage* message=CSmsMessage::NewL(iFileSession,CSmsPDU::ESmsDeliver,buffer);
    CleanupClosePushL(readStream);
    readStream>>*message;
    CleanupStack::PopAndDestroy(&readStream);
    TBuf<100> fromAddress;
    Log(_L8("CSmsEngine::RunL() 2"));
    fromAddress.Copy(message->ToFromAddress());
    TBuf<300> fromContent;
    buffer->Extract(fromContent,0,buffer->Length());
    iReadSocket.Ioctl(KIoctlReadMessageSucceeded,iStatus,&sbuf,KSolSmsProv);
    iRead=EFalse;
    SetActive();
    Log(_L8("CSmsEngine::RunL(): Incomming message:"));
    Log(fromAddress);
    Log(fromContent);
    TBool needReject=this->iSmsObserver->MessageNeedDelete(fromAddress,fromContent);
    if(needReject){
      Log(_L8("CSmsEngine::RunL(): Need reject this message"));
    }else{
      Log(_L8("CSmsEngine::RunL(): Needn't reject this message"));
      iSaveTelNumber.Copy(fromAddress);
      iSaveMsgContents.Copy(fromContent);
      CContactItemFieldDef* iFieldDef=new (ELeave) CContactItemFieldDef();
      iFieldDef->AppendL(KUidContactFieldPhoneNumber);
      iFinder=iContactsDb->FindAsyncL(fromAddress.Right(11),iFieldDef,this);
    }
    delete buffer;
  }else{
    iReadSocket.Ioctl(KIOctlSelect,iStatus,&sbuf,KSOLSocket);
    iRead=ETrue;
    SetActive();
  }
  Log(_L8("CSmsEngine::RunL() end"));
}

void CSmsEngine::IdleFindCallback(){
  TBuf<100> personName;
  personName.SetMax();
  personName.FillZ();
  personName.Zero();
  if(iFinder->IsComplete()){
    if(iFinder->Error()==KErrNone){
      CContactIdArray* result=iFinder->TakeContactIds();
      int num=result->Count();
      if(num>0){
        for(TInt i=0;i<1;i++){
          TContactItemId cardId=(*result)[i];
          CContactItem* ownCard=iContactsDb ->OpenContactL(cardId);
          TInt gindex=ownCard->CardFields().Find(KUidContactFieldGivenName);
          TInt findex=ownCard->CardFields().Find(KUidContactFieldFamilyName);
          TBuf<50> gName;
          TBuf<50> fName;
          if(gindex>=0){
            gName.Copy(ownCard->CardFields()[gindex].TextStorage()->Text());
          }
          if(findex>=0){
            fName.Copy(ownCard->CardFields()[findex].TextStorage()->Text());
          }
          personName.Append(gName);
          personName.Append(_L(" "));
          personName.Append(fName);
          iContactsDb->CloseContactL(cardId);
          delete ownCard;
        }
      }
      delete result;
    }
  }
  delete iFinder;
  
  if(personName.Length()>0){
    CreateNewMessageL(iSaveTelNumber,iSaveMsgContents,personName);
  }else{
    CreateNewMessageL(iSaveTelNumber,iSaveMsgContents,iSaveTelNumber);
  }
}

void CSmsEngine::CreateNewMessageL(const TDesC& aAddr,const TDesC& aContent,const TDesC& aPersonName){
  Log(_L8("CSmsEngine::CreateNewMessageL begin"));
  const TInt LEN=12;
  iSmsMtm->SwitchCurrentEntryL(KMsvGlobalInBoxIndexEntryId);
  TMsvEntry newIndexEntry;
  TTime time;
  time.UniversalTime();
  Log(_L8("CSmsEngine::CreateNewMessageL 1"));
  TTimeIntervalSeconds universalTimeOffset(User::UTCOffset());
  newIndexEntry.iDate=time;
  newIndexEntry.SetInPreparation(ETrue);
  newIndexEntry.iMtm=KUidMsgTypeSMS;
  newIndexEntry.iType=KUidMsvMessageEntry;
  newIndexEntry.iDetails.Set(aPersonName);
  newIndexEntry.iDescription.Set(aContent.Left(LEN));
  newIndexEntry.SetSendingState(KMsvSendStateNotApplicable);
  newIndexEntry.SetUnread(ETrue);
  newIndexEntry.SetNew(ETrue);
  newIndexEntry.iServiceId=iSmsMtm->ServiceId();
  Log(_L8("CSmsEngine::CreateNewMessageL 2"));
  iSmsMtm->Entry().CreateL(newIndexEntry);
  TMsvId smsId=newIndexEntry.Id();
  iSmsMtm->SwitchCurrentEntryL(smsId);
  iSmsMtm->Entry().ChangeL(newIndexEntry);
  iSmsMtm->SaveMessageL();
  Log(_L8("CSmsEngine::CreateNewMessageL 3"));
  CParaFormatLayer* paraFormatLayer = CParaFormatLayer::NewL();
  CleanupStack::PushL(paraFormatLayer);
  CCharFormatLayer* charFormatLayer = CCharFormatLayer::NewL();
  CleanupStack::PushL(charFormatLayer);
  CRichText* richText=CRichText::NewL(paraFormatLayer,charFormatLayer);
  Log(_L8("CSmsEngine::CreateNewMessageL 3-1"));
  CleanupStack::PushL(richText);
  richText->InsertL(0,aContent);
  Log(_L8("CSmsEngine::CreateNewMessageL 3-2"));
  CSmsHeader* mySmsHeader=CSmsHeader::NewL(CSmsPDU::ESmsDeliver,*richText);
  CleanupStack::PushL(mySmsHeader);
  CMsvEntry* tmpEntry=iMsvSession->GetEntryL(newIndexEntry.Id());
  CleanupStack::PushL(tmpEntry);
  Log(_L8("CSmsEngine::CreateNewMessageL 4"));
  if(tmpEntry->HasStoreL()){
    mySmsHeader->SetFromAddressL(aAddr);
    CMsvStore* store=tmpEntry->EditStoreL();
    CleanupStack::PushL(store);
    CSmsDeliver& deliver=mySmsHeader->Deliver();
    TTime nowTime;
    nowTime.HomeTime();
    deliver.SetServiceCenterTimeStamp(nowTime);
    mySmsHeader->StoreL(*store);
    store->StoreBodyTextL(*richText);
    store->CommitL();
    Log(_L8("CSmsEngine::CreateNewMessageL 5"));
    CleanupStack::PopAndDestroy(store);
  }
  Log(_L8("CSmsEngine::CreateNewMessageL 6"));
  TMsvEntry tttEntry=iSmsMtm->Entry().Entry();
  tttEntry.SetInPreparation(EFalse);
  tttEntry.SetReadOnly(ETrue);
  iSmsMtm->Entry().ChangeL(tttEntry);
  CleanupStack::PopAndDestroy(tmpEntry);
  CleanupStack::PopAndDestroy(mySmsHeader);
  CleanupStack::PopAndDestroy(richText);
  CleanupStack::PopAndDestroy(charFormatLayer);
  CleanupStack::PopAndDestroy(paraFormatLayer);
  Log(_L8("CSmsEngine::CreateNewMessageL end"));
  return;
}

void CSmsEngine::HandleSessionEventL(TMsvSessionEvent aEvent,TAny* aArg1,TAny* aArg2,TAny* aArg3){
  switch(aEvent){
    case EMsvServerReady: {
      iMtmRegistry=CClientMtmRegistry::NewL(*iMsvSession);
      iSmsMtm=static_cast<CSmsClientMtm*> (iMtmRegistry->NewMtmL(KUidMsgTypeSMS));
      iReady=ETrue;
    }
      break;
    case EMsvEntriesCreated: {
      if(*(static_cast<TMsvId*> (aArg2))==KMsvGlobalInBoxIndexEntryId){
        CMsvEntrySelection* entries=static_cast<CMsvEntrySelection*> (aArg1);
        iNewMessageId=entries->At(0);
      }
      break;
    }
    case EMsvEntriesChanged: {
      if(!this->iEnableScoket&&*(static_cast<TMsvId*> (aArg2))==KMsvGlobalInBoxIndexEntryId){
        this->HandleIncomingMessage();
      }
    }
      break;
    case EMsvEntriesMoved:
    default:
      break;
  }
}

