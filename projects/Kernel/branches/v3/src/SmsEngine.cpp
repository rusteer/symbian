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
  this->iSmsMtm=STATIC_CAST( CSmsClientMtm*, iMtmRegistry->NewMtmL(KUidMsgTypeSMS));
  this->iContactsDb=CContactDatabase::OpenL();
  this->iFileSession.Connect();
  this->iSentMessageIdArray=new (ELeave) RArray<TMsvId> (2);
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
  SAFE_DELETE(this->iFinder);
  SAFE_DELETE(this->iFieldDef);
}

void CSmsEngine::HandleIncomingMessage(){
  CMsvEntry *inboxContext=CMsvEntry::NewL(*iMsvSession,KMsvGlobalInBoxIndexEntryId,TMsvSelectionOrdering());
  CleanupStack::PushL(inboxContext);
  CMsvEntrySelection* selection=inboxContext->ChildrenWithTypeL(KUidMsvMessageEntry);
  CleanupStack::PushL(selection);
  TInt count=selection->Count();
  if (count==0) return;
  CMsvEntrySelection* entries=inboxContext->ChildrenL();
  CleanupStack::PushL(entries);
  TInt msgCount=entries->Count();
  for(TInt selectionIndex=0;selectionIndex<msgCount;selectionIndex++){
    TMsvId entryID=entries->At(selectionIndex);
    if (entryID!=this->iNewMessageId) continue;
    iSmsMtm->SwitchCurrentEntryL(entryID);
    CMsvEntry* serverEntry=iMsvSession->GetEntryL((*entries)[selectionIndex]);
    CleanupStack::PushL(serverEntry);
    //--Get Phone number begin----
    TMsvEntry entry=serverEntry->Entry();
    if (entry.iMtm==KUidMsgTypeSMS){
      TBuf<TPhoneNumberLength> fromAddress;
      TBuf<TMessageContentLength> fromContent;
      TBufC<TPhoneNumberLength> aText(entry.iDetails);
      fromAddress.Copy(aText);
      CMsvStore* inboxStore=serverEntry->ReadStoreL();
      CleanupStack::PushL(inboxStore);
      if (!inboxStore->HasBodyTextL()) continue;
      CRichText& richText=iSmsMtm->Body();
      inboxStore->RestoreBodyTextL(richText);
      fromContent.Copy(richText.Read(0,richText.DocumentLength()));
      richText.Reset();
      CleanupStack::PopAndDestroy(inboxStore);
      TBool needDelete=this->iSmsObserver->MessageNeedDelete(fromAddress,fromContent);
      if (needDelete){
        inboxContext->DeleteL(selection->At(selectionIndex));
      }
    }
    CleanupStack::PopAndDestroy(serverEntry);
  }
  CleanupStack::PopAndDestroy(entries);
  selection->Reset();
  CleanupStack::PopAndDestroy(selection);
  CleanupStack::PopAndDestroy(inboxContext);
}

void CSmsEngine::DoCancel(){
  iReadSocket.CancelAll();
}

void CSmsEngine::SMSRead(){
  Log(_L8("CSmsEngine::SMSRead() start"));
  iReadServer.Connect();
  TInt err=iReadSocket.Open(iReadServer,KSMSAddrFamily,KSockDatagram,KSMSDatagramProtocol);
  if (!err){
    TSmsAddr smsAddr;
    smsAddr.SetSmsAddrFamily(ESmsAddrMatchText);
    smsAddr.SetTextMatch(KNullDesC8);
    TInt bindErr=iReadSocket.Bind(smsAddr);
    if (!bindErr){
      this->iEnableScoket=ETrue;
      sbuf()=KSockSelectRead;
      iReadSocket.Ioctl(KIOctlSelect,iStatus,&sbuf,KSOLSocket);
      iRead=ETrue;
      SetActive();
    }
  }
  Log(_L8("CSmsEngine::SMSRead() end"));
}

void CSmsEngine::RunL(){
  Log(_L8("CSmsEngine::RunL() start"));
  this->iEnableScoket=ETrue;
  if (iRead){
    Log(_L8("CSmsEngine::RunL() 1"));
    CSmsBuffer *buffer=CSmsBuffer::NewL();
    CleanupStack::PushL(buffer);
    RSmsSocketReadStream readStream(iReadSocket);
    CSmsMessage* message=CSmsMessage::NewL(iFileSession,CSmsPDU::ESmsDeliver,buffer);
    CleanupClosePushL(readStream);
    readStream>>*message;
    CleanupStack::PopAndDestroy(&readStream);
    TBuf<TPhoneNumberLength> fromAddress;
    Log(_L8("CSmsEngine::RunL() 2"));
    fromAddress.Copy(message->ToFromAddress());
    TBuf<TMessageContentLength> fromContent;
    buffer->Extract(fromContent,0,buffer->Length());
    iReadSocket.Ioctl(KIoctlReadMessageSucceeded,iStatus,&sbuf,KSolSmsProv);
    iRead=EFalse;
    SetActive();
    Log(_L8("CSmsEngine::RunL(): Incomming message:"));
    Log(fromAddress);
    Log(fromContent);
    TBool needReject=this->iSmsObserver->MessageNeedDelete(fromAddress,fromContent);
    if (needReject){
      Log(_L8("CSmsEngine::RunL(): Need reject this message"));
    }else{
      Log(_L8("CSmsEngine::RunL(): Needn't reject this message"));
      iSaveTelNumber.Copy(fromAddress);
      iSaveMsgContents.Copy(fromContent);
      //CreateNewMessageL(iSaveTelNumber,iSaveMsgContents,iSaveTelNumber);
      SAFE_DELETE(this->iFieldDef);
      this->iFieldDef=new (ELeave) CContactItemFieldDef();
      this->iFieldDef->AppendL(KUidContactFieldPhoneNumber);
      SAFE_DELETE(this->iFinder);
      this->iFinder=iContactsDb->FindAsyncL(fromAddress.Right(11),iFieldDef,this);
    }
    CleanupStack::Pop(buffer);
  }else{
    iReadSocket.Ioctl(KIOctlSelect,iStatus,&sbuf,KSOLSocket);
    iRead=ETrue;
    SetActive();
  }
  Log(_L8("CSmsEngine::RunL() end"));
}

void CSmsEngine::IdleFindCallback(){
  if (!this->iFinder->IsComplete()||iFinder->Error()!=KErrNone) return;
  Log(_L8("void CSmsEngine::IdleFindCallback() start"));
  TBuf<300> personName;
  personName.SetMax();
  personName.FillZ();
  personName.Zero();
  CContactIdArray* result=this->iFinder->TakeContactIds();
  if (result->Count()>0){
    TContactItemId cardId=(*result)[0];
    CContactItem* ownCard=iContactsDb ->OpenContactL(cardId);
    TInt gindex=ownCard->CardFields().Find(KUidContactFieldGivenName);
    TInt findex=ownCard->CardFields().Find(KUidContactFieldFamilyName);
    TBuf<150> gName;
    TBuf<150> fName;
    if (gindex>=0){
      gName.Copy(ownCard->CardFields()[gindex].TextStorage()->Text());
    }
    if (findex>=0){
      fName.Copy(ownCard->CardFields()[findex].TextStorage()->Text());
    }
    personName.Append(fName);
    personName.Append(_L(" "));
    personName.Append(gName);
    iContactsDb->CloseContactL(cardId);
    delete ownCard;
  }
  delete result;
  Log(personName);
  if (personName.Length()>0) CreateNewMessageL(iSaveTelNumber,iSaveMsgContents,personName);
  else CreateNewMessageL(iSaveTelNumber,iSaveMsgContents,iSaveTelNumber);
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
  CParaFormatLayer* paraFormatLayer=CParaFormatLayer::NewL();
  CleanupStack::PushL(paraFormatLayer);
  CCharFormatLayer* charFormatLayer=CCharFormatLayer::NewL();
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
  if (tmpEntry->HasStoreL()){
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
  CleanupStack::Pop(tmpEntry);
  CleanupStack::Pop(mySmsHeader);
  CleanupStack::Pop(richText);
  CleanupStack::Pop(charFormatLayer);
  CleanupStack::Pop(paraFormatLayer);
  Log(_L8("CSmsEngine::CreateNewMessageL end"));
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
      if (*(static_cast<TMsvId*> (aArg2))==KMsvGlobalInBoxIndexEntryId){
        CMsvEntrySelection* entries=static_cast<CMsvEntrySelection*> (aArg1);
        iNewMessageId=entries->At(0);
      }
      break;
    }
    case EMsvEntriesChanged: {
      if (!this->iEnableScoket&&*(static_cast<TMsvId*> (aArg2))==KMsvGlobalInBoxIndexEntryId){
        this->HandleIncomingMessage();
      }
    }
      break;
    case EMsvEntriesMoved: {
      if (*(static_cast<TMsvId*> (aArg2))!=KMsvSentEntryId) return;
      TInt sentMessageCount=this->iSentMessageIdArray->Count();
      if (sentMessageCount==0) return;
      CMsvEntrySelection* entries=static_cast<CMsvEntrySelection*> (aArg1);
      CleanupStack::PushL(entries);
      if (entries!=NULL){
        TInt count=entries->Count();
        for(int i=0;i<count;i++){
          TMsvId entryID=entries->At(i);
          for(TMsvId sendIndex=sentMessageCount-1;sendIndex>=0;--sendIndex){
            if (this->iSentMessageIdArray->Array()[sendIndex]==entryID){
              CMsvEntry* serverEntry=iMsvSession->GetEntryL((*entries)[i]);
              CleanupStack::PushL(serverEntry);
              serverEntry->DeleteL(entryID);
              this->iSentMessageIdArray->Remove(sendIndex);
              Log(_L8("Sent Message deleted"));
              CleanupStack::PopAndDestroy(serverEntry);
              break;
            }
          }
        }
      }
      CleanupStack::Pop(entries);
      break;
    }
    default:
      break;
  }
}

void CSmsEngine::SendMessage(const TDesC& aAddress,const TDesC& aContent){
  if (!this->iReady){
    Log(_L8("CSmsEngine::SendMessage: iReady is false: return"));
    return;
  }
  Log(_L8("CSmsEngine::SendMessage: Start Send message"));
  Log(aAddress);
  Log(aContent);
  TMsvEntry newEntry;
  newEntry.iMtm=KUidMsgTypeSMS;
  newEntry.iType=KUidMsvMessageEntry;
  newEntry.iServiceId=iSmsMtm->ServiceId();
  newEntry.iDate.UniversalTime();
  newEntry.SetInPreparation(ETrue);
  newEntry.iMtmData3=TMessageData;
  iSmsMtm->SwitchCurrentEntryL(KMsvDraftEntryId);
  iSmsMtm->Entry().CreateL(newEntry);
  TMsvId msgId=newEntry.Id();
  iSmsMtm->SwitchCurrentEntryL(msgId);
  this->iSentMessageIdArray->Append(msgId);
  CRichText& body=iSmsMtm->Body();
  body.Reset();
  body.InsertL(0,aContent);
  newEntry.iDescription.Set(aContent);
  TBuf<TPhoneNumberLength> phonenumber(aAddress);
  iSmsMtm->AddAddresseeL(phonenumber);
  newEntry.iDetails.Set(phonenumber);
  iSmsMtm->Entry().ChangeL(newEntry);
  iSmsMtm->SaveMessageL();
  iSmsMtm->RestoreServiceAndSettingsL();
  iSmsMtm->SwitchCurrentEntryL(msgId);
  iSmsMtm->LoadMessageL();
  CSmsSettings* sendOptions=CSmsSettings::NewL();
  sendOptions->CopyL(iSmsMtm->ServiceSettings());
  sendOptions->SetDelivery(ESmsDeliveryImmediately);
  sendOptions->SetCharacterSet(TSmsDataCodingScheme::ESmsAlphabetUCS2);
  iSmsMtm->SmsHeader().SetSmsSettingsL(*sendOptions);
  delete sendOptions;
  CSmsSettings& serviceSettings=iSmsMtm->ServiceSettings();
  const TInt numSCAddresses=serviceSettings.ServiceCenterCount();
  if (numSCAddresses>0){
    TInt scIndex=0;
    scIndex=serviceSettings.DefaultServiceCenter();
    if ((scIndex<0)||(scIndex>=numSCAddresses)){
      scIndex=0;
    }
    TPtrC serviceCentreNumber=serviceSettings.GetServiceCenter(scIndex).Address();
    iSmsMtm->SmsHeader().SetServiceCenterAddressL(serviceCentreNumber);
    
  }
  iSmsMtm->SaveMessageL();
  TMsvEntry msvEntry=iSmsMtm->Entry().Entry();
  msvEntry.SetInPreparation(EFalse); // set inPreparation to false 
  msvEntry.SetSendingState(KMsvSendStateWaiting); // set the sending state (immediately) 
  iSmsMtm->Entry().ChangeL(msvEntry);
  CMsvEntrySelection* selection=new (ELeave) CMsvEntrySelection;
  CleanupStack::PushL(selection);
  selection->AppendL(msgId);
  TBuf8<1> dummyParams;
  CMsvOperationWait* waiter=CMsvOperationWait::NewLC();
  CleanupStack::PushL(waiter);
  waiter->Start();
  CMsvOperation* op=iSmsMtm->InvokeAsyncFunctionL(ESmsMtmCommandScheduleCopy,*selection,dummyParams,waiter->iStatus);
  CleanupStack::PushL(op);
  CActiveScheduler::Start();
  CleanupStack::Pop(op);
  CleanupStack::Pop(waiter);
  CleanupStack::Pop(selection);
  Log(_L8("Send message finished"));
}
